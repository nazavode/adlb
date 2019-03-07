/*
    This sample program is a demo of using adlb to parallelize
    an embarrassingly parallel algorithm:
        Markov Chain Monte Carlo of the hard disk (sphere) problem
    The master rank merely does Puts of ints to be used as seeds
    for the random number generator.  Each worker rank retrieves a
    seed and then uses it to do a large number of Markov Chain
    computations attempting valid movements of the hard disks.
    At the end of each such computation, the workers report the
    final positions of the disks as a "solution" to the problem.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "adlb.h"

#define NUMDISKS 4
#define WORK 1
#define SOLN 2

int rand_int_min_max(int min, int max)
{
    int temp1, temp2, rint;

    rint = random();
    rint = min + (int) (max-min+1)*(rint/(RAND_MAX+1.0));
    return rint;
}

double rand_dbl() 
{
    double rdbl;

    rdbl = (double)random() / ((double)RAND_MAX+1.0);
    return rdbl;
}

double rand_dbl_min_max(double min, double max) 
{ 
    double rdbl;

    rdbl = min + ((max-min) * rand_dbl());     
    return rdbl;     
} 

double dist( double x1, double y1, double x2, double y2)
{
    double d;

    d = sqrt( pow((x1-x2),2.0) + pow((y1-y2),2.0) );
    return d;
}

struct point {
    double x,y;
} points[NUMDISKS];

int main(int argc, char *argv[])
{
    int i, rc, choice, goodB, pidx, randcnt, num_mcs, am_server, am_debug_server,
        num_world_nodes, num_servers, my_world_rank, rint;
    int answer_rank, work_type, work_prio, work_len, work_handle[ADLB_HANDLE_SIZE];
    int num_types = 2, types[2] = { WORK, SOLN }, req_types[3];
    double sigma, sigmaSquared, delta, bx, by, rand1, rand2;
    double disk_vector[NUMDISKS*2];
    MPI_Comm app_comm;

    num_servers = 0;
    num_mcs = 0;
    for (i=1; i < argc; i++)
    {        
        if (strcmp(argv[i],"-nmcs") == 0)
            num_mcs = atoi(argv[++i]);
        else if (strcmp(argv[i],"-nservers") == 0)
            num_servers = atoi(argv[++i]);
    }

    rc = MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&num_world_nodes);
    if (num_world_nodes < 3)
    {
        printf("** require at least 3 ranks; one server, master, worker \n");
        exit(-1);
    }
    if ( ! num_servers)
        num_servers = 1;
    if ( ! num_mcs)
        num_mcs = num_world_nodes - num_servers - 1;
    MPI_Comm_rank(MPI_COMM_WORLD,&my_world_rank);
    rc = ADLB_Init(num_servers,0,1,num_types,types,&am_server,&am_debug_server,&app_comm);
    if (am_server)
    {
        ADLB_Server(5000000,(double)0.0);
        ADLB_Finalize();
        MPI_Finalize();
        exit(0);
    }
    if (my_world_rank == 0)
    {
        for (randcnt=0; randcnt < num_mcs; randcnt++)
        {
            rint = randcnt + 100;
            rc = ADLB_Put(&rint,sizeof(int),-1,my_world_rank,WORK,1); 
        }
        for (randcnt=0; randcnt < num_mcs; randcnt++)
        {
            req_types[0] = SOLN;
            req_types[1] = -1;
            rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                              &work_len,&answer_rank);
            if (rc < 0  ||  work_type != SOLN)
            {
                aprintf(1,"**** RESERVE FAILED rc %d  type %d\n",rc,work_type);
                exit(-1);
            }
            rc = ADLB_Get_reserved(disk_vector,work_handle);
            for (i=0; i < NUMDISKS*2; i+=2)
                printf("(%12.8f,%12.8f) ",disk_vector[i+0],disk_vector[i+1]);
            printf("\n");
        }
        aprintf(1,"********** SETTING NO MORE WORK ***********************************\n");
        ADLB_Set_problem_done();
        ADLB_Finalize();
        MPI_Finalize();
        exit(0);
    }

    /* worker ranks */
    while (1)
    {
        req_types[0] = WORK;
        req_types[1] = -1;
        rc = ADLB_Reserve(req_types,&work_type,&work_prio,work_handle,
                          &work_len,&answer_rank);
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
        if (work_type != WORK)
        {
            aprintf(1,"OOPS: invalid type %d\n",work_type);
            exit(-1);
        }
        rc = ADLB_Get_reserved(&rint,work_handle);
        srandom(rint);
        sigma = 0.20;
        sigmaSquared = sigma * sigma;
        delta = 0.15;
        /* currently hardcoded just for NUMDISKS = 4 */
        points[0].x = 0.25;
        points[0].y = 0.25;
        points[1].x = 0.75;
        points[1].y = 0.25;
        points[2].x = 0.25;
        points[2].y = 0.75;
        points[3].x = 0.75;
        points[3].y = 0.75;
        for (i=0; i < 1000000; i++)
        {
            choice = rand_int_min_max(0,NUMDISKS-1);
            rand1 = rand_dbl_min_max(-1*delta,delta);
            rand2 = rand_dbl_min_max(-1*delta,delta);
            bx = points[choice].x + rand1;
            by = points[choice].y + rand2;
            // printf("choice %d  px %f  rand1 %f  bx %f\n",choice,points[choice].x,rand1,bx);
            if (bx < sigma  ||  bx > (1-sigma)  ||  by < sigma  ||  by > (1-sigma))
                goodB = 0;  /* false */
            else
            {
                goodB = 1;  /* true unless chgd below */
                for (pidx=0; pidx < NUMDISKS; pidx++)
                {
                    if (dist(points[pidx].x,points[pidx].y,bx,by) < sigmaSquared)
                    {
                        goodB = 0;  /* false */
                        break;
                    }
                }
            }
            if (goodB)
            {
                points[choice].x = bx;
                points[choice].y = by;
            }
        }
        for (i=0; i < NUMDISKS*2; i+=2)
        {
            disk_vector[i+0] = points[i/2].x;
            disk_vector[i+1] = points[i/2].y;
            // aprintf(1111,"DEBUG %12.8f %12.8f \n",disk_vector[i+0],disk_vector[i+1]);
        }
        rc = ADLB_Put(disk_vector,NUMDISKS*2*sizeof(double),0,my_world_rank,SOLN,2); 
    }
    ADLB_Finalize();
    MPI_Finalize();
    return 0;
}
