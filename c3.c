#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adlb.h"

#define NUM_AS_TO_GEN_PER_BATCH  100
#define NUM_BS_TO_GEN_PER_BATCH  100
#define NUM_CS_TO_GEN_PER_BATCH   60
#define MAX_LOOP1_IDX              2
#define MAX_LOOP2_IDX              4

#define TIME_FOR_FAKE_A_WORK     1.8
#define TIME_FOR_FAKE_C_WORK     0.4

#define MASTER_RANK                0

#define TYPE_A                     1
#define TYPE_A_ANSWER              2
#define TYPE_B                     3
#define TYPE_C                     4
#define TYPE_C_ANSWER              5
#define TYPE_NEVER_PUT_FOR_MASTER  6

#define SRVR_MAX_MALLOC_AMT  (1024 * 1024 * 1024)


void do_fake_work_for_secs(double);

int num_types = 6;
int type_vect[6] = { TYPE_A, TYPE_A_ANSWER,
                     TYPE_B,
                     TYPE_C, TYPE_C_ANSWER,
                     TYPE_NEVER_PUT_FOR_MASTER };

double start_time, end_time;

int main(int argc, char *argv[])
{
    int i, rc, provided;
    int num_servers, num_slaves, num_world_ranks, my_world_rank;
    int num_app_ranks, my_app_rank;
    int num_As_to_gen_per_batch, num_Bs_to_gen_per_batch, num_Cs_to_gen_per_batch;
    int num_As, num_Bs, num_Cs;
    int am_server, use_debug_server, am_debug_server, aprintf_flag;
    int req_types[4], work_type, answer_rank, work_len;
    int work_prio, work_handle[ADLB_HANDLE_SIZE];
    int exp_num_As = 0, exp_num_Bs = 0, exp_num_Cs = 0, exp_num_batches = 0;
    int num_A_answers_this_batch, num_A_answers_total;
    int num_C_answers_this_batch, num_C_answers_total;
    int stat_num_A_answers, stat_num_C_answers;
    int work_A[20], work_B[10], work_C[20];
    int total_num_puts, total_num_reserves, total_num_gets;
    int loopidx1, loopidx2, num_slaves_doing_first_phase;
    int priority_A = 3, priority_B = 2, priority_C = 1;
    int priority_A_ANSWER = 9, priority_C_ANSWER = 9;
    double time_for_fake_A_work, time_for_fake_C_work;
    double time1, total_put_time, total_reserve_time, total_get_time;
    double malloc_hwm, avg_time_on_rq;
    MPI_Comm app_comm;

    time_for_fake_A_work = TIME_FOR_FAKE_A_WORK;
    time_for_fake_C_work = TIME_FOR_FAKE_C_WORK;
    num_As_to_gen_per_batch = NUM_AS_TO_GEN_PER_BATCH;
    num_Bs_to_gen_per_batch = NUM_BS_TO_GEN_PER_BATCH;
    num_Cs_to_gen_per_batch = NUM_CS_TO_GEN_PER_BATCH;
    num_servers = 1;
    use_debug_server = 0;
    aprintf_flag = 1;

    for (i=1; i < argc; i++)
    {
        if (strcmp(argv[i],"-nservers") == 0)
            num_servers = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-nas") == 0)
            num_As_to_gen_per_batch = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-nbs") == 0)
            num_Bs_to_gen_per_batch = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-ncs") == 0)
            num_Cs_to_gen_per_batch = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-atime") == 0)
            time_for_fake_A_work = atof(argv[i+1]);
        else if (strcmp(argv[i],"-ctime") == 0)
            time_for_fake_C_work = atof(argv[i+1]);
        else if (strcmp(argv[i],"-debug") == 0)
            use_debug_server = atoi(argv[i+1]);
    }

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_ranks);
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);
    rc = ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,type_vect,
                   &am_server,&am_debug_server,&app_comm);

    if ( ! am_server  &&  ! am_debug_server)
    {
        MPI_Comm_size(app_comm,&num_app_ranks);
        MPI_Comm_rank(app_comm,&my_app_rank);
        if (num_app_ranks < 2)
        {
            aprintf(1,"**** TOO FEW APP RANKS - requires at least a master and one slave\n");
            ADLB_Abort(-1);
        }
        num_slaves = num_app_ranks - 1;  // all but master_rank
        num_slaves_doing_first_phase = num_app_ranks / 20;  /* int arithmetic */
        if (num_slaves_doing_first_phase <= 0)  /* small numbers of ranks */
            num_slaves_doing_first_phase = 1;
        num_A_answers_total = 0;
        num_C_answers_total = 0;
        num_As = 0;
        num_Bs = 0;
        num_Cs = 0;
        total_put_time = total_reserve_time = total_get_time = 0.0;
        total_num_puts = total_num_reserves = total_num_gets = 0;
    }

    rc = MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    if (am_server)
    {
        ADLB_Server(5000000,(double)0.0);  /* need 5000000 to hold 999 Puts below */
        rc = ADLB_Info_get(ADLB_INFO_MALLOC_HWM,&malloc_hwm);
        rc = ADLB_Info_get(ADLB_INFO_AVG_TIME_ON_RQ,&avg_time_on_rq);
        printf("MALLOC_HWM %.0f  AVG_TIME_ON_RQ %f\n",malloc_hwm,avg_time_on_rq);
    }
    else if (am_debug_server)
    {
        aprintf(1,"BECOMING DEBUG SERVER\n");
        ADLB_Debug_server(300.0);
    }
    else if (my_app_rank == MASTER_RANK)  /* if master */
    {
        start_time = MPI_Wtime();
        exp_num_As = num_slaves_doing_first_phase  * 
                     MAX_LOOP1_IDX * MAX_LOOP2_IDX * num_As_to_gen_per_batch;
        exp_num_Bs = num_slaves_doing_first_phase  *
                     MAX_LOOP1_IDX * 1             * num_Bs_to_gen_per_batch;
        exp_num_Cs = exp_num_Bs * num_Cs_to_gen_per_batch;
        exp_num_batches = (MAX_LOOP1_IDX * MAX_LOOP2_IDX)  // num A batches
                          + MAX_LOOP1_IDX                  // num B batches
                          + exp_num_Bs;                   // num C batches
        aprintf(1,"STAT nums to handle: As %d  Bs %d  Cs %d totalABC %d  batches %d\n",
                  exp_num_As,exp_num_Bs,exp_num_Cs,
                  exp_num_As+exp_num_Bs+exp_num_Cs,exp_num_batches);
        /* It should be possible to go into ADLB_Finalize at this point but I chose not to
           do so because aprintf may not function correctly afterwards
        */
        req_types[0] = TYPE_NEVER_PUT_FOR_MASTER;  /* so I can hang until exhaustion */
        req_types[1] = req_types[2] = req_types[3] = -1;
        aprintf(1111,"AT 0 RESERVE\n");
        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                          &work_len,&answer_rank);
        // aprintf(1111,"PAST 0 RESERVE\n");
        if (rc == ADLB_DONE_BY_EXHAUSTION)  /* exhaustion */
        {
            aprintf(1,"********** master done\n");
        }
        else
        {
            aprintf(1,"**** RESERVE GOT UNEXPECTED rc %d\n",rc);
            ADLB_Abort(-1);
        }
    }
    else // normal app slave
    {
        /* 1st phase */
        /* 5% of the slaves go in here initially */
        if (my_app_rank <= num_slaves_doing_first_phase)
        {
            if (my_app_rank == 1)  /* 1 always does this */
                aprintf(1,":::: entering 1st phase ::::::::::::\n");
            for (loopidx1=0; loopidx1 < MAX_LOOP1_IDX; loopidx1++)
            {
                for (loopidx2=0; loopidx2 < MAX_LOOP2_IDX; loopidx2++)
                {
                    aprintf(1,"begin_batch_put A\n");
                    ADLB_Begin_batch_put( NULL, 0 );
                    for (i=0; i < num_As_to_gen_per_batch; i++)    /* put As */
                    {
                        /* put "my" As in the pool with priority_A */
                        memset(work_A,0,20*sizeof(int));  /* an A is 20 doubles         */
                        work_A[0] = (my_app_rank);      /* id of A is orig rank plus  */
                        work_A[1] = num_As + 1;           /*    a unique Aid here       */
                        time1 = MPI_Wtime();
                        rc = ADLB_Put(work_A,20*sizeof(int),-1,my_app_rank,TYPE_A,priority_A); 
                        total_put_time += MPI_Wtime() - time1;
                        total_num_puts++;
                        aprintf(1111,"PUT A VAL %d %d\n",(int)work_A[0],(int)work_A[1]);
                        num_As++;
                    }
                    ADLB_End_batch_put();
                    num_A_answers_this_batch = 0;
                    while (num_A_answers_this_batch < num_As_to_gen_per_batch)
                    {
                        req_types[0] = TYPE_A;
                        req_types[1] = TYPE_A_ANSWER;
                        req_types[2] = req_types[3] = -1;
                        time1 = MPI_Wtime();
                        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                                          &work_len,&answer_rank);
                        if (rc == ADLB_DONE_BY_EXHAUSTION)
                        {
                            aprintf(1,"OOPS: **** got exhaustion before all A answers\n");
                            ADLB_Abort(-1);
                        }
                        if (rc < 0)
                        {
                            aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                            ADLB_Abort(-1);
                        }
                        total_reserve_time += MPI_Wtime() - time1;  // do time here, only good rsrv
                        total_num_reserves++;
                        if (work_type == TYPE_A)
                        {
                            time1 = MPI_Wtime();
                            rc = ADLB_Get_reserved(work_A,work_handle);
                            total_get_time += MPI_Wtime() - time1;
                            total_num_gets++;
                            do_fake_work_for_secs(time_for_fake_A_work);
                            if (answer_rank == my_app_rank)
                            {
                                num_A_answers_this_batch++;
                                num_A_answers_total++;
                            }
                            else
                            {
                                time1 = MPI_Wtime();
                                rc = ADLB_Put(work_A,20*sizeof(int),answer_rank,-1,
                                              TYPE_A_ANSWER,priority_A_ANSWER); 
                                total_put_time += MPI_Wtime() - time1;
                                total_num_puts++;
                            }
                            aprintf(1111,"HANDLED A VAL %d %d\n",work_A[0],work_A[1]);
                        }
                        else if (work_type == TYPE_A_ANSWER)
                        {
                            time1 = MPI_Wtime();
                            rc = ADLB_Get_reserved(work_A,work_handle);
                            total_get_time += MPI_Wtime() - time1;
                            total_num_gets++;
                            num_A_answers_this_batch++;
                            num_A_answers_total++;
                        }
                        else
                        {
                            aprintf(1,"**** UNEXECTED TYPE RETRIEVED BY RESERVE %d\n",work_type);
                            ADLB_Abort(-1);
                        }
                    }
                    aprintf(1111,"NUM A answers handled this loop %d\n",num_A_answers_this_batch);
                }
                aprintf(1,"begin_batch_put B\n");
                ADLB_Begin_batch_put( NULL, 0 );
                for (i=0; i < num_Bs_to_gen_per_batch; i++)    /* put Bs */
                {
                    memset(work_B,0,10*sizeof(int));    /* a B is 10 doubles */
                    work_B[0] = my_app_rank;
                    work_B[1] = num_Bs + 1;
                    time1 = MPI_Wtime();
                    rc = ADLB_Put(work_B,10*sizeof(int),-1,my_app_rank,TYPE_B,priority_B); 
                    total_put_time += MPI_Wtime() - time1;
                    total_num_puts++;
                    num_Bs++;
                }
                ADLB_End_batch_put();
            }
        }

        /* 2nd phase */
        /* all slaves go in here after 5% perform the if-block above first */
        if (my_app_rank == 1)  /* 1 always does the above loop */
            aprintf(1,":::: entering 2nd phase ::::::::::::\n");
        while (1)
        {
            req_types[0] = req_types[1] = req_types[2] = req_types[3] = -1;
            // aprintf(1111,"AT TOP RESERVE\n");
            time1 = MPI_Wtime();
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                              &work_len,&answer_rank);
            // aprintf(1111,"PAST TOP RESERVE rc %d\n",rc);
            if (rc == ADLB_DONE_BY_EXHAUSTION)
            {
                // aprintf(1,"GOT DONE_BY_EXHAUSTION x1\n");
                break;
            }
            if (rc < 0)
            {
                aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                ADLB_Abort(-1);
            }
            total_reserve_time += MPI_Wtime() - time1;  // do time here, only for good reserve
            total_num_reserves++;
            if (work_type == TYPE_A)
            {
                time1 = MPI_Wtime();
                rc = ADLB_Get_reserved(work_A,work_handle);
                if (rc == ADLB_DONE_BY_EXHAUSTION)
                {
                    // aprintf(1,"GOT DONE_BY_EXHAUSTION x2\n");
                    break;
                }
                if (rc < 0)
                {
                    aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                    ADLB_Abort(-1);
                }
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                do_fake_work_for_secs(time_for_fake_A_work);
                // aprintf(1111,"HANDLING A VAL %d %d\n",work_A[0],work_A[1]);
                time1 = MPI_Wtime();
                rc = ADLB_Put(work_A,20*sizeof(int),answer_rank,-1,
                              TYPE_A_ANSWER,priority_A_ANSWER); 
                total_put_time += MPI_Wtime() - time1;
                total_num_puts++;
                // aprintf(1111,"HANDLED A VAL %d %d\n",work_A[0],work_A[1]);
            }
            else if (work_type == TYPE_B)
            {
                time1 = MPI_Wtime();
                rc = ADLB_Get_reserved(work_B,work_handle);
                if (rc == ADLB_DONE_BY_EXHAUSTION)
                {
                    // aprintf(1,"GOT DONE_BY_EXHAUSTION x3\n");
                    break;
                }
                if (rc < 0)
                {
                    aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                    ADLB_Abort(-1);
                }
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                // aprintf(1111,"HANDLING B VAL %d %d\n",work_B[0],work_B[1]);
                aprintf(1,"begin_batch_put C\n");
                ADLB_Begin_batch_put( NULL, 0 );
                for (i=0; i < num_Cs_to_gen_per_batch; i++)
                {
                    memset(work_C,0,20*sizeof(int));    // an C is 20 doubles
                    work_C[0] = work_B[0];    // a C is identified by the B (A) whence it comes
                    work_C[1] = work_B[1];    //     including both rank and Aid
                    work_C[2] = i;            // uniquify the C also
                    time1 = MPI_Wtime();
                    rc = ADLB_Put(work_C,20*sizeof(int),-1,my_app_rank,TYPE_C,priority_C); 
                    total_put_time += MPI_Wtime() - time1;
                    total_num_puts++;
                    num_Cs++;
                    // aprintf(1111,"PUT C VAL %d %d %d\n",work_C[0],work_C[1],work_C[2]);
                }
                ADLB_End_batch_put();
                num_C_answers_this_batch = 0;
                while (num_C_answers_this_batch < num_Cs_to_gen_per_batch)
                {
                    req_types[0] = TYPE_C;
                    req_types[1] = TYPE_C_ANSWER;
                    req_types[2] = req_types[3] = -1;
                    time1 = MPI_Wtime();
                    rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                                      &work_len,&answer_rank);
                    if (rc == ADLB_DONE_BY_EXHAUSTION)
                    {
                        aprintf(1,"OOPS: **** got exhaustion before all C answers\n");
                        ADLB_Abort(-1);
                    }
                    if (rc < 0)
                    {
                        aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                        ADLB_Abort(-1);
                    }
                    total_reserve_time += MPI_Wtime() - time1;
                    total_num_reserves++;
                    time1 = MPI_Wtime();
                    rc = ADLB_Get_reserved(work_C,work_handle);
                    if (rc == ADLB_DONE_BY_EXHAUSTION)
                    {
                        // aprintf(1111,"GOT DONE_BY_EXHAUSTION x6\n");
                        break;
                    }
                    if (rc < 0)
                    {
                        aprintf(1,"**** GET FAILED rc %d\n",rc);
                        ADLB_Abort(-1);
                    }
                    total_get_time += MPI_Wtime() - time1;
                    total_num_gets++;
                    if (work_type == TYPE_C)
                    {
                        // aprintf(1111,"HANDLING C VAL %d %d %d  ar=%d\n",
                                  // work_C[0],work_C[1],work_C[2],answer_rank);
                        do_fake_work_for_secs(time_for_fake_C_work);
                        // aprintf(1111,"HANDLED C VAL %d %d %d  ar=%d\n",
                                  // work_C[0],work_C[1],work_C[2],answer_rank);
                        // Put C answer targeted to the answer_rank
                        time1 = MPI_Wtime();
                        rc = ADLB_Put(work_C,20*sizeof(int),answer_rank,-1,
                                      TYPE_C_ANSWER,priority_C_ANSWER); 
                        total_put_time += MPI_Wtime() - time1;
                        total_num_puts++;
                        // aprintf(1111,"PUT C VAL %d %d %d\n",work_C[0],work_C[1],work_C[2]);
                    }
                    else if (work_type == TYPE_C_ANSWER)
                    {
                        // aprintf(1111,"HANDLING Canswer VAL %d %d %d\n",
                                  // work_C[0],work_C[1],work_C[2]);
                        num_C_answers_this_batch++;
                        num_C_answers_total++;
                        // aprintf(1111,"HANDLED Canswer VAL %d %d %d  numCanswers %d\n",
                                  // work_C[0],work_C[1],work_C[2],num_C_answers_this_batch);
                    }
                    else
                    {
                        aprintf(1,"** UNKNOWN TYPE FOUND %d\n",work_type);
                        ADLB_Abort(-1);
                    }
                }
                // aprintf(1111,"NUM C answers handled this loop %d\n",num_C_answers_this_batch);
                if (rc == ADLB_DONE_BY_EXHAUSTION)  // may have arrived in Canswer loop
                    break;
                // aprintf(1111,"HANDLED B VAL %d %d\n",work_B[0],work_B[1]);
            }
            else if (work_type == TYPE_C)
            {
                time1 = MPI_Wtime();
                rc = ADLB_Get_reserved(work_C,work_handle);
                if (rc == ADLB_DONE_BY_EXHAUSTION)
                {
                    // aprintf(1,"GOT DONE_BY_EXHAUSTION x6\n");
                    break;
                }
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                // aprintf(1111,"HANDLING C VAL %d %d %d  ar=%d\n",
                          // work_C[0],work_C[1],work_C[2],answer_rank);
                do_fake_work_for_secs(time_for_fake_C_work);
                // Put C answer targeted to the answer_rank
                time1 = MPI_Wtime();
                rc = ADLB_Put(work_C,20*sizeof(int),answer_rank,-1,
                              TYPE_C_ANSWER,priority_C_ANSWER); 
                total_put_time += MPI_Wtime() - time1;
                total_num_puts++;
                // aprintf(1111,"HANDLED C VAL %d %d %d  ar=%d\n",
                          // work_C[0],work_C[1],work_C[2],answer_rank);
            }
        }
    }  // app slaves

    if ( ! am_server  &&  ! am_debug_server)
    {
        end_time = MPI_Wtime();
        MPI_Reduce(&num_A_answers_total,&stat_num_A_answers,1,
                   MPI_INT,MPI_SUM,MASTER_RANK,app_comm );
        MPI_Reduce(&num_C_answers_total,&stat_num_C_answers,1,
                   MPI_INT,MPI_SUM,MASTER_RANK,app_comm );
        if (my_app_rank == MASTER_RANK)  /* if master */
        {
            aprintf(1,"STAT: total time = %10.2f\n",end_time-start_time);
            aprintf(1,"STAT: num answers: As %d Cs %d\n",stat_num_A_answers,stat_num_C_answers);
            if (exp_num_As != stat_num_A_answers  ||  exp_num_Cs != stat_num_C_answers)
            {
                aprintf(1,"STAT: OOPS: wrong number of As or Cs computed ********\n");
                ADLB_Abort(-1);
            }
        }
        else
        {
            aprintf(1,"c3: total put time %14.6f  numputs %d  avg %14.6f\n",
                      total_put_time,total_num_puts,total_put_time/total_num_puts);
            aprintf(1,"c3: total reserve time %14.6f  numreserves %d  avg %14.6f\n",
                      total_reserve_time,total_num_reserves,
                      total_reserve_time/total_num_reserves);
            aprintf(1,"c3: total get time %14.6f  numgets %d  avg %14.6f\n",
                      total_get_time,total_num_gets,total_get_time/total_num_gets);
            aprintf(1,"c3: answers: As %d Cs %d\n",num_A_answers_total,num_C_answers_total);
        }
    }

    aprintf(1,"AT ADLB_FINALIZE\n");
    ADLB_Finalize();
    aprintf(1,"AT MPI_FINALIZE\n");
    MPI_Finalize();
    printf("DONE\n");

    return 0;
}

void do_fake_work_for_secs(double num_secs)
{
    double time1, tempval = 99.99;

    time1 = MPI_Wtime();
    while ((MPI_Wtime() - time1) < num_secs)
        tempval = sqrt(tempval + 50000.0) + 1.0;
}
