/* This version of grid_daf makes sure that each row stays in lock step with
 * its neighbors, which requires more sync and slows things down a bit by
 * using rank 0 to make sure that all rows proceed at the same pace.  This
 * causes rank 0 to become a bit of a bottleneck.  This version also uses a
 * for-loop in compute to artificially inflate time to do work.
 * If the number of ranks working on the problem matches the number of rows,
 * then rank 0 will probably not have to wait long at the end of each iter
 * to start the next one.
 */

#include "adlb.h"

void printgrid(double **,int,int);
double avggrid(double **,int,int);
double avgbnd(double **,int,int);
void work(int);

double phi(int x, int y)    /* The function on the boundary */
{
    // return 1;   
    return ((x * x) - (y * y) + (x * y));   
}

int nrows, ncols, niters, size_1_row, size_3_rows, size_1_prob;
int am_debug_server, use_debug_server, aprintf_flag;
int num_types = 2, type_vect[2] = { 00, 99 };
double avg, start_time, end_time, *agrid_contig, **agrid,
       *prob_contig, **prob_three_rows, *prob_row_idx, *prob_row_iter;

int main(int argc, char *argv[])
{
    int i, rc;
    int num_servers, am_server, num_total_ranks, num_app_ranks, my_app_rank;
    MPI_Comm app_comm;

    if (argc < 3)
    {
	printf("usage: %s nrows ncols niters\n",argv[0]);
	exit(-1);
    }

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_total_ranks);
    if (num_total_ranks < 2)
    {
        printf("**** this app needs at least 2 processes: server, worker\n");
        exit(-1);
    }

    num_servers = 1;
    nrows = atoi(argv[1]);
    ncols = atoi(argv[2]);
    niters = atoi(argv[3]);

    use_debug_server = 0;
    aprintf_flag = 1;
    rc = ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,type_vect,
                   &am_server,&am_debug_server,&app_comm);

    if ( ! am_server)
    {
        MPI_Comm_size(app_comm,&num_app_ranks);
        MPI_Comm_rank(app_comm,&my_app_rank);
    }

    rc = MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    if (am_server)
    {
        ADLB_Server(3000000,(double)0.0);  /* 0.0 -> periodic_log_interval */
    }
    else
    {
        for (i=1; i < argc; i++)
        {
            if (strcmp(argv[i],"-nrows") == 0)
                nrows = atoi(argv[i+1]);
            else if (strcmp(argv[i],"-ncols") == 0)
                ncols = atoi(argv[i+1]);
            else if (strcmp(argv[i],"-niters") == 0)
                niters = atoi(argv[i+1]);
        }
        agrid_contig = malloc((nrows+2) * (ncols+2) * sizeof(double));
        agrid = malloc((nrows+2) * sizeof(double *));
        for (i=0; i < nrows+2; i++)
            agrid[i] = &(agrid_contig[i*(ncols+2)]);
        gridinit(agrid,nrows,ncols);
        size_1_row  = 1 * (ncols+2) * sizeof(double);
        size_3_rows = 3 * (ncols+2) * sizeof(double);
        size_1_prob = size_3_rows + 2 * sizeof(double);  /* plus row idx and row iter */
        prob_contig = malloc(size_1_prob);
	prob_three_rows = malloc(3 * sizeof(double *));
	for (i=0; i < 3; i++)
	    prob_three_rows[i] = &(prob_contig[i*(ncols+2)]);
        prob_row_idx  = &(prob_contig[3*(ncols+2)+0]);
        prob_row_iter = &(prob_contig[3*(ncols+2)+1]);
        
        if (my_app_rank == 0)
        {
            ADLB_Begin_batch_put( NULL, 0 );
            for (i=1; i <= nrows; i++)
            {
                memcpy(prob_contig,&(agrid_contig[(i-1)*(ncols+2)]),size_3_rows);
                *prob_row_idx  = (double) i;
                *prob_row_iter = (double) 1;
                rc = ADLB_Put(prob_contig,size_1_prob,-1,my_app_rank,00,00); 
                aprintf(0000,"PUT row %d %d\n",i,(int)*prob_row_iter);
            }
            ADLB_End_batch_put();
            printf("\nnproc\tniters\tnrows\tncols\n");
            printf("%d \t  %d \t  %d \t  %d \n", num_app_ranks,niters,nrows,ncols);
            /****
            printf("original grid:\n");
            printgrid(agrid,nrows,ncols);
            ****/

            work(my_app_rank);

            /****
            printf("resulting grid:\n");
            printgrid(agrid,nrows,ncols);
            ****/
            avg = avggrid(agrid,nrows,ncols);
            printf("average value of grid = %f\n",avg);
        }
        else
            work(my_app_rank);
    }

    end_time = MPI_Wtime();
    printf("total time = %10.2f\n",end_time-start_time);

    ADLB_Finalize();
    MPI_Finalize();
    return 0;
}


/*
    "m" is the matrix, "r" is the number of rows of data
    (m[1]-m[r]; m[0] and m[r+1] are boundaries),
    and "c" is the number of columns of data
*/

int gridinit(double *m[], int r, int c)
{
    int i, j;
    double bndavg;
    
    for (j=0; j < (c + 2); j++)
    {
        m[0][j] = phi(1,j+1);
        m[r+1][j]= phi(r+2,j+1);
    }
    for (i=1; i < (r + 2); i++)
    {
        m[i][0] = phi(i+1,1);
        m[i][c+1] = phi(i+1,c+2);
    }
    bndavg = avgbnd(m,r,c);
    printf("boundary average = %f\n",bndavg);

    /* initialize the interior of the grids to the average over the boundary*/
    for (i=1; i <= r; i++)
        for (j=1; j <= c; j++)
            /* m[i][j] = bndavg; this optimization hinders debugging */
            m[i][j] = 0.0;
}

void compute(double *p[], double *q[], int rp, int rq, int ncols)
{
    int i = 0, j;
    double stime;

    aprintf(0000,"DOING ROW %d  %d\n",rq,rp);
    for (j=1; j <= ncols; j++) 
    {
        stime = MPI_Wtime();
        do {
            q[rq][j] = (p[rp-1][j] + p[rp+1][j] + p[rp][j-1] + p[rp][j+1]) / 4.0;
            i++;
        } while (MPI_Wtime() - stime <= 0.001);  // artificially inflate the work time
        aprintf(0000,"VAL %f from %f %f %f %f\n",
                q[rq][j],p[rp-1][j],p[rp+1][j],p[rp][j-1],p[rp][j+1]);
    }
}

void work(int my_rank)
{
    int i, rc, done, work_type, work_prio, work_handle[ADLB_HANDLE_SIZE], work_len,
        answer_rank, req_types[4], num_rows_done_this_iter = 0, num_rows_done = 0;
    double wqtime;

    done = 0;
    while ( ! done)
    {
        req_types[0] = -1;
        req_types[1] = req_types[2] = req_types[3] = -1;
        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,&work_len,&answer_rank);
        if (rc == ADLB_NO_MORE_WORK)
            break;
        // rc = ADLB_Get_reserved(prob_contig,work_handle);
        rc = ADLB_Get_reserved_timed(prob_contig,work_handle,&wqtime);
        // rc = ADLB_Get_reserved(prob_contig,work_handle);
        // printf("WQTIME %f\n",wqtime);
        if (rc == ADLB_NO_MORE_WORK)
            break;
        if (work_type == 99)    /* should only come to rank 0 */
        {
            aprintf(0000,"FINISHED ROW %d\n",(int)*prob_row_idx);
            memcpy(agrid[(int)*prob_row_idx],prob_three_rows[1],size_1_row);  /* just one row */
            *prob_row_iter += 1.0;
            if ((int)*prob_row_iter > niters)
                num_rows_done++;
            if (num_rows_done >= nrows)
            {
                ADLB_Set_no_more_work();
            }
            else
            {
                num_rows_done_this_iter++;
                if (num_rows_done_this_iter >= nrows)
                {
                    num_rows_done_this_iter = 0;
                    for (i=1; i <= nrows; i++)
                    {
                        *prob_row_idx = (double) i;
                        memcpy(prob_contig,&(agrid_contig[((int)(i-1))*(ncols+2)]),size_3_rows);
                        rc = ADLB_Put(prob_contig,size_1_prob,-1,0,00,00); 
                        aprintf(0000,"PUT row %d %d\n",i,(int)*prob_row_iter);
                    }
                }
            }
        }
        else
        {
            compute(prob_three_rows,agrid,1,(int)*prob_row_idx,ncols);
            memcpy(prob_contig,&(agrid_contig[((int)(*prob_row_idx-1))*(ncols+2)]),size_3_rows);
            rc = ADLB_Put(prob_contig,size_1_prob,0,0,99,99); 
        }
    }
}

void printgrid(double *m[], int r, int c)
{
    int i, j;

    for (i=0; i < (r+2); i++)
        for (j=0; j < (c+2); j++)
            printf("%3d %3d %10.5f\n",i,j,m[i][j]);
}

double avggrid(double *m[], int r, int c)
{
    int i,j;
    double avg = 0.0;

    for (i = 0; i < (r+2); i++)
        for (j = 0; j < (c+2); j++)
            avg += m[i][j];
    return (avg/((r+2)*(c+2)));
}

double avgbnd(double *m[], int r, int c)
{
    int i,j;
    double avg = 0.0;

    for (i = 0; i < (r+2); i++)
        avg += m[i][0];
    for (i = 0; i < (r+2); i++)
        avg += m[i][c+1];
    for (i = 1; i < (c+1); i++)
        avg += m[0][i];
    for (i = 1; i < (c+1); i++)
        avg += m[r+1][i];
    return (avg/(2*(c+2) + 2*(r+2) - 4)); /* average over boundary */
}
