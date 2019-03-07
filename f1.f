      implicit real*8 ( a-z )
      
      include "mpif.h"
      include "adlbf.h"

      integer, parameter ::  A_EPOCH     = 2,
     &                       CS_PER_B    = 8,
     &                       MASTER_RANK = 0

      integer, parameter ::  TAG_B_ANSWER = 1,
     &                       TAG_C_ANSWER = 2

      integer, parameter ::  TYPE_A = 1,
     &                       TYPE_B = 2,
     &                       TYPE_C = 3

      integer, parameter ::  NUM_TYPES = 3
      integer, parameter ::  TYPE_VECT(3) = (/1,2,3/)

      integer            ::  REQ_TYPES(4) = (/-1,-1,-1,-1/)

      integer            ::  work_handle(ADLB_HANDLE_SIZE)

      integer :: status(MPI_STATUS_SIZE)
      integer :: ierr, num_delay, num_time_units_per_A, num_servers,
     &           i, num_world_nodes, my_world_rank, num_As, num_Bs,
     &           am_server, app_comm, num_app_nodes, my_app_rank, sum,
     &           num_slaves, iv, j, time_unit_this_A, num_As_this_node,
     &           priority_A, priority_B, priority_C,
     &           work_type, work_prio, orig_rank,
     &           work_len, answer_rank, num_C_answers, msg_available,
     &           use_debug_server, am_debug_server, aprintf_flag

      real*8 :: work_A(20), work_B(10), work_C(20)
      real*8 :: malloc_hwm, avg_time_on_rq
      real*8 :: temp_max_malloc, temp_periodic
      real*8 :: debug_server_timeout

      character*64 chararg
      
      write(6,*) 'BEFORE HANDLING CMD_LINE ARGS'
      call flush(6)

      num_delay = 10000000
      num_time_units_per_A = A_EPOCH * 2
      num_servers = 1
      num_As = 4
      use_debug_server = 0
      aprintf_flag = 1

      ! do i = 1, iargc(), 1
      !    call getarg( i, chararg )
      !    if ( trim(chararg) == '-nunits' )  then
      !       call getarg( i+1, chararg )
      !       read(chararg,*) num_time_units_per_A
      !    elseif ( trim(chararg) == '-nas' )  then
      !       call getarg( i+1, chararg )
      !       read(chararg,*) num_As
      !    elseif ( trim(chararg) == '-nservers' )  then
      !       call getarg( i+1, chararg )
      !       read(chararg,*) num_servers
      !    endif
      ! enddo

      write(6,*) 'PAST HANDLING CMD_LINE ARGS'
      call flush(6)

      priority_A = 0
      priority_B = -2
      priority_C = -1

      call MPI_Init( ierr )
      if ( ierr /= MPI_SUCCESS ) then
          write(6,*) 'MPI_Init failed with rc=', ierr
      endif
      call MPI_COMM_RANK( MPI_COMM_WORLD, my_world_rank, ierr )
      call MPI_COMM_SIZE( MPI_COMM_WORLD, num_world_nodes, ierr )
      write (6,*) 'starting', my_world_rank, num_world_nodes
      call flush(6)

      call ADLB_INIT( num_servers, use_debug_server, aprintf_flag,
     &                NUM_TYPES, TYPE_VECT, am_server, am_debug_server,
     &                app_comm, ierr )
      write(6,*) 'num_servers and am_server', num_servers, am_server
      call flush(6)

      if ( am_server /= 1  .and.  am_debug_server /= 1 ) then
          call MPI_COMM_SIZE( app_comm, num_app_nodes, ierr )
          write(6,*) 'num_app_nodes', num_app_nodes
          call MPI_COMM_RANK( app_comm, my_app_rank, ierr )
          write(6,*) 'num_app_nodes and my_app_rank',
     &                num_app_nodes, my_app_rank
          call flush(6)
          num_slaves = num_app_nodes - 1
      endif

      call MPI_BARRIER( MPI_COMM_WORLD, ierr )
      start_time = MPI_WTIME()

      if ( am_server == 1 ) then
          temp_max_malloc = 3000000.0
          temp_periodic = 0.0
          call ADLB_SERVER( temp_max_malloc, temp_periodic, ierr )
          call ADLB_INFO_GET(ADLB_INFO_MALLOC_HWM,malloc_hwm,ierr);
          write(6,*) 'IERR=', ierr, 'MALLOC_HWM=', malloc_hwm
          call flush(6)
      elseif ( am_debug_server == 1 ) then
          debug_server_timeout = 300.0
          call ADLB_DEBUG_SERVER( debug_server_timeout, ierr )
      elseif (my_app_rank == MASTER_RANK) then
          sum = 0
          !!  one B per A per time_units_per_A/A_EPOCH
          num_Bs = num_As * (num_time_units_per_A / A_EPOCH)
          do i = 1, num_Bs, 1
              write(6,*) 'waiting for mpi_recv; i=', i
              call flush(6)
              call MPI_RECV(iv,1,MPI_INTEGER,MPI_ANY_SOURCE,MPI_ANY_TAG,
     &                      app_comm, status, ierr)
              write(6,*) 'past mpi_recv; tag=', status(MPI_TAG)
              call flush(6)
              if ( status(MPI_TAG) .eq.  TAG_B_ANSWER) then
                  sum = sum + iv
                  write (6,*) 'past mpi_recv; i iv sum', i, iv, sum
                  call flush(6)
              else    ! may need elseif here later for other msg types
                  write (6,*) '** invalid tag received', status(MPI_TAG)
                  call flush(6)
              call flush(6)
              endif
          enddo
          write (6,*) '****** SETTING NO MORE WORK ********'
          call flush(6)
          call ADLB_SET_PROBLEM_DONE( ierr )
          write (6,*) 'expected sum =',
     &                num_As*(num_time_units_per_A/A_EPOCH)*CS_PER_B
          write (6,*) 'done:    sum =', sum
          call flush(6)
      else  !! slave
          if ( num_As >= num_slaves ) then
              num_As_this_node = num_As / num_slaves
              j = num_As - (num_As_this_node * num_slaves)
              if ( j > 0 ) then
                  if ( my_app_rank <= j ) then
                      num_As_this_node = num_As_this_node + 1
                  endif
              endif
          else
              if ( my_app_rank >= 1  .and.  my_app_rank <= num_As ) then
                  num_As_this_node = 1
              else
                  num_As_this_node = 0
              endif
          endif
          write (6,*) 'NUM_AS_THIS_NODE', num_As_this_node
          call flush(6)
          call ADLB_BEGIN_BATCH_PUT( NULL, 0, ierr )
          do i = 1, num_As_this_node, 1
              work_A(1) = my_world_rank
              work_A(2) = i
              work_A(3) = 1.0  ! starting time unit for this A
              write (6,*) 'PT0'
              call flush(6)
              call ADLB_PUT( work_A, 20*8, -1, my_app_rank,
     &                       TYPE_A, priority_A, ierr)
              write (6,*) 'PUT A VAL', work_A(1), work_A(2)
              call flush(6)
          enddo
          call ADLB_END_BATCH_PUT( ierr )
          do while ( .true. )
             write (6,*) 'DOING RESERVE'
             call flush(6)
             REQ_TYPES(1) = -1
             call ADLB_RESERVE( REQ_TYPES,
     &                          work_type, work_prio, work_handle,
     &                          work_len, answer_rank, ierr )
             write (6,*) 'RESERVE IERR', ierr
             call flush(6)
             if ( ierr == ADLB_NO_MORE_WORK ) then
                 write (6,*) 'GOT NO_MORE_WORK x1'
                 call flush(6)
                 go to 10  !! fake break
             endif
             if ( ierr == 0 ) then
                 write (6,*) '******** RESERVE FAILED ********'
                 call flush(6)
                 exit
             endif
             if ( work_type == TYPE_A ) then
                 call ADLB_GET_RESERVED( work_A, work_handle, ierr )
                 if ( ierr == ADLB_NO_MORE_WORK ) then
                     write (6,*) 'GOT NO_MORE_WORK x2'
                     call flush(6)
                     go to 10  !! fake break
                 endif
                 tempval = work_A(1)
                 do i = 1, num_delay, 1
                     tempval = sqrt(tempval + 5000000.0) + 1
                 enddo
                 time_unit_this_A = work_A(3) + 1
                 write (6,*) 'HANDLING A VAL', work_A(1), work_A(2)
                 call flush(6)
                 if ( (mod(time_unit_this_A,A_EPOCH)) == 0  .and.
     &                 time_unit_this_A <= num_time_units_per_A ) then
                     work_B(1) = work_A(1)
                     work_B(2) = work_A(2)
                     call ADLB_PUT( work_B, 10*8, -1, my_app_rank,
     &                              TYPE_B, priority_B, ierr)
                     write (6,*) 'PUT B VAL', work_B(1), work_B(2)
                     call flush(6)
                     priority_B = priority_A - 2
                 endif
                 if ( time_unit_this_A <= num_time_units_per_A ) then
                     work_A(3) = time_unit_this_A + 1
                     priority_A = priority_A - 3
                     call ADLB_PUT( work_A, 20*8, -1, my_app_rank,
     &                               TYPE_A, priority_A, ierr )
                     write (6,*) 'PUT A VAL', work_A(1), work_A(2)
                     call flush(6)
                 endif
                 write (6,*) 'HANDLED A VAL', work_A(1), work_A(2)
                 call flush(6)
             elseif ( work_type == TYPE_B ) then
                 call ADLB_GET_RESERVED( work_B, work_handle, ierr )
                 if ( ierr == ADLB_NO_MORE_WORK ) then
                     write (6,*) 'GOT NO_MORE_WORK x3'
                     call flush(6)
                     go to 10  !! fake break
                 endif
                 write (6,*) 'HANDLING B VAL', work_B(1), work_B(2)
                 call flush(6)
                 call ADLB_BEGIN_BATCH_PUT( NULL, 0, ierr )
                 do i = 1, CS_PER_B, 1
                     work_C(1) = work_B(1)
                     work_C(2) = work_B(2)
                     call ADLB_PUT(work_C, 20*8, -1, my_app_rank,
     &                             TYPE_C, priority_C, ierr )
                     write (6,*) 'PUT C VAL', work_C(1), work_C(2)
                     call flush(6)
                     priority_C = priority_B + 1
                 enddo
                 call ADLB_END_BATCH_PUT( ierr )
                 sum = 0
                 num_C_answers = 0
                 do while ( num_C_answers < CS_PER_B )
                     write (6,*) 'PT1'
                     call flush(6)
                     call MPI_IPROBE( MPI_ANY_SOURCE, TAG_C_ANSWER,
     &                                app_comm, msg_available,
     &                                status, ierr )
                     write (6,*) 'PT2', msg_available
                     call flush(6)
                     if ( msg_available > 0 ) then
                         call MPI_RECV( iv, 1, MPI_INTEGER,
     &                                  MPI_ANY_SOURCE,
     &                                  TAG_C_ANSWER, app_comm,
     &                                  status, ierr )
                         sum = sum + iv
                         num_C_answers = num_C_answers + 1
                     else
                         REQ_TYPES(1) = TYPE_C
                         call ADLB_IRESERVE( REQ_TYPES,
     &                                       work_type, work_prio,
     &                                       work_handle, work_len,
     &                                       answer_rank, ierr )
                         if ( ierr == ADLB_NO_MORE_WORK ) then
                             write (6,*) 'GOT NO_MORE_WORK x4'
                             call flush(6)
                             go to 10  !! fake break
                         endif
                         if ( ierr > 0 ) then
                             write (6,*) 'PT3'
                             call flush(6)
                             call ADLB_GET_RESERVED( work_C,
     &                                               work_handle,
     &                                               ierr )
                             if ( ierr == ADLB_NO_MORE_WORK ) then
                                 write (6,*) 'GOT NO_MORE_WORK x5'
                                 call flush(6)
                                 go to 10  !! fake break
                             endif
                             write (6,*) 'HANDLING C VAL',
     &                                    work_C(1), work_C(2)
                             call flush(6)
                             tempval = work_C(1)
                             do i = 1, num_delay, 1
                                 tempval = sqrt(tempval + 5000000.0) + 1
                             enddo
                             if ( answer_rank .eq. my_world_rank ) then
                                 sum = sum + 1
                                 num_C_answers = num_C_answers + 1
                             else
                                 j = 1
                                 call MPI_SEND( j, 1, MPI_INTEGER,
     &                                          answer_rank,
     &                                          TAG_C_ANSWER,
     &                                          app_comm, ierr )
                             endif
                             write (6,*) 'bHANDLED C VAL',
     &                                   work_C(1), work_C(2)
                         else
                             call MPI_RECV( iv, 1, MPI_INTEGER,
     &                                      MPI_ANY_SOURCE,
     &                                      TAG_C_ANSWER, app_comm,
     &                                      status, ierr )
                             sum = sum + iv
                             num_C_answers = num_C_answers + 1
                         endif
                     endif
                 enddo  !! recving C answers
                 if ( ierr == ADLB_NO_MORE_WORK )  then
                     go to 10  !! fake break
                 endif
                 call MPI_SEND( sum, 1, MPI_INTEGER, MASTER_RANK,
     &                          TAG_B_ANSWER, app_comm, ierr )
                 write (6,*) 'HANDLED B VAL', work_B(1), work_B(2)
                 call flush(6)
             elseif ( work_type == TYPE_C ) then
                 call ADLB_GET_RESERVED( work_C, work_handle, ierr )
                 if ( ierr == ADLB_NO_MORE_WORK ) then
                     write (6,*) 'GOT NO_MORE_WORK x6'
                     call flush(6)
                     go to 10  !! fake break
                 endif
                 write (6,*) 'bHANDLING C VAL', work_C(1), work_C(2)
                 call flush(6)
                 tempval = work_C(1)
                 do i = 1, num_delay, 1
                     tempval = sqrt(tempval + 5000000.0) + 1
                 enddo
                 j = 1
                 call MPI_SEND( j, 1, MPI_INTEGER, answer_rank,
     &                          TAG_C_ANSWER, app_comm, ierr )
                 write (6,*) 'bHANDLED C VAL', work_C(1), work_C(2)
                 call flush(6)
             endif  !! if TYPE_C
          enddo  !! infinite loop
 10       continue              !! destination of fake break
      endif  !! slave

      if ( my_world_rank == MASTER_RANK ) then
          end_time = MPI_WTIME()
          write (6,*) 'total time =', end_time - start_time
      endif
      write (6,*) 'AT ADLB_FINALIZE'
      call flush(6)
      call ADLB_FINALIZE( ierr )
      write (6,*) 'AT MPI_FINALIZE'
      call flush(6)
      call MPI_FINALIZE( ierr )
      write (6,*) 'DONE'
      call flush(6)

      stop
      end
