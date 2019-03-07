#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adlb.h"

#define A_EPOCH   2
#define CS_PER_B  4

#define MASTER_RANK 0

#define TAG_B_ANSWER    1
#define TAG_C_ANSWER    2
#define TAG_B_CHANGE    3

#define TYPE_A      1
#define TYPE_B      2
#define TYPE_C      3

int num_types = 3;
int type_vect[3] = { TYPE_A, TYPE_B, TYPE_C };

double start_time, end_time;

int main(int argc, char *argv[])
{
    int i, j, rc, iv, sum, am_server;
    int num_time_units_per_A, num_servers, num_slaves, num_world_nodes, my_world_rank;
    int my_app_rank, num_app_nodes;
    int num_As, num_Bs, num_Cs, num_As_this_node;
    int priority_A = 0, priority_B = -2, priority_C = -1;
    int req_types[4], work_type, answer_rank, work_len, num_delay, time_unit_this_A;
    int work_prio, orig_rank, work_handle[ADLB_HANDLE_SIZE], num_C_answers, msg_available;
    int work_A[20], work_B[10], work_C[20];
    int use_debug_server, am_debug_server, aprintf_flag;
    double tempval;
    double malloc_hwm, avg_time_on_rq;
    MPI_Comm app_comm;
    MPI_Status status;

    num_delay = 6000000;
    num_time_units_per_A = A_EPOCH * 2;
    num_servers = 1;
    num_As = 4;
    use_debug_server = 0;  /* default */
    aprintf_flag = 1;
    for (i=1; i < argc; i++)
    {
        if (strcmp(argv[i],"-nunits") == 0)
            num_time_units_per_A = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-nas") == 0)
            num_As = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-nservers") == 0)
            num_servers = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-debug") == 0)
            use_debug_server = atoi(argv[i+1]);
    }
    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_nodes);
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);
    rc = ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,type_vect,
                   &am_server,&am_debug_server,&app_comm);
    if ( ! am_server  &&  ! am_debug_server)
    {
        MPI_Comm_size(app_comm,&num_app_nodes);
        MPI_Comm_rank(app_comm,&my_app_rank);
        num_slaves = num_app_nodes - 1;
    }

    rc = MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    if (am_server)
    {
        ADLB_Server(3000000,(double)0.0);  /* 0.0 -> periodic_log_interval */
        rc = ADLB_Info_get(ADLB_INFO_MALLOC_HWM,&malloc_hwm);
        rc = ADLB_Info_get(ADLB_INFO_AVG_TIME_ON_RQ,&avg_time_on_rq);
        printf("MALLOC_HWM %.0f  AVG_TIME_ON_RQ %f\n",malloc_hwm,avg_time_on_rq);
    }
    else if (am_debug_server)
    {
        aprintf(1,"BECOMING DEBUG SERVER\n");
        ADLB_Debug_server(300.0);
        ADLB_Finalize();
        MPI_Finalize();
        exit(0);
    }
    else if (my_app_rank == MASTER_RANK)  /* if master */
    {
        sum = 0;
        num_Bs = num_As * (num_time_units_per_A / A_EPOCH); //  one B per A per time_units_per_A/A_EPOCH
        for (i=0; i < num_Bs; i++)
        {
            aprintf(1,"waiting for mpi_recv; i=%d to %d\n",i,num_Bs-1);
            rc = MPI_Recv(&iv,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,app_comm,&status);
            if (status.MPI_TAG == TAG_B_ANSWER)
            {
                sum += iv;
                aprintf(1,"past mpi_recv; i=%d iv=%d sum=%d\n",i,iv,sum);
            }
            else if (status.MPI_TAG == TAG_B_CHANGE)
            {
                // the number of Bs chgd
                // do I run a new A thru all 120 steps or just go from curr time_unit?
                //    iv should tell me how many more/fewer TAG_B_ANSWERs to expect
            }
            else
            {
                aprintf(1,"**** unexpected msg received: type %d from wrank %d\n",
                        status.MPI_TAG,status.MPI_SOURCE);
            }
        }
        aprintf(1,"********** SETTING NO MORE WORK ***********************************\n");
        ADLB_Set_problem_done();
        aprintf(1,"expected sum = %d\n",num_As*(num_time_units_per_A/A_EPOCH)*CS_PER_B);
        aprintf(1,"done:    sum = %d\n",sum);
    }
    else  /* slave */
    {
        /* put "my" As in the pool with priority_A */
        if (num_As >= num_slaves)
        {
            num_As_this_node = (num_As / num_slaves);
            j = num_As - (num_As_this_node * num_slaves);
            if (j)
                if (my_app_rank <= j)
                    num_As_this_node++;
        }
        else
        {
            if (my_app_rank >= 1  &&  my_app_rank <= num_As)
                num_As_this_node = 1;
            else
                num_As_this_node = 0;
        }
        aprintf(1,"NUM_AS_THIS_NODE=%d\n",num_As_this_node);
        ADLB_Begin_batch_put( NULL, 0 );
        for (i=0; i < num_As_this_node; i++)
        {
            memset(work_A,0,20*sizeof(int));    // an A is 20 doubles
            work_A[0] = (int)(my_world_rank);   // id of A is orig rank plus
            work_A[1] = (int)(i+1);             //    a unique Aid at that rank
            work_A[2] = (int)1;               // starting time_unit for this A
            rc = ADLB_Put(work_A,20*sizeof(int),-1,my_app_rank,TYPE_A,priority_A); 
            aprintf(1,"PUT A VAL %d %d\n",(int)work_A[0],(int)work_A[1]);
        }
        ADLB_End_batch_put();
        while (1)
        {
            req_types[0] = -1;
            req_types[1] = req_types[2] = req_types[3] = -1;
            aprintf(1111,"AT RESERVE\n");
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                              &work_len,&answer_rank);
            aprintf(1111,"PAST RESERVE rc %d\n",rc);
            if (rc == ADLB_NO_MORE_WORK)
            {
                aprintf(1,"GOT NO_MORE_WORK x1\n");
                break;
            }
            if (rc < 0)
            {
                aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                exit(-1);
            }
            if (work_type == TYPE_A)
            {
                rc = ADLB_Get_reserved(work_A,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    aprintf(1,"GOT NO_MORE_WORK x2\n");
                    break;
                }
                tempval = (double)work_A[0];
                for (i=0; i < num_delay; i++)
                    tempval = sqrt(tempval + 5000000.0) + 1;
                time_unit_this_A = (int)work_A[2];
                aprintf(1,"HANDLING A VAL %d %d  TIME %d\n",(int)work_A[0],(int)work_A[1],time_unit_this_A);
                if (time_unit_this_A % A_EPOCH == 0  &&  time_unit_this_A <= num_time_units_per_A)
                {
                    memset(work_B,0,10*sizeof(int));    // a B is 10 doubles
                    work_B[0] = work_A[0];    // a B is identified by the A whence it comes
                    work_B[1] = work_A[1];    //     including both rank and Aid
                    aprintf(1111,"RMB: C1 DBG\n");
                    rc = ADLB_Put(work_B,10*sizeof(int),-1,my_app_rank,TYPE_B,priority_B); 
                    aprintf(1,"PUT B VAL %d %d\n",(int)work_B[0],(int)work_B[1]);
                    priority_B = priority_A - 2;  // priority_B -= 10;  // ?? priority of A -2
                }
                if (time_unit_this_A < num_time_units_per_A)
                {
                    work_A[2] = (int) (time_unit_this_A + 1);
                    priority_A -= 3;  // -= 10;
                    aprintf(1111,"RMB: C1 DBG\n");
                    rc = ADLB_Put(work_A,20*sizeof(int),-1,my_app_rank,TYPE_A,priority_A); 
                    aprintf(1,"PUT A VAL %d %d\n",(int)work_A[0],(int)work_A[1]);
                }
                aprintf(1,"HANDLED A VAL %d %d  TIME %d\n",(int)work_A[0],(int)work_A[1],time_unit_this_A);
            }
            else if (work_type == TYPE_B)
            {
                rc = ADLB_Get_reserved(work_B,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    aprintf(1,"GOT NO_MORE_WORK x3\n");
                    break;
                }
                aprintf(1,"HANDLING B VAL %d %d\n",(int)work_B[0],(int)work_B[1]);
                ADLB_Begin_batch_put( NULL, 0 );
                for (i=0; i < CS_PER_B; i++)
                {
                    memset(work_C,0,20*sizeof(int));    // an C is 20 doubles
                    work_C[0] = work_B[0];    // a C is identified by the B (A) whence it comes
                    work_C[1] = work_B[1];    //     including both rank and Aid
                    rc = ADLB_Put(work_C,20*sizeof(int),-1,my_app_rank,TYPE_C,priority_C); 
                    aprintf(1,"PUT C VAL %d %d\n",(int)work_C[0],(int)work_C[1]);
                    priority_C = priority_B + 1;  //  -= 10;  // ?? priority_B + 1  ??
                }
                ADLB_End_batch_put();
                sum = 0;
                num_C_answers = 0;
                while (num_C_answers < CS_PER_B)
                {
                    rc = MPI_Iprobe(MPI_ANY_SOURCE,TAG_C_ANSWER,app_comm,&msg_available,&status);
                    if (msg_available)
                    {
                        rc = MPI_Recv(&iv,1,MPI_INT,MPI_ANY_SOURCE,TAG_C_ANSWER,app_comm,&status);
                        sum += iv;
                        num_C_answers++;
                        aprintf(1111,"PTA numCanswers %d\n",num_C_answers);
                    }
                    else
                    {
                        req_types[0] = TYPE_C;
                        req_types[1] = req_types[2] = req_types[3] = -1;
                        rc = ADLB_Ireserve(req_types,&work_type,&work_prio,work_handle,
                                           &work_len,&answer_rank);
			aprintf(1111,"RMB: NOHANG GOT RC %d\n",rc);
                        if (rc == ADLB_NO_MORE_WORK)
                        {
                            aprintf(1,"GOT NO_MORE_WORK x4\n");
                            break;
                        }
                        if (rc > 0)
                        {
                            rc = ADLB_Get_reserved(work_C,work_handle);
                            if (rc == ADLB_NO_MORE_WORK)
                            {
                                aprintf(1,"GOT NO_MORE_WORK x5\n");
                                break;
                            }
                            aprintf(1,"aHANDLING C VAL %d %d\n",(int)work_C[0],(int)work_C[1]);
                            tempval = (double)work_C[0];
                            for (i=0; i < num_delay; i++)
                                tempval = sqrt(tempval + 5000000.0) + 1;
                            if (answer_rank == my_world_rank)
                            {
                                sum += 1;
                                num_C_answers++;
                            }
                            else
                            {
                                j = 1;
                                rc = MPI_Send(&j,1,MPI_INT,answer_rank,TAG_C_ANSWER,app_comm);
                            }
                            aprintf(1111,"PTB numCanswers %d\n",num_C_answers);
                            // aprintf(1,"aHANDLED C VAL %d %d\n",(int)work_C[0],(int)work_C[1]);
                        }
                        else
                        {
			    aprintf(1111,"RMB: DOING RECV\n");
                            rc = MPI_Recv(&iv,1,MPI_INT,MPI_ANY_SOURCE,TAG_C_ANSWER,app_comm,&status);
                            sum += iv;
                            num_C_answers++;
                            aprintf(1111,"PTC numCanswers %d\n",num_C_answers);
                        }
                    }
                }
                if (rc == ADLB_NO_MORE_WORK)
                    break;
                rc = MPI_Send(&sum,1,MPI_INT,MASTER_RANK,TAG_B_ANSWER,app_comm);
                aprintf(1,"HANDLED B VAL %d %d\n",(int)work_B[0],(int)work_B[1]);
            }
            else if (work_type == TYPE_C)
            {
                rc = ADLB_Get_reserved(work_C,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    aprintf(1,"GOT NO_MORE_WORK x6\n");
                    break;
                }
                if (rc < 0)
                {
                    aprintf(1,"FAILED GET_RESERVED\n");
                    exit(-1);
                }
                aprintf(1,"bHANDLING C VAL %d %d ar=%d\n",(int)work_C[0],(int)work_C[1],answer_rank);
                tempval = (double)work_C[0];
                for (i=0; i < num_delay; i++)
                    tempval = sqrt(tempval + 5000000.0) + 1;
                if (answer_rank == my_world_rank)
                {
                    sum += iv;
                    num_C_answers++;
                }
                else
                {
                    j = 1;
                    rc = MPI_Send(&j,1,MPI_INT,answer_rank,TAG_C_ANSWER,app_comm);
                }
                aprintf(1,"bHANDLED C VAL %d %d\n",(int)work_C[0],(int)work_C[1]);
            }
        }
    }  /* slave */

    if (my_world_rank == MASTER_RANK)  /* if master */
    {
        end_time = MPI_Wtime();
        aprintf(1,"total time = %10.2f\n",end_time-start_time);
    }
    aprintf(1,"AT ADLB_FINALIZE\n");
    ADLB_Finalize();
    aprintf(1,"AT MPI_FINALIZE\n");
    MPI_Finalize();
    printf("DONE\n");

    return 0;
}
