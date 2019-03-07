/* A reasonable nqueens program that supports options:
 *     -n          board size
 *     -q          quiet: do NOT keep or print solutions
 *     -1          just compute first solution and stop
 *     -maxdfp     max depth for puts (will not do puts 
 *                 for sub-problems "deeper" than this
 *                 i.e. in columns further across the board;
 *                 note that if you set this value high, then
 *                 you stress the system; if higher than the
 *                 boardsize, then essentially every sub-problem
 *                 gets Put into the work pool
 * The program Puts data into the work with priority equal to the
 * column so that longer sub-problems have higher priority; favors
 * a depth-first search.  Solutions (if kept) are placed in the pool
 * with highest priority and targeted for rank 0.  Rank 0 does not
 * participate in the computation but merely collects the solutions
 * or count of solutions when they are not kept and printed.
 * This implies that you must have at least 3 ranks:
 *     server
 *     master
 *     worker
 *
 * When you are searching for all solutions, the program runs an extra
 * 5 seconds because that is the amount of time that adlb waits after
 * all ranks are on the rq before it declares done by exhaustion.
 *
 * When you are just searching for one solution, you may find it right
 * away, but the program may wait an extra 30 seconds to terminate.  This
 * is because one of the ranks may be going down a very deep branch that
 * could actually take several minutes.  But, I have a call in here every
 * 30 seconds to the procedure named ADLBP_Info_num_work_units which is
 * merely to see if any other ranks have called ADLB_Set_problem_done.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "adlb.h"

#define WORK                  1000
#define SOLUTION              2000
#define QUIET_SOLUTION_COUNT  3000

int safe_position(int, int, int *);
int nqbranch(int *);
void worker(void);

int num_world_nodes, my_world_rank, num_servers, num_workers, max_depth_for_puts;
int nput_probs = 0;
int nput_solns = 0;
int nprobs_handled = 0;
int nqueens = 4;
int just_one_solution = 0;
int quiet = 0;


double start_time, end_time;

int safe_position(int col, int row, int rows[])
{
    int i;

    for (i=0; i < col; i++)
        if (((rows[i]+i) == (col+row)) || (i-rows[i] == (col-row)) || (rows[i] == row))
            return 0;
    return 1;
}

double prev_time_nmw_checked = 0.0;

int nqbranch(int *wq_node)
{
    int i, j, opencol, nsolns, rc;
    int max_prio, num_max_prio, num_type;  /* for Info call */
    double time1, time2;

    nsolns = 0;
    nprobs_handled++;
    opencol = nqueens;
    for (i=0; i < nqueens && opencol >= nqueens; i++)
        if (wq_node[i] < 0)
            opencol = i;
    if (opencol <= max_depth_for_puts)
    {
        for (i=0; i < nqueens; i++)
        {
            if ( safe_position(opencol,i,wq_node))
            {
                nput_probs++;
                wq_node[opencol] = i;  /* overlay the -1 */
                rc = ADLB_Put(wq_node,nqueens*sizeof(int),-1,my_world_rank,WORK,opencol); 
                wq_node[opencol] = -1;  /* restore */
                if (rc == ADLB_NO_MORE_WORK)
                    return rc;
            }
        }
    }
    else
    {
        for (i=0; i < nqueens; i++)
        {
            if ( safe_position(opencol,i,wq_node))
            {
                if (opencol == (nqueens-1))  /* filled last pos */
                {
                    nsolns++;
                    if ( ! quiet)
                    {
                        nput_solns++;
                        wq_node[opencol] = i;  /* overlay the -1 */
                        rc = ADLB_Put(wq_node,nqueens*sizeof(int),0,my_world_rank,SOLUTION,999); 
                        wq_node[opencol] = -1;  /* restore */
                        if (rc == ADLB_NO_MORE_WORK)
                            return rc;
                    }
                    if (just_one_solution)
                        return 1;
                }
                else
                {
                    wq_node[opencol] = i;  /* overlay the -1 */
                    rc = nqbranch(wq_node);
                    wq_node[opencol] = -1;  /* restore */
                    if (rc == ADLB_NO_MORE_WORK)
                        return rc;
                    nsolns += rc;
                    if ((MPI_Wtime()-prev_time_nmw_checked) > 30.0)
                    {
                        /* dummy query just to check for no more work */
                        rc = ADLBP_Info_num_work_units(WORK,&max_prio,&num_max_prio,&num_type);
                        if (rc == ADLB_NO_MORE_WORK)
                            return rc;
                        prev_time_nmw_checked = MPI_Wtime();
                    }
                }
            }
        }
    }
    return nsolns;
}


int main(int argc, char *argv[])
{
    int i, j, rc, *wq_node;
    int am_server, use_debug_server, am_debug_server, req_types[3];
    int work_type, work_len, work_prio, work_handle[ADLB_HANDLE_SIZE], answer_rank;
    int num_types = 3, types[4] = { WORK, SOLUTION, QUIET_SOLUTION_COUNT };
    int num_reserves, num_total_solutions, sum;
    double time1, time2, max_nqb_time = 0.0, max_rsrv_time = 0.0;
    MPI_Comm app_comm;

    num_servers = 1;          /* default */
    max_depth_for_puts = -1;  /* default */
    num_total_solutions = 0;
    use_debug_server = 0;
    for (i=1; i < argc; i++)
    {        
        if (strcmp(argv[i],"-q") == 0)
            quiet = 1;
        else if (strcmp(argv[i],"-1") == 0)
            just_one_solution = 1;
        else if (strcmp(argv[i],"-n") == 0)
            nqueens = atoi(argv[++i]);
        else if (strcmp(argv[i],"-nservers") == 0)
            num_servers = atoi(argv[++i]);
        else if (strcmp(argv[i],"-maxdfp") == 0)
            max_depth_for_puts = atoi(argv[++i]);
        else if (strcmp(argv[i],"-debug") == 0)
            use_debug_server = atoi(argv[++i]);
    }

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_nodes);
    if (num_world_nodes < 2)
    {
        printf("** must have at least 3 ranks: one master, one worker, one server \n");
        exit(-1);
    }
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);
    rc = ADLB_Init(num_servers,use_debug_server,1,num_types,types,
                   &am_server,&am_debug_server,&app_comm);
    if (am_server)
    {
        ADLB_Server(25000000,(double)0.0);
        ADLB_Finalize();
        MPI_Finalize();
        exit(0);
    }
    else if (am_debug_server)
    {
        aprintf(1,"BECOMING DEBUG SERVER\n");
        ADLB_Debug_server(300.0);
        ADLB_Finalize();
        MPI_Finalize();
        exit(0);
    }
    num_workers = num_world_nodes - num_servers;
    if (use_debug_server)
        num_workers--;
    aprintf(1111,"NUMS %d NUMW %d\n",num_servers,num_workers);

    wq_node = malloc(nqueens*sizeof(int));

    if (my_world_rank == 0)
    {
        for (i=0; i < nqueens; i++)
        {
            for (j=0; j < nqueens; j++)
                wq_node[j] = -1;
            wq_node[0] = i;  /* first element of the row */
            rc = ADLB_Put(wq_node,nqueens*sizeof(int),-1,my_world_rank,WORK,1); 
        }
        if (quiet)
            req_types[0] = QUIET_SOLUTION_COUNT;
        else
            req_types[0] = SOLUTION;
        req_types[1] = -1;
    }
    else
    {
        req_types[0] = WORK;
        req_types[1] = -1;
    }
    num_reserves = 0;

    if (max_depth_for_puts < 0)
    {
        max_depth_for_puts = nqueens;  /* default */
        for (i=0,j=nqueens-1,sum=nqueens; i < nqueens; i++,j--)
        {
            sum = sum + (sum * j);
            if (sum > num_workers)
            {
                max_depth_for_puts = i + 2;  /* 2 -> pad by 1 extra ? */
                break;
            }
        }
    }
    aprintf(1111,"max_depth_for_puts %d  i %d  sum %d\n",max_depth_for_puts,i,sum);

    start_time = MPI_Wtime();

    while ( 1 )
    {
        time1 = MPI_Wtime();
        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                          &work_len,&answer_rank);
        time2 = MPI_Wtime();
        if ((time2-time1) > 5.0)
            aprintf(1111,"LONG RESERVE: %f\n",time2-time1);
        if (rc == ADLB_NO_MORE_WORK)
        {
            aprintf(1,"GOT NO_MORE_WORK\n");
            break;
        }
        else if (rc == ADLB_DONE_BY_EXHAUSTION)
        {
            aprintf(1,"GOT EXHAUSTION\n");
            break;
        }
        else if (rc < 0)
        {
            aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
            exit(-1);
        }
        if ((time2-time1) > max_rsrv_time)
            max_rsrv_time = time2 - time1;
        num_reserves++;
        rc = ADLB_Get_reserved(wq_node,work_handle);
        if (rc == ADLB_NO_MORE_WORK)
        {
            aprintf(1,"GOT NO_MORE_WORK\n");
            break;
        }
        else if (rc == ADLB_DONE_BY_EXHAUSTION)
        {
            aprintf(1,"GOT EXHAUSTION\n");
            break;
        }
        else if (rc < 0)
        {
            aprintf(1,"**** RESERVE FAILED rc %d\n",rc);
            exit(-1);
        }
        if (work_type == SOLUTION)
        {
            num_total_solutions++;
            if ( ! quiet)
            {
                for (i=0; i < nqueens; i++)
                    printf("%d ",wq_node[i]);
                printf("\n");
            }
            if (just_one_solution)
                ADLB_Set_problem_done();
        }
        else if (work_type == QUIET_SOLUTION_COUNT)
        {
            num_total_solutions += wq_node[0];
            if (num_total_solutions >= 1  &&  just_one_solution)
                ADLB_Set_problem_done();
        }
        else if (work_type == WORK)
        {
            time1 = MPI_Wtime();
            rc = nqbranch(wq_node);
            if (rc == ADLB_NO_MORE_WORK)
                break;
            time2 = MPI_Wtime();
            if ((time2-time1) > max_nqb_time)
                max_nqb_time = time2 - time1;
            aprintf(0000,"NQB TIME %f  cnt %d  rows %d %d %d %d %d %d\n",
                    time2-time1,rc,wq_node[0],wq_node[1],wq_node[2],
                    wq_node[3],wq_node[4],wq_node[5]);
            if (quiet)
            {
                wq_node[0] = rc;  /* num solns */
                rc = ADLB_Put(wq_node,nqueens*sizeof(int),0,my_world_rank,
                              QUIET_SOLUTION_COUNT,999); 
                if (rc == ADLB_NO_MORE_WORK)
                    break;
            }
        }
        else
        {
            aprintf(1111,"** calling abort: UNKNOWN work type %d\n",work_type);
            ADLB_Abort(-1);
        }
    }
    aprintf(1111,"MAX_TIMES: nqb %f  rsrv %f\n",max_nqb_time,max_rsrv_time);

    end_time = MPI_Wtime();

    if (my_world_rank == 0)
        printf("found %d solutions, time %f\n",num_total_solutions,end_time-start_time);
    printf("nprobs_handled %d  nput_probs %d  nput_solns %d  num_reserves %d\n",
           nprobs_handled,nput_probs,nput_solns,num_reserves);
    ADLB_Finalize();
    MPI_Finalize();
    return 0;
}
