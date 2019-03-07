/* This program does yet not do what it is supposed to do at all */
/* The idea is to create a synthetic work unit that takes a specific amount of time. 
   Currently, once it is created, it doesn't atkethat amount of time whenexecuted.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

#define  LOOPLIMIT 100000	/* limits on triply-nested loop */

void do_work_for_secs(double);
void nugget();			/* indivisible unit of work */
int  define_work(double, int*, int*, int*);
int  do_work(int, int, int);

int numprocs, myrank;

int main(int argc, char *argv[])
{
  double spintime, starttime, endtime;
  int i, j, k;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

  if (myrank == 0) {
    if (argc < 2) {
      printf("usage: %s <secs>", argv[0]);
      exit(-1);
    }
      spintime = atof(argv[1]);
      printf("defining work unit for %f seconds\n", spintime);
  }
  MPI_Bcast(&spintime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  if (myrank == 0) {
    define_work(spintime, &i, &j, &k);
    /* at this point i, j, k define a unit of work that takes spintime seconds
       for a process to do */
  } else {
    sleep((int) spintime);
  }

  MPI_Bcast(&i, 1, MPI_INT, 0, MPI_COMM_WORLD); /* fix later to send array of 3 ints */
  MPI_Bcast(&j, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&k, 1, MPI_INT, 0, MPI_COMM_WORLD);

  if (myrank == 0)
    starttime = MPI_Wtime();
  MPI_Barrier(MPI_COMM_WORLD);
  do_work(i, j, k);
  MPI_Barrier(MPI_COMM_WORLD);
  if (myrank == 0)
    endtime = MPI_Wtime();

  MPI_Finalize();

  if (myrank == 0)
    printf("%d processes, time = %f seconds, speedup = %f\n",
	   numprocs,
	   endtime - starttime,
	   (numprocs * spintime) / (endtime - starttime));

  return(0);
}
      
int define_work(double time, int *pi, int *pj, int *pk)
{
  double starttime, endtime;
  int i, j, k;

  starttime = MPI_Wtime();
  for (i = 0; i < LOOPLIMIT; i++) {
    for (j = 0; j < LOOPLIMIT; j++) {
      for (k = 0; k < LOOPLIMIT; k++) {
	nugget();
	if (MPI_Wtime() - starttime >= time) 
	  break;
      }
      if (MPI_Wtime() - starttime >= time) 
      break;
    }
    if (MPI_Wtime() - starttime >= time) 
    break;
  }
  printf("%d: define_work, time = %f, i = %d, j = %d, k = %d\n", myrank, time, i, j, k);
  *pi = i; *pj = j, *pk = k;
}

int do_work(int i, int j, int k)
{
  int i1, j1, k1;
  double ignore, starttime, endtime;
    
  printf("%d: doing work (%d,%d,%d)\n", myrank, i, j, k);
  starttime = MPI_Wtime(); 
  for (i1 = 0; i1 <= i; i1++) {
    for (j1 = 0; j1 <= j; j1++) {
      for (k1 = 0; k1 <= k; k1++) {
	nugget();
	ignore = MPI_Wtime();  	/* to match what is done in define work */
      }
      ignore = MPI_Wtime(); 
    }
    ignore = MPI_Wtime();  	
  }
  endtime = MPI_Wtime();
  printf("%d: doing work took %f seconds\n", myrank, endtime - starttime);
  return(0);
}

void nugget()
{
  int i;
  double x;
  for (i = 0; i < 1000; i++) {
    x = sqrt(sqrt(sqrt((double) i) + sqrt((double) (i + 1))));
    x = sqrt(sqrt(sqrt((double) (i + 2) ) + sqrt((double) (i + 3))));
  }
}
