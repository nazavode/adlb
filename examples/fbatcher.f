      implicit real*8 ( a-z )
      
      include "mpif.h"
      include "adlbf.h"

      integer :: status(MPI_STATUS_SIZE)
      integer :: work_handle(ADLB_HANDLE_SIZE)
      integer :: ierr, aprintf_flag, num_servers, use_debug_server,
     &           am_server, am_debug_server, app_comm,
     &           my_world_rank, num_world_nodes,
     &           num_app_nodes, my_app_rank,
     &           work_type, work_prio, work_len, answer_rank

      integer :: REQ_TYPES(4) = (/-1,-1,-1,-1/)

      integer, parameter ::  NUM_TYPES = 1
      integer, parameter ::  TYPE_VECT(1) = (/1/)
      integer, parameter ::  MASTER_RANK = 0

      real*8 :: malloc_hwm
      real*8 :: temp_max_malloc, temp_periodic
      real*8 :: debug_server_timeout

      logical done

      character(300) :: infilename, line
      
      call MPI_Init( ierr )
      if ( ierr /= MPI_SUCCESS ) then
          write(6,*) 'MPI_Init failed with rc=', ierr
      endif
      call MPI_COMM_RANK( MPI_COMM_WORLD, my_world_rank, ierr )
      call MPI_COMM_SIZE( MPI_COMM_WORLD, num_world_nodes, ierr )
      write (6,*) 'starting', my_world_rank, num_world_nodes
      call flush(6)

      aprintf_flag = 1
      num_servers = 1
      use_debug_server = 0

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
      else  !!  workers
          if (my_app_rank == MASTER_RANK) then
              call getarg ( 1, infilename )
              write (6,'(10a)') 'reading file ', trim(infilename)
              open ( 5, file=infilename, form="FORMATTED",
     &               status="OLD", action="READ" )
              do while ( .true. )
                  read ( 5, '(a)', end=100 ) line
                  call ADLB_PUT( line,300,-1,-1,1,1,ierr)
                  write(6,*) 'PUTCMD=', trim(line)
                  call flush(6)
              enddo
 100          continue              !! destination for end of read
          endif  !! if master_rank

          done = .false.
          do while ( .not. done )
              REQ_TYPES(1) = -1
              call ADLB_RESERVE( REQ_TYPES,
     &                           work_type, work_prio, work_handle,
     &                           work_len, answer_rank, ierr )
              write (6,*) 'RESERVE IERR', ierr
              call flush(6)
              if ( ierr == ADLB_DONE_BY_EXHAUSTION ) then
                  write (6,*) 'GOT EXHAUSTION '
                  call flush(6)
                  go to 200  !! fake break
              endif
              if ( ierr == ADLB_NO_MORE_WORK ) then
                  write (6,*) 'GOT NO_MORE_WORK '
                  call flush(6)
                  go to 200  !! fake break
              endif
              if ( ierr == 0 ) then
                  write (6,*) '******** RESERVE FAILED ********'
                  call flush(6)
                  call ADLB_ABORT( -1, ierr )
              endif
              if ( work_type /= 1 ) then
                  write (6,*) '******** UNEXPECTED TYPE ********', work_type
                  call flush(6)
                  call ADLB_ABORT( -1, ierr )
              endif
              call ADLB_GET_RESERVED( line, work_handle, ierr )
              if ( ierr == ADLB_NO_MORE_WORK ) then
                  write (6,*) 'GOT NO_MORE_WORK x2'
                  call flush(6)
                  go to 200  !! fake break
              endif
              write (6,*) 'HANDLING WORK ', line
              call flush(6)
          enddo
 200  continue              !! destination for fake break
      endif  !! else workers

      if ( my_world_rank == MASTER_RANK ) then
          end_time = MPI_WTIME()
          write (6,*) 'total time =', end_time - start_time
      endif
      write (6,*) 'AT ADLB_FINALIZE', my_world_rank
      call flush(6)
      call ADLB_FINALIZE( ierr )
      write (6,*) 'AT MPI_FINALIZE', my_world_rank
      call flush(6)
      call MPI_FINALIZE( ierr )
      write (6,*) 'DONE'
      call flush(6)

      stop
      end
