#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adlb.h"

#define NUM_TYPES 8

int num_types = NUM_TYPES;

int types[NUM_TYPES];
int len[NUM_TYPES], prio[NUM_TYPES], delay[NUM_TYPES];
int num_servers, num_world_ranks, num_app_ranks, my_world_rank, my_app_rank;

int *buff1, *buff2, *buff3, *buff4;

double start_time, end_time;

int main(int argc, char *argv[])
{
    int i, j, rc, am_server, aprintf_flag;
    int req_types[4], work_type, answer_rank, work_len;
    int work_prio, orig_rank, work_handle[ADLB_HANDLE_SIZE], num_C_answers, msg_available;
    int use_debug_server, am_debug_server;
    double malloc_hwm, avg_time_on_rq;
    MPI_Comm app_comm;
    MPI_Status status;

    aprintf_flag = 1;
    use_debug_server = 0;
    for (i=0; i < num_types; i++)
    {
        types[i] = i+100;
        len[i] = 1000;
        prio[i] = 1;
        delay[i] = 6000000;
    }
    num_servers = 1;
    for (i=1; i < argc; i++)
    {
        if (strcmp(argv[i],"-nservers") == 0)
            num_servers = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-debug") == 0)
            use_debug_server = atoi(argv[i+1]);
    }

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_ranks);
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);
    rc = ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,types,
                   &am_server,&am_debug_server,&app_comm);
    if ( ! am_server  &&  ! am_debug_server)
    {
        MPI_Comm_size(app_comm,&num_app_ranks);
        MPI_Comm_rank(app_comm,&my_app_rank);
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
    else if (my_app_rank == 0)  /* if master */
    {
        buff1 = malloc(len[0]*sizeof(int));
        for (i=0; i < len[0]; i++)
            buff1[i] = 1;  /* just some value */
        buff2 = malloc(len[7]*sizeof(int));
        ADLB_Begin_batch_put( NULL, 0 );
        for (i=0; i < 999; i++)
        {
            /* put:  buff, len, reserve_rank, answer_rank, type, prio */
            rc = ADLB_Put(buff1,len[0]*sizeof(int),-1,my_app_rank,types[0],prio[0]); 
            if (rc < 0)
            {
                aprintf(1,"** put failed; rc %d\n",rc);
                exit(-1);
            }
        }
        ADLB_End_batch_put();
        req_types[0] = types[7];  /* use last type to indicate one work done */
        req_types[1] = req_types[2] = req_types[3] = -1;
        for (i=0; i < 999; i++)
        {
            aprintf(1111,"AT RESERVE\n");
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                              &work_len,&answer_rank);
            aprintf(1111,"PAST RESERVE rc %d\n",rc);
            rc = ADLB_Get_reserved(buff2,work_handle);
            aprintf(1111,"PAST GET rc %d\n",rc);
        }
        free(buff1);
        aprintf(1,"********** SETTING NO MORE WORK ***********************************\n");
        ADLB_Set_problem_done();
    }
    else  /* regular app rank */
    {
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
                aprintf(1,"GOT NO_MORE_WORK\n");
                break;
            }
            else if (rc < 0)
            {
                aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                exit(-1);
            }
            buff1 = malloc(work_len);
            if (work_type == types[0])
            {
                rc = ADLB_Get_reserved(buff1,work_handle);
                if (rc == ADLB_NO_MORE_WORK)
                {
                    aprintf(1,"GOT NO_MORE_WORK\n");
                    break;
                }
                buff2 = malloc(len[7]*sizeof(int));
                for (i=0; i < len[7]; i++) buff1[i] = 7;
                rc = ADLB_Put(buff2,len[7]*sizeof(int),0,my_app_rank,types[7],prio[7]); 
                if (rc == ADLB_NO_MORE_WORK)
                {
                    aprintf(1,"GOT NO_MORE_WORK\n");
                    break;
                }
                else if (rc < 0)
                {
                    aprintf(1,"** put failed; rc %d\n",rc);
                    exit(-1);
                }
                free(buff2);
            }
            else
            {
                aprintf(1,"** unexpected work type %d\n",work_type);
                exit(-1);
            }
            free(buff1);
        }
    }  /* slave */

    if (my_world_rank == 0)  /* if master */
    {
        end_time = MPI_Wtime();
        aprintf(1,"total time = %10.2f\n",end_time-start_time);
    }
    aprintf(1,"AT ADLB_FINALIZE\n");
    ADLB_Finalize();
    aprintf(1,"AT MPI_FINALIZE\n");
    MPI_Finalize();
    aprintf(1,"DONE\n");

    return 0;
}
