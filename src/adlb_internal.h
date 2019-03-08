#ifndef ADLB_ADLB_INTERNAL_H_INCLUDED
#define ADLB_ADLB_INTERNAL_H_INCLUDED

// Unclear how to forward declare MPI types,
// just include the proper header
#include <mpi.h>

void *dmalloc(int,const char *,int);
void dfree(void *,int,const char *,int);
void adlbp_dbgprintf(int flag, int linenum, char *fmt, ...);
int adlbp_Probe(int , int, MPI_Comm, MPI_Status *);  /* used in aldb.c and adlb_prof.c */

#define aprintf(flag,...) adlbp_dbgprintf(flag,__LINE__,__VA_ARGS__)

#define amalloc(nbytes)   dmalloc(nbytes,__FUNCTION__,__LINE__)

#define afree(ptr,nbytes) dfree(ptr,nbytes,__FUNCTION__,__LINE__)

#endif