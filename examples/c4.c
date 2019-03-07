/*
    This program is a mini-app that implements the fundamental algorithm
    performed by the gfmc physics app.  Pseudocode for the algorithm is
    provided in the accompanying file named gfmcAlgorithm.txt.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adlb.h"

#define NUM_AS_TO_GEN_PER_BATCH   40  /* gfmc: 40 in walkers */
#define NUM_BS_TO_GEN_PER_BATCH   40  /* gfmc: 40 in walkers */
#define NUM_CS_TO_GEN_PER_BATCH   15  /* gfmc: 15 (or random number) in workers */
#define NUM_DS_TO_GEN_PER_BATCH   72  /* gfmc: 72 in large D batch */
#define WALKER_OUTER_IDX_M        10  /* gfmc: M in walkers; as much as you want */
#define WALKER_INNER_IDX          20  /* gfmc: 20 in walkers */

#define TIME_FOR_A_WORK          0.5
#define TIME_FOR_B_WORK          0.5
#define TIME_FOR_C_WORK          0.1
#define TIME_FOR_D_WORK          0.5

#define MASTER_RANK                0

#define TYPE_A                     1
#define TYPE_A_ANSWER              2
#define TYPE_B                     3
#define TYPE_B_ANSWER              4
#define TYPE_C                     5
#define TYPE_C_ANSWER              6
#define TYPE_D                     7
#define TYPE_D_ANSWER              8

#define SRVR_MAX_MALLOC_AMT  (1024 * 1024 * 1024)


void do_work_for_secs(double num_secs);
void do_put_Cs(int num_to_put);
void do_put_Ds(int num_to_put);
int do_get_and_handle_C_answers(int num_to_get);
int do_get_and_handle_D_answers(int num_to_get);

int num_types = 8;
int type_vect[8] = { TYPE_A, TYPE_A_ANSWER,
                     TYPE_B, TYPE_B_ANSWER,
                     TYPE_C, TYPE_C_ANSWER,
                     TYPE_D, TYPE_D_ANSWER };

int num_app_ranks, my_app_rank;
int num_As, num_Bs, num_Cs, num_Ds;
int work_A[20], work_B[20], work_C[20], work_D[20];
int num_As_to_gen_per_batch, num_Bs_to_gen_per_batch;
int num_Cs_to_gen_per_batch, num_Ds_to_gen_per_batch;
int am_server, use_debug_server, am_debug_server, aprintf_flag;
int exp_num_As, exp_num_Bs, exp_num_Cs, exp_num_Ds;
int num_A_answers_this_batch, num_A_answers_total;
int num_C_answers_this_batch, num_C_answers_total;
int num_D_answers_this_batch, num_D_answers_total;
int stat_num_A_answers, stat_num_C_answers, stat_num_D_answers;
int total_num_puts, total_num_reserves, total_num_gets;
int loopidx1, loopidx2, num_walkers, walker_outer_idx_m, walker_inner_idx;
int priority_A = 1, priority_B = 1, priority_C = 2, priority_D = 3;
int priority_A_ANSWER = 9, priority_B_ANSWER = 9, priority_C_ANSWER = 9, priority_D_ANSWER = 9;
double time_for_A_work, time_for_B_work, time_for_C_work, time_for_D_work;
double time1, total_put_time, total_reserve_time, total_get_time;
double malloc_hwm, avg_time_on_rq;
double start_time, end_time;
MPI_Comm app_comm;

int main(int argc, char *argv[])
{
    int i, rc, provided;
    int num_servers, num_slaves, num_world_ranks, my_world_rank;
    int req_types[5], work_type, answer_rank, work_len;
    int work_prio, work_handle[ADLB_HANDLE_SIZE];

    time_for_A_work = TIME_FOR_A_WORK;
    time_for_B_work = TIME_FOR_B_WORK;
    time_for_C_work = TIME_FOR_C_WORK;
    time_for_D_work = TIME_FOR_D_WORK;
    num_As_to_gen_per_batch = NUM_AS_TO_GEN_PER_BATCH;
    num_Bs_to_gen_per_batch = NUM_BS_TO_GEN_PER_BATCH;
    num_Cs_to_gen_per_batch = NUM_CS_TO_GEN_PER_BATCH;
    num_Ds_to_gen_per_batch = NUM_DS_TO_GEN_PER_BATCH;
    walker_outer_idx_m = WALKER_OUTER_IDX_M;
    walker_inner_idx   = WALKER_INNER_IDX;
    num_walkers = 0;  /* changes below */
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
        else if (strcmp(argv[i],"-nds") == 0)
            num_Ds_to_gen_per_batch = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-nws") == 0)
            num_walkers = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-atime") == 0)
            time_for_A_work = atof(argv[i+1]);
        else if (strcmp(argv[i],"-btime") == 0)
            time_for_B_work = atof(argv[i+1]);
        else if (strcmp(argv[i],"-ctime") == 0)
            time_for_C_work = atof(argv[i+1]);
        else if (strcmp(argv[i],"-dtime") == 0)
            time_for_D_work = atof(argv[i+1]);
        else if (strcmp(argv[i],"-m") == 0)
            walker_outer_idx_m = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-i") == 0)
            walker_inner_idx = atoi(argv[i+1]);
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
        if (num_walkers <= 0)  /* not overridden on cmd-line */
            num_walkers = num_app_ranks / 50;  /* int arithmetic */
        if (num_walkers <= 0)  /* may still be low if small number of ranks */
            num_walkers = 1;
        num_A_answers_total = 0;
        num_C_answers_total = 0;
        num_D_answers_total = 0;
        num_As = 0;
        num_Bs = 0;
        num_Cs = 0;
        num_Ds = 0;
        total_put_time = total_reserve_time = total_get_time = 0.0;
        total_num_puts = total_num_reserves = total_num_gets = 0;
    }

    rc = MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    if (am_server)
    {
        ADLB_Server(SRVR_MAX_MALLOC_AMT,(double)0.0);  /* need 5000000 to hold 999 Puts below */
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
        exp_num_As = num_walkers * walker_outer_idx_m * walker_inner_idx * 
                     num_As_to_gen_per_batch * 2;   /* 2 -> every A gens one more A */
        exp_num_Bs = num_Bs_to_gen_per_batch * num_walkers * walker_outer_idx_m;
        exp_num_Cs = exp_num_Bs * num_Cs_to_gen_per_batch;
        exp_num_Ds = exp_num_As + (exp_num_Bs * num_Ds_to_gen_per_batch) + exp_num_Cs * 3;
        aprintf(1,"STAT: numworldranks %d  numservers %d  numwalkers %d\n",
                num_world_ranks,num_servers,num_walkers);
        aprintf(1,"STAT: outeridxM %d inneridx %d  atime %.2f btime %.2f ctime %.2f dtime %.2f\n",
                walker_outer_idx_m,walker_inner_idx,
                time_for_A_work,time_for_B_work,time_for_C_work,time_for_D_work);
        aprintf(1,"STAT: nums to handle: As %d  Bs %d  Cs %d  Ds %d  totalABCD %d\n",
                  exp_num_As, exp_num_Bs, exp_num_Cs, exp_num_Ds, 
                  exp_num_As+exp_num_Bs+exp_num_Cs+exp_num_Ds);
        for (i=0; i < exp_num_Bs; i++)    // num_walkers * 40 * M
        {
            req_types[0] = TYPE_B_ANSWER;
            req_types[1] = req_types[2] = req_types[3] = req_types[4] = -1;
            time1 = MPI_Wtime();
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                              &work_len,&answer_rank);
            if (rc != ADLB_SUCCESS)
            {
                aprintf(1,"**** RESERVE GOT UNEXPECTED rc %d\n",rc);
                ADLB_Abort(-1);
            }
            total_reserve_time += MPI_Wtime() - time1;
            total_num_reserves++;
            time1 = MPI_Wtime();
            rc = ADLB_Get_reserved(work_B,work_handle);
            aprintf(1111,"HANDLING B ANSWER %d %d\n",(int)work_B[0],(int)work_B[1]);
            total_get_time += MPI_Wtime() - time1;
            total_num_gets++;
        }
        ADLB_Set_problem_done();
        aprintf(1,"********** master done\n");
    }
    else  /* walker or regular app rank */
    {
        /* walkers - 2% of the slaves (or at least 1) go into the "if" block here initially */
        if (my_app_rank <= num_walkers)
        {
            aprintf(1,":::: entering walker phase ::::::::::::\n");
            for (loopidx1=0; loopidx1 < walker_outer_idx_m; loopidx1++)
            {
                for (loopidx2=0; loopidx2 < walker_inner_idx; loopidx2++)
                {
                    aprintf(1,"begin_batch_put A\n");
                    ADLB_Begin_batch_put( NULL, 0 );
                    for (i=0; i < num_As_to_gen_per_batch; i++)    /* put batch of As */
                    {
                        /* put "my" As in the pool with priority_A */
                        memset(work_A,0,20*sizeof(int));  /* an A is 20 doubles         */
                        work_A[0] = (my_app_rank);        /* id of A is orig rank plus  */
                        work_A[1] = num_As + 1;           /*    a unique Aid here       */
                        time1 = MPI_Wtime();
                        rc = ADLB_Put(work_A,20*sizeof(int),-1,my_app_rank,TYPE_A,priority_A); 
                        total_put_time += MPI_Wtime() - time1;
                        total_num_puts++;
                        num_As++;
                        aprintf(1111,"PUT A VAL p1 %d %d\n",(int)work_A[0],(int)work_A[1]);
                    }
                    ADLB_End_batch_put();
                    num_A_answers_this_batch = 0;
                    while (num_A_answers_this_batch < (2*num_As_to_gen_per_batch))  // A and A'
                    {
                        req_types[0] = TYPE_A_ANSWER;
                        req_types[1] = TYPE_A;
                        req_types[2] = req_types[3] = req_types[4] = -1;
                        time1 = MPI_Wtime();
                        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                                          &work_len,&answer_rank);
                        if (rc < 0)
                        {
                            aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                            ADLB_Abort(-1);
                        }
                        total_reserve_time += MPI_Wtime() - time1;  // do time here, only good rsrv
                        total_num_reserves++;
                        time1 = MPI_Wtime();
                        rc = ADLB_Get_reserved(work_A,work_handle);  // A and Aanswer same size
                        total_get_time += MPI_Wtime() - time1;
                        total_num_gets++;
                        if (work_type == TYPE_A_ANSWER)
                        {
                            aprintf(1111,"HANDLING A ANSWER %d %d\n",(int)work_A[0],(int)work_A[1]);
                            /* only put as many new As here as were created in the batch above */
                            if (num_A_answers_this_batch < num_As_to_gen_per_batch)
                            {
                                memset(work_A,0,20*sizeof(int));  /* an A is 20 doubles         */
                                work_A[0] = (my_app_rank);        /* id of A is orig rank plus  */
                                work_A[1] = num_As + 1;           /*    a unique Aid here       */
                                time1 = MPI_Wtime();
                                rc = ADLB_Put(work_A,20*sizeof(int),-1,my_app_rank,TYPE_A,priority_A); 
                                total_put_time += MPI_Wtime() - time1;
                                total_num_puts++;
                                num_As++;
                                aprintf(1111,"PUT A VAL p2 %d %d\n",(int)work_A[0],(int)work_A[1]);
                            }
                            num_A_answers_this_batch++;
                            num_A_answers_total++;
                        }
                        else if (work_type == TYPE_A)
                        {
                            do_work_for_secs(time_for_A_work);
                            do_put_Ds(1);
                            // aprintf(1111,"PUT D VAL %d %d\n",(int)work_D[0],(int)work_D[1]);
                            rc = do_get_and_handle_D_answers(1);
                            time1 = MPI_Wtime();
                            rc = ADLB_Put(work_A,20*sizeof(int),answer_rank,my_app_rank,
                                          TYPE_A_ANSWER,priority_A_ANSWER); 
                            total_put_time += MPI_Wtime() - time1;
                            total_num_puts++;
                            aprintf(1111,"PUT A ANSWER %d %d for %d\n",work_A[0],work_A[1],answer_rank);
                            aprintf(1111,"HANDLED A VAL %d %d\n",work_A[0],work_A[1]);
                            if (rc == ADLB_NO_MORE_WORK)
                            {
                                break;
                            }
                        }
                        else
                        {
                            aprintf(1,"**** UNEXPECTED TYPE RETRIEVED BY RESERVE %d\n",work_type);
                            ADLB_Abort(-1);
                        }
                    }
                }
                aprintf(1,"begin_batch_put B\n");
                ADLB_Begin_batch_put( NULL, 0 );
                for (i=0; i < num_Bs_to_gen_per_batch; i++)    /* put Bs */
                {
                    memset(work_B,0,20*sizeof(int));    /* a B is 20 doubles */
                    work_B[0] = my_app_rank;
                    work_B[1] = num_Bs + 1;
                    time1 = MPI_Wtime();
                    rc = ADLB_Put(work_B,20*sizeof(int),-1,my_app_rank,TYPE_B,priority_B); 
                    total_put_time += MPI_Wtime() - time1;
                    total_num_puts++;
                    num_Bs++;
                    aprintf(1111,"PUT B VAL %d %d\n",(int)work_B[0],(int)work_B[1]);
                }
                ADLB_End_batch_put();
            }
        }

        aprintf(1,":::: entering general worker phase ::::::::::::\n");
        /* 2nd phase */
        /* all slaves go in here after walkers (2%) first perform the if-block above */
        if (my_app_rank == 1)  /* 1 always does the above loop */
            aprintf(1,":::: entering 2nd phase ::::::::::::\n");
        while (1)
        {
            req_types[0] = TYPE_A;
            req_types[1] = TYPE_B;
            req_types[2] = TYPE_C;
            req_types[3] = TYPE_D;
            req_types[4] = -1;
            time1 = MPI_Wtime();
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,&work_len,&answer_rank);
            if (rc == ADLB_NO_MORE_WORK)
            {
                aprintf(1,"GOT NO_MORE_WORK x1\n");
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
                if (rc == ADLB_NO_MORE_WORK)
                {
                    aprintf(1,"GOT NO_MORE_WORK x2\n");
                    break;
                }
                if (rc < 0)
                {
                    aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                    ADLB_Abort(-1);
                }
                // aprintf(1111,"HANDLING A VAL %d %d\n",work_A[0],work_A[1]);
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                do_work_for_secs(time_for_A_work);
                do_put_Ds(1);
                // aprintf(1111,"PUT D VAL %d %d\n",(int)work_D[0],(int)work_D[1]);
                rc = do_get_and_handle_D_answers(1);
                if (rc == ADLB_NO_MORE_WORK)
                    break;
                time1 = MPI_Wtime();
                rc = ADLB_Put(work_A,20*sizeof(int),answer_rank,my_app_rank,
                              TYPE_A_ANSWER,priority_A_ANSWER); 
                aprintf(1111,"PUT A ANSWER %d %d for %d\n",(int)work_A[0],(int)work_A[1],answer_rank);
                total_put_time += MPI_Wtime() - time1;
                total_num_puts++;
                if (rc == ADLB_NO_MORE_WORK)
                {
                    break;
                }
                aprintf(1111,"HANDLED A VAL %d %d\n",work_A[0],work_A[1]);
            }
            else if (work_type == TYPE_B)
            {
                time1 = MPI_Wtime();
                rc = ADLB_Get_reserved(work_B,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    // aprintf(1,"GOT NO_MORE_WORK x3\n");
                    break;
                }
                if (rc < 0)
                {
                    aprintf(1,"**** GET RESERVED FAILED rc %d\n",rc);
                    ADLB_Abort(-1);
                }
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                // aprintf(1111,"HANDLING B VAL %d %d\n",work_B[0],work_B[1]);
                aprintf(1,"begin_batch_put C\n");
                // make Ds
                ADLB_Begin_batch_put( NULL, 0 );
                do_put_Ds(num_Ds_to_gen_per_batch);
                // aprintf(1111,"PUT D VALs num %d \n",num_Ds_to_gen_per_batch);
                ADLB_End_batch_put();
                rc = do_get_and_handle_D_answers(num_Ds_to_gen_per_batch);
                if (rc == ADLB_NO_MORE_WORK)
                    break;
                // now do the Cs
                ADLB_Begin_batch_put( NULL, 0 );
                do_put_Cs(num_Cs_to_gen_per_batch);
                ADLB_End_batch_put();
                rc = do_get_and_handle_C_answers(num_Cs_to_gen_per_batch);
                if (rc == ADLB_NO_MORE_WORK)  // may have arrived in Canswer loop
                    break;
                do_work_for_secs(time_for_B_work);
                aprintf(1111,"HANDLED B VAL %d %d\n",work_B[0],work_B[1]);
                // send B answer
                memset(work_B,0,20*sizeof(int));    /* a B is 20 doubles */
                work_B[0] = my_app_rank;
                work_B[1] = num_Bs + 1;
                time1 = MPI_Wtime();
                rc = ADLB_Put(work_B,20*sizeof(int),MASTER_RANK,my_app_rank,
                              TYPE_B_ANSWER,priority_B_ANSWER); 
                aprintf(1111,"PUT B ANSWER %d %d\n",work_B[0],work_B[1]);
                total_put_time += MPI_Wtime() - time1;
                total_num_puts++;
            }
            else if (work_type == TYPE_C)
            {
                time1 = MPI_Wtime();
                rc = ADLB_Get_reserved(work_C,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    // aprintf(1,"GOT NO_MORE_WORK x6\n");
                    break;
                }
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                // make Ds
                ADLB_Begin_batch_put( NULL, 0 );
                do_put_Ds(3);
                ADLB_End_batch_put();
                rc = do_get_and_handle_D_answers(3);
                if (rc == ADLB_NO_MORE_WORK)
                    break;
                do_work_for_secs(time_for_C_work);
                // Put C answer targeted to the answer_rank
                time1 = MPI_Wtime();
                rc = ADLB_Put(work_C,20*sizeof(int),answer_rank,my_app_rank,
                              TYPE_C_ANSWER,priority_C_ANSWER); 
                aprintf(1111,"PUT C ANSWER %d %d to %d\n",(int)work_C[0],(int)work_C[1],answer_rank);
                total_put_time += MPI_Wtime() - time1;
                total_num_puts++;
                if (rc == ADLB_NO_MORE_WORK)
                {
                    break;
                }
            }
            else if (work_type == TYPE_D)
            {
                time1 = MPI_Wtime();
                rc = ADLB_Get_reserved(work_D,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    // aprintf(1,"GOT NO_MORE_WORK x6\n");
                    break;
                }
                total_get_time += MPI_Wtime() - time1;
                total_num_gets++;
                do_work_for_secs(time_for_D_work);
                time1 = MPI_Wtime();
                rc = ADLB_Put(work_D,20*sizeof(int),answer_rank,my_app_rank,
                              TYPE_D_ANSWER,priority_D_ANSWER); 
                aprintf(1111,"PUT D ANSWER %d %d for %d\n",(int)work_D[0],(int)work_D[1],answer_rank);
                total_put_time += MPI_Wtime() - time1;
                total_num_puts++;
                // aprintf(1111,"HANDLED D VAL %d %d\n",work_D[0],work_D[1]);
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
        MPI_Reduce(&num_D_answers_total,&stat_num_D_answers,1,
                   MPI_INT,MPI_SUM,MASTER_RANK,app_comm );
        if (my_app_rank == MASTER_RANK)  /* if master */
        {
            aprintf(1,"STAT: num answers: As %d  Bs %d  Cs %d  Ds %d  totalABCD %d\n",
                      stat_num_A_answers,exp_num_Bs,stat_num_C_answers,stat_num_D_answers,
                      stat_num_A_answers+exp_num_Bs+stat_num_C_answers+stat_num_D_answers);
            aprintf(1,"STAT: total time = %10.2f\n",end_time-start_time);
            if (exp_num_As != stat_num_A_answers  ||    // Bs handled by master itself
                exp_num_Cs != stat_num_C_answers  ||
                exp_num_Ds != stat_num_D_answers)
            {
                aprintf(1,"STAT: OOPS: wrong number of As or Cs or Ds computed ********\n");
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
            aprintf(1,"c3: answers: As %d  Bs %d  Cs %d  Ds %d  totalABCD %d\n",
                      num_A_answers_total,exp_num_Bs,num_C_answers_total,num_D_answers_total,
                      num_A_answers_total+exp_num_Bs+num_C_answers_total+num_D_answers_total);
        }
    }

    aprintf(1,"AT ADLB_FINALIZE\n");
    ADLB_Finalize();
    aprintf(1,"AT MPI_FINALIZE\n");
    MPI_Finalize();
    printf("DONE\n");

    return 0;
}

void do_put_Cs(int num_to_put)
{
    int i, rc;

    for (i=0; i < num_to_put; i++)
    {
        memset(work_C,0,20*sizeof(int));
        work_C[0] = my_app_rank;
        work_C[1] = num_Cs + 1;
        time1 = MPI_Wtime();
        rc = ADLB_Put(work_C,20*sizeof(int),-1,my_app_rank,TYPE_C,priority_C); 
        total_put_time += MPI_Wtime() - time1;
        total_num_puts++;
        num_Cs++;
        aprintf(1111,"PUT C VAL %d %d\n",work_C[0],work_C[1]);
    }
}

int do_get_and_handle_C_answers(int num_to_get)
{
    int rc;
    int req_types[5], work_type, answer_rank, work_len;
    int work_prio, work_handle[ADLB_HANDLE_SIZE];

    num_C_answers_this_batch = 0;
    while (num_C_answers_this_batch < num_to_get)
    {
        req_types[0] = TYPE_C;
        req_types[1] = TYPE_C_ANSWER;
        req_types[2] = req_types[3] = req_types[4] = -1;
        time1 = MPI_Wtime();
        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                          &work_len,&answer_rank);
        if (rc < 0)
        {
            aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
            ADLB_Abort(-1);
        }
        total_reserve_time += MPI_Wtime() - time1;
        total_num_reserves++;
        if (work_type == TYPE_C)
        {
            time1 = MPI_Wtime();
            rc = ADLB_Get_reserved(work_C,work_handle);
            if (rc < 0)
            {
                aprintf(1,"**** GET FAILED rc %d\n",rc);
                ADLB_Abort(-1);
            }
            total_get_time += MPI_Wtime() - time1;
            total_num_gets++;
            // make Ds
            ADLB_Begin_batch_put( NULL, 0 );
            do_put_Ds(3);
            ADLB_End_batch_put();
            rc = do_get_and_handle_D_answers(3);
            do_work_for_secs(time_for_C_work);
            time1 = MPI_Wtime();
            rc = ADLB_Put(work_C,20*sizeof(int),answer_rank,my_app_rank,
                          TYPE_C_ANSWER,priority_C_ANSWER); 
            aprintf(1111,"PUT C ANSWER %d %d to %d\n",(int)work_C[0],(int)work_C[1],answer_rank);
            total_put_time += MPI_Wtime() - time1;
            total_num_puts++;
        }
        else if (work_type == TYPE_C_ANSWER)
        {
            time1 = MPI_Wtime();
            rc = ADLB_Get_reserved(work_C,work_handle);
            if (rc < 0)
            {
                aprintf(1,"**** GET FAILED rc %d\n",rc);
                ADLB_Abort(-1);
            }
            total_get_time += MPI_Wtime() - time1;
            total_num_gets++;
            num_C_answers_this_batch++;
            num_C_answers_total++;
            aprintf(1111,"HANDLED C ANSWER from %d\n",answer_rank);
        }
        else
        {
            aprintf(1,"** UNKNOWN TYPE FOUND %d\n",work_type);
            ADLB_Abort(-1);
        }
    }
    return MPI_SUCCESS;
}


void do_put_Ds(int num_to_put)
{
    int i, rc;

    for (i=0; i < num_to_put; i++)
    {
        memset(work_D,0,20*sizeof(int));
        work_D[0] = (my_app_rank);        /* id of D is orig rank plus  */
        work_D[1] = num_Ds + 1;           /*    a unique Did here       */
        time1 = MPI_Wtime();
        rc = ADLB_Put(work_D,20*sizeof(int),-1,my_app_rank,TYPE_D,priority_D); 
        total_put_time += MPI_Wtime() - time1;
        total_num_puts++;
        num_Ds++;
        aprintf(1111,"PUT D VAL %d %d\n",(int)work_D[0],(int)work_D[1]);
    }
}

int do_get_and_handle_D_answers(int num_to_get)
{
    int rc;
    int req_types[5], work_type, answer_rank, work_len;
    int work_prio, work_handle[ADLB_HANDLE_SIZE];

    num_D_answers_this_batch = 0;
    while (num_D_answers_this_batch < num_to_get)
    {
        req_types[0] = TYPE_D_ANSWER;
        req_types[1] = TYPE_D;
        req_types[2] = req_types[3] = req_types[4] = -1;
        time1 = MPI_Wtime();
        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                          &work_len,&answer_rank);
        if (rc < 0)
        {
            aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
            ADLB_Abort(-1);
        }
        total_reserve_time += MPI_Wtime() - time1;
        total_num_reserves++;
        if (work_type == TYPE_D_ANSWER)
        {
            time1 = MPI_Wtime();
            rc = ADLB_Get_reserved(work_D,work_handle);
            if (rc < 0)
            {
                aprintf(1,"**** GET RESERVED FAILED rc %d\n",rc);
                ADLB_Abort(-1);
            }
            total_get_time += MPI_Wtime() - time1;
            total_num_gets++;
            num_D_answers_this_batch++;
            num_D_answers_total++;
            aprintf(1111,"HANDLED D ANSWER %d %d\n",(int)work_D[0],(int)work_D[1]);
        }
        else if (work_type == TYPE_D)
        {
            time1 = MPI_Wtime();
            rc = ADLB_Get_reserved(work_D,work_handle);
            if (rc < 0)
            {
                aprintf(1,"**** GET RESERVED FAILED rc %d\n",rc);
                ADLB_Abort(-1);
            }
            total_get_time += MPI_Wtime() - time1;
            total_num_gets++;
            do_work_for_secs(time_for_D_work);
            time1 = MPI_Wtime();
            rc = ADLB_Put(work_D,20*sizeof(int),answer_rank,my_app_rank,
                          TYPE_D_ANSWER,priority_D_ANSWER); 
            aprintf(1111,"PUT D ANSWER %d %d for %d\n",(int)work_D[0],(int)work_D[1],answer_rank);
            total_put_time += MPI_Wtime() - time1;
            total_num_puts++;
            // aprintf(1111,"HANDLED D VAL %d %d\n",work_D[0],work_D[1]);
        }
        else
        {
            aprintf(1,"** UNKNOWN TYPE FOUND %d\n",work_type);
            ADLB_Abort(-1);
        }
    }
    return MPI_SUCCESS;
}

void do_work_for_secs(double num_secs)
{
    double time1, tempval = 99.99;

    time1 = MPI_Wtime();
    while ((MPI_Wtime() - time1) < num_secs)
        tempval = sqrt(tempval + 50000.0) + 1.0;
}
