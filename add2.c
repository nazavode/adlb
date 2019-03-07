#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adlb.h"

#define MAXLEN 1024
#define TYPE_AB 1
#define TYPE_C  2

int c[MAXLEN], size;
int num_types = 2, type_vect[2] = {TYPE_AB,TYPE_C};

main(int argc, char *argv[])
{
    int i, j, rc, provided, my_world_rank, num_world_nodes, *num_added, aprintf_flag;
    int a, b, int3[3], work_type, worklen, dest_rank, answer_rank, done_cnt;
    int num_servers, am_server, work_handle[ADLB_HANDLE_SIZE], work_len, work_prio;
    int req_types[4], num_app_nodes, my_app_rank, use_debug_server,am_debug_server;
    double start_time, end_time;
    FILE *infile;
    MPI_Comm app_comm;

    num_servers = 1;
    use_debug_server = 0;
    aprintf_flag = 1;

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_nodes);
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);

    rc = ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,type_vect,
                   &am_server,&am_debug_server,&app_comm);

    if ( ! am_server)
    {
        MPI_Comm_size(app_comm,&num_app_nodes);
        MPI_Comm_rank(app_comm,&my_app_rank);
    }

    rc = MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    if (am_server)
    {
        ADLB_Server(3000000,(double)0.0);  /* 0.0 -> periodic_log_interval */
    }
    else if (my_app_rank == 0)  /* if master */
    {
        if (argc < 2)
        {
            printf("usage: add2 in_filename\n");
            exit(-1);
        }
        if ((infile = fopen(argv[1],"r")) == NULL)
        {
            printf("** unable to open file: %s\n",argv[1]);
            exit(-1);
        }
        num_added = (int*) malloc(num_world_nodes*sizeof(int));
        memset(num_added,0,num_world_nodes*sizeof(int));
        size = 0;
        ADLB_Begin_batch_put( NULL, 0 );
        while ((rc = fscanf(infile,"%d %d",&a,&b)) != EOF)
        {
            int3[0] = size;
            int3[1] = a;
            int3[2] = b;
            printf("PUT AB int3[1]=%d\n",int3[1]); fflush(stdout);
            rc = ADLB_Put(int3,3*sizeof(int),-1,my_app_rank,TYPE_AB,00); 
            size++;
            if (size > MAXLEN)
            {
                printf("** arrays are too large; maxlen=%d\n",MAXLEN);
                exit(-1);
            }
        }
        ADLB_End_batch_put();
    }

    if ( ! am_server)
    {
        rc = MPI_Barrier(app_comm);
        done_cnt = 0;
        while (1)
        {
            /* daskfor */
            req_types[0] = -1;
            req_types[1] = req_types[2] = req_types[3] = -1;
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,&work_len,&answer_rank);
            printf("RESERVED: rc %d  type %d\n",rc,work_type);
            if (rc == ADLB_NO_MORE_WORK)
                break;
            rc = ADLB_Get_reserved(int3,work_handle);
            if (rc == ADLB_NO_MORE_WORK)
                break;
            /****/

            if (work_type == TYPE_C)  /* only handled by rank 0 */
            {
                c[int3[0]] = int3[1];  /* int3[0] -> idx, int3[1] -> val */
                num_added[int3[2]]++;  /* int3[2] -> from_rank */
                done_cnt++;
                aprintf(1111,"RMB: SIZE %d  DONE_CNT %d\n",size,done_cnt);
                if (done_cnt >= size)
                    ADLB_Set_problem_done();
            }
            else
            {
                /* int3[0] -> idx ; leave it alone */
                int3[1] += int3[2];  /* int3[1 and 2] -> vals to add */
                int3[2] = my_world_rank;
                for (i=0; i < 200000000; i++)
                    ;  /* dummy work */
                rc = ADLB_Put(int3,3*sizeof(int),0,0,TYPE_C,99); 
                printf("PUT C: %d\n",int3[1]);
            }
        }
    }

    if (my_app_rank == 0)
    {
        for (i=0; i < size; ) 
        {
            for (j=0; (j < 8) && (i < size); j++)
                printf("%d\t", c[i++]);
            printf("\n");
        }
        for (i=0; i < num_app_nodes; i++)
            printf("%d added %d\n",i,num_added[i]);
    }
    ADLB_Finalize();
    printf("PAST ADLB_FINALIZE\n");
    MPI_Finalize();
    printf("PAST MPI_FINALIZE\n");

    return 0;
}
