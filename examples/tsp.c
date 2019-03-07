#include <stdio.h>

/* This program seems to get fairly good parallelism if you have one
 * server per worker.  The real problem is the scheme for termination.
 * There is not a good method for telling when to stop.
 */

#include "adlb.h"

#define WORK_TYPE        1
#define WORK_PRIO        1
#define BOUND_UPDT       2
#define BOUND_UPDT_PRIO  999999999     /* higher than any work */

int num_cities, rtlen, *dists_vect, **dists;
int num_servers, num_world_ranks, my_world_rank, num_app_ranks, my_app_rank;
int am_server, num_types, req_types[4], bound_dist, *bound_path;
int parent_app_rank, lchild_app_rank, rchild_app_rank, pool_size;

double malloc_hwm, avg_time_on_rq, start_time, end_time, temp_time;


int main(int argc, char *argv[])
{
    int i, j, rc, done, *msg_to_0, *updt_buf, *work_buf;
    int work_type, work_len, work_prio, work_handle[ADLB_HANDLE_SIZE], answer_rank;
    int len_partial_path, *partial_path, rhs_rank, cidx, city_already_in_path;
    int nput_work, new_len, dist, temp_bsf_dist, *temp_bsf_path, fss, nrecvd;
    int aprintf_flag, use_debug_server, am_debug_server;
    MPI_Comm app_comm;

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_ranks);
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);
    if (num_world_ranks < 2)
    {
        printf("** require 2 ranks:  server, worker\n");
        exit(-1);
    }

    num_servers = 1;
    fss = 0;  /* full search space; 0 -> trim, i.e. do NOT search full space */
    use_debug_server = 0;  /* default */
    aprintf_flag = 1;
    for (i=1; i < argc; i++)
    {
        if (strcmp(argv[i],"-nservers") == 0)
            num_servers = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-fss") == 0)
            fss = atoi(argv[i+1]);
        else if (strcmp(argv[i],"-debug") == 0)
            use_debug_server = atoi(argv[i+1]);
    }

    num_types = 2;
    req_types[0] = BOUND_UPDT;
    req_types[1] = WORK_TYPE;
    rc = ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,req_types,
                   &am_server,&am_debug_server,&app_comm);
    if ( ! am_server  &&  ! am_debug_server)
    {
        MPI_Comm_size(app_comm,&num_app_ranks);
        MPI_Comm_rank(app_comm,&my_app_rank);
    }

    rc = MPI_Barrier(MPI_COMM_WORLD);

    if (am_server)
    {
        ADLB_Server(30000000,0.0);
        rc = ADLB_Info_get(ADLB_INFO_MALLOC_HWM,&malloc_hwm);
        rc = ADLB_Info_get(ADLB_INFO_AVG_TIME_ON_RQ,&avg_time_on_rq);
        printf("MALLOC_HWM %.0f  AVG_TIME_ON_RQ %f\n",malloc_hwm,avg_time_on_rq);
    }
    else if (am_debug_server)
    {
        aprintf(1,"BECOMING DEBUG SERVER\n");
        ADLB_Debug_server(300.0);
    }
    else
    {
        if (my_app_rank == 0)
        {
            aprintf(1,"reading input from stdin ... \n");
            scanf("%d",&num_cities);
        }
        MPI_Bcast(&num_cities,1,MPI_INT,0,app_comm);
        rtlen = num_cities + 1;  /* goes back to city 0 */
        dists_vect = malloc(num_cities * num_cities * sizeof(int));
        dists = malloc(num_cities * sizeof(int *));
        for (i=0; i < num_cities; i++)
            dists[i] = dists_vect + (i * num_cities);
        if (my_app_rank == 0)
        {
            for (i=0; i < num_cities; i++)
            {
                for (j=0; j < num_cities; j++)
                {
                    scanf("%d",&dists[i][j]);
                    if (dists[i][j] == 0)
                        dists[i][j] = 999999999;
                    // printf("%d ",dists[i][j]);
                }
                // printf("\n");
            }
        }

        bound_path    = malloc(rtlen     * sizeof(int));
        partial_path  = malloc(rtlen     * sizeof(int));
        temp_bsf_path = malloc(rtlen     * sizeof(int));
        msg_to_0      = malloc((rtlen+1) * sizeof(int));
        work_buf      = malloc((rtlen+1) * sizeof(int));
        updt_buf      = work_buf;

        if (my_app_rank == 0)  /* do this before sync at next bcast */
        {
            work_buf[0] = 1;  /* len of problem */
            work_buf[1] = 0;  /* only one city in initial problem */
            rc = ADLB_Put(work_buf,(rtlen+1)*sizeof(int),-1,my_app_rank,WORK_TYPE,WORK_PRIO); 
        }
        MPI_Bcast(dists_vect,num_cities*num_cities,MPI_INT,0,app_comm);

        bound_dist = 0;
        for (i=0; i < num_cities; i++)
        {
            bound_path[i] = i;
            if (i < (num_cities-1))
                bound_dist += dists[i][i+1];
        }
        bound_path[num_cities] = 0;
        bound_dist += dists[num_cities-1][0];

        if (my_app_rank == (num_app_ranks-1))
            rhs_rank = 0;
        else
            rhs_rank = my_app_rank + 1;
        if (my_app_rank == 0)
            parent_app_rank = -1;
        else
            parent_app_rank = (my_app_rank - 1) / 2;
        lchild_app_rank = (my_app_rank * 2) + 1;
        if (lchild_app_rank > (num_app_ranks - 1))
            lchild_app_rank = -1;
        rchild_app_rank = (my_app_rank * 2) + 2;
        if (rchild_app_rank > (num_app_ranks - 1))
            rchild_app_rank = -1;

        start_time = MPI_Wtime();
    
        nput_work = 0;
        while (1)
        {
            req_types[0] = BOUND_UPDT;
            req_types[1] = WORK_TYPE; 
            req_types[2] = -1;
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                              &work_len,&answer_rank);
            if (rc == ADLB_NO_MORE_WORK)
                break;
            else if (rc == ADLB_DONE_BY_EXHAUSTION)
                break;
            if (rc < 0)
            {
                aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
                exit(-1);
            }
            rc = ADLB_Get_reserved(work_buf,work_handle);  /* updt_buf is same addr */
            if (rc == ADLB_NO_MORE_WORK)
                break;
            else if (rc == ADLB_DONE_BY_EXHAUSTION)
                break;
            if (rc < 0)
            {
                aprintf(1,"**** GET_RESERVED FAILED rc %d\n",rc);
                exit(-1);
            }
            // aprintf(1111,"GOT %d\n",work_type);
            if (work_type == BOUND_UPDT)
            {
                if (updt_buf[0] < bound_dist)
                {
                    bound_dist = updt_buf[0];
                    memcpy(bound_path,&updt_buf[1],rtlen*sizeof(int));
                    if (lchild_app_rank >= 0)
                        rc = ADLB_Put(updt_buf,(rtlen+1)*sizeof(int),lchild_app_rank,
                                      my_app_rank,BOUND_UPDT,BOUND_UPDT_PRIO); 
                    if (rchild_app_rank >= 0)
                        rc = ADLB_Put(updt_buf,(rtlen+1)*sizeof(int),rchild_app_rank,
                                      my_app_rank,BOUND_UPDT,BOUND_UPDT_PRIO); 
                }
            }
            else if (work_type == WORK_TYPE)
            {
                // aprintf(1111,"GOT work lenpart %d  bsf %d\n",work_buf[0],bound_dist);
                ADLB_Begin_batch_put( NULL,0 );
                temp_bsf_dist = bound_dist;
                len_partial_path = work_buf[0];
                partial_path = &work_buf[1];
                for (cidx=1; cidx < num_cities; cidx++)  /* skip 0, start city */
                {
                    city_already_in_path = 0;
                    for (j=1; j < len_partial_path; j++)    /* start at 1 */
                    {
                        if (cidx == partial_path[j])
                        {
                            city_already_in_path = 1;
                            break;
                        }
                    }
                    if ( ! city_already_in_path)
                    {
                        partial_path[len_partial_path] = cidx;
                        new_len = len_partial_path + 1;
                        if (new_len == num_cities)  /* all but back to 0 */
                        {
                            dist = 0;
                            for (i=0; i < (new_len-1); i++)   /* one less hop than ends */
                                dist += dists[partial_path[i]][partial_path[i+1]];
                            dist += dists[partial_path[i]][0];  /* back to 0 */
                            if (dist < temp_bsf_dist)
                            {
                                temp_bsf_dist = dist;
                                memcpy(temp_bsf_path,partial_path,new_len*sizeof(int));
                                temp_bsf_path[new_len] = 0;  /* back to 0 */
                            }
                        }
                        else
                        {
                            dist = 0;
                            for (i=0; i < (new_len-1); i++)   /* one less hop than ends */
                                dist += dists[partial_path[i]][partial_path[i+1]];
                            if (fss  ||  dist < bound_dist)  /* better new distance */
                            {
                                work_buf[0] = new_len;
                                /* heuristic: bump prio to favor longer partial paths */
                                rc = ADLB_Put(work_buf,(rtlen+1)*sizeof(int),-1,
                                              my_app_rank,WORK_TYPE,WORK_PRIO+new_len); 
                                nput_work++;
                            }
                        }
                    }
                }
                if ( ! fss  &&  temp_bsf_dist < bound_dist)
                {
                    msg_to_0[0] = temp_bsf_dist;
                    memcpy(&msg_to_0[1],temp_bsf_path,rtlen*sizeof(int));
                    rc = ADLB_Put(msg_to_0,(rtlen+1)*sizeof(int),0,my_app_rank,
                                  BOUND_UPDT,BOUND_UPDT_PRIO); 
                }
                ADLB_End_batch_put();
            }
        }
        aprintf(11,"NPUT %d\n",nput_work);
    }

    if (my_world_rank == 0)
    {
        printf("bdist %d\n",bound_dist);
        printf("bpath ");
        for (i=0; i < rtlen; i++)
            printf("%d ",bound_path[i]);
        printf("\n");
        ADLB_Set_problem_done();
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
