#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"  /* only used for timings */

#define ROWS 		1000
#define COLUMNS 	1000

struct globmem
{
    double a[ROWS+2][COLUMNS+2];
    double b[ROWS+2][COLUMNS+2];
    int st[ROWS+2], pq[ROWS+1];
    int pqbeg, pqend, niters, nproc, rows, ncols;
} *glob;

void work(char who);
double avggrid();
double avgbnd();

slave()
{
    work('s');
}

phi(int x,int y)			/* The function on the boundary */
{
      // return 1;
      return((x * x) - (y * y) + (x * y));   
}

main(int argc, char *argv[])
{
    int i;
    double timestart, timeend;
    double avg;

    if (argc < 3)
    {
	printf("usage: %s nrows ncols niters\n",argv[0]);
	exit(-1);
    }

    MPI_Init(&argc,&argv);  /* only used for timings */

    glob = (struct globmem *) malloc(sizeof(struct globmem));

    glob->rows = atoi(argv[1]);
    glob->ncols = atoi(argv[2]);
    glob->niters = atoi(argv[3]);

    printf("nrows\tncols\tniters\n");
    printf("%d \t  %d \t  %d \t\n",glob->rows,glob->ncols,glob->niters);

    gridinit(glob->a,glob->rows,glob->ncols);
    gridinit(glob->b,glob->rows,glob->ncols);

    /****
    printf("original grid:\n");
    printgrid(glob->a,glob->rows,glob->ncols);
    ****/
    
    glob->pqbeg = glob->pqend = 0;
    for (i=1; i <= glob->rows; i++)
        queueprob(i);

    /* initialize the status vector */
    for (i=0; i < (glob->rows+2); i++)
        glob->st[i] = 0;

    timestart = MPI_Wtime();
    work('m');
    timeend = MPI_Wtime();
    printf("\nruntime %f\n",timeend-timestart);

    /****
    printf("the resulting grid:\n");
    if (glob->niters % 2 == 0)
        printgrid(glob->a,glob->rows,glob->ncols);
    else
        printgrid(glob->b,glob->rows,glob->ncols);
    ****/

    if (glob->niters % 2 == 0)
        avg = avggrid(glob->a,glob->rows,glob->ncols);
    else
        avg = avggrid(glob->b,glob->rows,glob->ncols);
    printf("average value of grid = %f\n",avg);

    return 0;
}

/* "m" is the matrix, "r" is the number of rows of data (m[1]-m[r];
   m[0] and m[r+1] are boundaries), and "c" is the number of columns
   of data.
*/

gridinit(double m[ROWS+2][COLUMNS+2],int r,int c)
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
	    m[i][j] = 0;
}

queueprob(int x)
{
    glob->pq[glob->pqend] = x;
    glob->pqend = (glob->pqend + 1) % (ROWS + 1);
}

compute(double p[ROWS+2][COLUMNS+2],double q[ROWS+2][COLUMNS+2], int r, int ncols)
{
    int i, j;
    double stime;

    // printf("DOING ROW %d\n",r);
    for (j=1; j <= ncols; j++) 
    {
        stime = MPI_Wtime();
        do {
            q[r][j] = (p[r-1][j] + p[r+1][j] + p[r][j-1] + p[r][j+1]) / 4.0;
        } while (MPI_Wtime() - stime <= 0.001);  // artificially inflate the work time
        // printf("VAL %f from %f %f %f %f\n", q[r][j],p[r-1][j],p[r+1][j],p[r][j-1],p[r][j+1]);
    }
}

int putprob(int r)
{
    int qprob;

    qprob = 0;
    glob->st[r]++;
    if (r == 1)
        glob->st[0] = glob->st[r];
    else if (r == glob->rows)
        glob->st[glob->rows+1] = glob->st[r];

    if (glob->st[r] < glob->niters)
    {
        if ((r > 1) && (glob->st[r-2] >= glob->st[r]) 
		    && (glob->st[r-1] == glob->st[r]))
	{
            queueprob(r-1);
            qprob = 1;
        }
        if (r < glob->rows && glob->st[r+1] == glob->st[r] 
			   && glob->st[r+1] <= glob->st[r+2])
	{
            queueprob(r+1);
            qprob = 1;
        }
        if (glob->st[r-1] == glob->st[r] && 
	    glob->st[r] == glob->st[r+1])
	{
            queueprob(r);
            qprob = 1;
        }
    }
    if (qprob)
        return(1);		/* new problem */
    else
	return(0);		/* no new problem */
}

int getprob(int *v)
{
    int rc = 1;
    int *p = (int *) v;

    if (glob->pqbeg != glob->pqend) 
    {
	*p = glob->pq[glob->pqbeg];
	glob->pqbeg = (glob->pqbeg+1) % (ROWS + 1); 
	rc = 0;
    }
    return(rc);
}

void work(char who)			/* main routine for all processes */
{
    int r,rc,i;

    rc = getprob(&r);
    while (rc == 0)
    {
	if ((glob->st[r] % 2) == 0)
	    compute(glob->a,glob->b,r,glob->ncols);
	else 
	    compute(glob->b,glob->a,r,glob->ncols);
	putprob(r);
	rc = getprob(&r);
        /* printf("ITER %d  RC %d\n",(glob->st[r] % 2),rc); */
    }
}

printgrid(double m[ROWS+2][COLUMNS+2], int r, int c)
{
    int i,j;
    for (i = 0; i < (r+2); i++)
	for (j = 0; j < (c+2); j++)
	    printf("%3d %3d %10.5f\n",i,j,m[i][j]);
}

double avggrid(double m[ROWS+2][COLUMNS+2], int r, int c)
{
    int i,j;
    double avg = 0;

    for (i = 0; i < (r+2); i++)
	for (j = 0; j < (c+2); j++)
	    avg += m[i][j];
    return(avg/((r+2)*(c+2)));
}

double avgbnd(double m[ROWS+2][COLUMNS+2], int r, int c)
{
    int i,j;
    double avg = 0;

    for (i = 0; i < (r+2); i++)
	    avg += m[i][0];
    for (i = 0; i < (r+2); i++)
	    avg += m[i][c+1];
    for (i = 1; i < (c+1); i++)
	    avg += m[0][i];
    for (i = 1; i < (c+1); i++)
	    avg += m[r+1][i];
    return(avg/(2*(c+2) + 2*(r+2) - 4)); /* average over boundary */
}

