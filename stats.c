/* This program defines a library for accumulating basic statistics on
   the fly.  That is, values of the variable being recorded are added to
   a structure as they are generated, and a running min, max, mean, and
   sample standard deviation are continuously updated, and can be
   inquired for at any time. The values are assumed to be doubles.  The
   algorithm for computing the sample standard deviation incrementally
   is taken from the book "Accracy and Stability of Numerical
   Algorithms", by Nicholas Higham, pp. 12-13.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* the statistics accumulation object */
#define MAXLEN 80
typedef struct {
  double sum;			/* running sum */
  double min;			/* running minimum */
  double max;			/* running maximum */
  double mean;			/* running average */
  double stddev;		/* running standard deviation */
  double q;			/* intermediate value for computation */
  double m;			/* intermediate value */
  int    numvals;		/* number of values contributed so far */
  int    active;		/* whether timer is on (1) or off (0) */
  char   name[MAXLEN];		/* name of statistics structure */
} statstruct, *statsptr;


/* initialization and contribution functions */
statsptr statsinit(char *);            /* allocate and initialize a stats structure */
void     statsfinalize(statsptr);      /* deallocate stats structure */
int      statsenter(statsptr, double); /* contribute a value to a stats structure */

/* turning statistics collection on and off, and resetting (reinitializing without reallocating) */
void   statson(statsptr);
void   statsoff(statsptr);
void   statsreset(statsptr);

/* access functions */
char  *name(statsptr);
int    numvals(statsptr);
double sum(statsptr);
double min(statsptr);
double max(statsptr);
double mean(statsptr);
double stddev(statsptr);

/* Debugging function */
int statsdump(statsptr);

/* test program for the stats library */
int main(int argc, char *argv[])
{
  statsptr p1, p2;
  double a;
  int i;
 
  printf("initializing...");

  p1 = statsinit("timer 1");  statson(p1);
  p2 = statsinit("timer 2");  statson(p2);

  printf("initialized\n");

  statsdump(p1);
  statsenter(p1, 1.0);
  statsdump(p1);
  statsenter(p2, 101.0);
  statsenter(p1, 2.0);
  statsdump(p1);
  statsenter(p2, 102.0);
  statsenter(p1, 3.0);
  statsdump(p1);
  statsenter(p2, 103.0);

  statsoff(p1);

  statsenter(p1, 1000000.0);	/* should not be counted */

  printf("\ntesting dump function:\n");
  statsdump(p1);
  printf("\n");
  statsdump(p2);

  printf("\ntesting accessor functions (for %s):\n", p1->name);

  printf("name = %s\n",              name(p1));
  printf("number of values = %d\n",  numvals(p1));
  printf("sum of values = %f\n",     sum(p1));
  printf("max of values = %f\n",     max(p1));
  printf("min of values = %f\n",     min(p1));
  printf("average of values = %f\n", mean(p1));
  printf("std dev of values = %f\n", stddev(p1));

  statsreset(p1); statson(p1);
  for (a = 0.0; a < 1000.0; a = a + 1.0)
    statsenter(p1, a);

  statsreset(p2); statson(p2);
  for (i = 0; i < 1000; i++)
    statsenter(p2, 500.0);

  statsdump(p1);
  statsdump(p2);

  statsfinalize(p1);
  statsfinalize(p2);

  return(0);
}

statsptr statsinit(char *name)
{
  statsptr p;
  
  p = malloc(sizeof(statstruct));
  strncpy(p->name, name, MAXLEN);
  p->numvals = 0;
  p->active  = 0;		/* note initialized to "off" */
  p->sum = 0.0;
  p->min = 100000000.0;
  p->max = -1.0;
  p->m   = 0.0;
  p->q   = 0.0;

  return(p);
}

void statsreset(statsptr p)
{
  p->numvals = 0;
  p->active  = 0;		/* note initialized or reset to "off" */
  p->sum = 0.0;
  p->min = 100000000.0;
  p->max = -1.0;
  p->m   = 0.0;
  p->q   = 0.0;
}

void statsfinalize(statsptr p)
{
  free(p);
}

/* turn stats collection on and off*/
void statson(statsptr p)  {p->active = 1;}
void statsoff(statsptr p) {p->active = 0;}

/* contribute a value to a stats structure */
int statsenter(statsptr p, double newval)
{
  if (p->active == 1) {
    p->numvals++;
    p->sum += newval;
    p->mean = p->sum / p->numvals;
    if (newval < p->min)
      p->min = newval;
    if (newval > p->max)
      p->max = newval;
    /* sample standard deviation computation from Higham, p. 12 */
    if (p->numvals == 1) {
      p->q = 0;
      p->m = newval;
    } else {
      p->q = p->q + (p->numvals - 1) * (newval - p->m) * (newval - p->m) / p->numvals;
      p->m = p->m + (newval - p->m) / p->numvals;
    }
  }
  return(0);
}

/* accessor functions */
char  *name(statsptr p)    {return p->name;}
int    numvals(statsptr p) {return p->numvals;}
double sum(statsptr p)     {return p->sum;}
double min(statsptr p)     {return p->min;}
double max(statsptr p)     {return p->max;}
double mean(statsptr p)    {return p->mean;}
double stddev(statsptr p)  {return (p->numvals == 1 ? 0 : sqrt((p->q)/(p->numvals - 1)));}

/* debugging function */
int statsdump(statsptr p)
{
  printf("name = %s\n", p->name);
  printf("status = %s\n", p->active ? "on" : "off");
  if (p->numvals == -1)
    printf("not initialized\n");
  else if (p->numvals == 0)
    printf("no data\n");
  else {
    printf("numvals = %d, sum = %f\n", p->numvals, p->sum);
    printf("max = %f, min = %f, avg = %f\n",
	   p->max, p->min, p->mean);
    printf("q = %f, m = %f, stddev = %f\n",
	   p->q, p->m, (p->numvals == 1 ? 0 : sqrt((p->q)/(p->numvals - 1))));
  }
  return(0);
}

