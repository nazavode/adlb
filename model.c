#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adlb.h"


#define MASTER_RANK 0
#define PROBLEM     1
#define SOLUTION    2
#define PROBLEM_PRIORITY 5

int num_types = 2;
int type_vect[2] = {PROBLEM, SOLUTION};

double start_time, end_time;


int main(int argc, char *argv[])
{
  int i, j, k, rc, am_server, done, numprobs;
  int aprintf_flag, problem;
  int num_servers, num_slaves, num_world_nodes, my_world_rank;
  int my_app_rank, num_app_nodes, num_done = 0;
  int req_types[4], work_type, answer_rank, work_len, num_delay, time_unit_this_A;
  int work_prio, orig_rank, work_handle[ADLB_HANDLE_SIZE];
  int use_debug_server, am_debug_server;
  double malloc_hwm, avg_time_on_rq, periodic_log_interval;
  MPI_Comm app_comm;
  MPI_Status status;

  /* defaults for command-line arguments */
  num_servers = 1;
  use_debug_server = 0; 
  aprintf_flag = 1;
  periodic_log_interval = 0.0;
  numprobs = 0;
  for ( i=1; i < argc; i++ ) {
    if ( strcmp( argv[i], "-nservers" ) == 0 )
      num_servers = atoi( argv[i+1] );
    else if ( strcmp( argv[i], "-debug" ) == 0 )
      use_debug_server = atoi( argv[i+1] );
    else if ( strcmp( argv[i], "-plog" ) == 0 )
      periodic_log_interval = atof( argv[i+1] );
    else if ( strcmp( argv[i], "-numprobs" ) == 0 )
      numprobs = atoi( argv[i+1] );
  }
  rc = MPI_Init( &argc, &argv );
  MPI_Comm_size( MPI_COMM_WORLD, &num_world_nodes );
  MPI_Comm_rank( MPI_COMM_WORLD, &my_world_rank );
  
  rc = ADLB_Init( num_servers, use_debug_server, aprintf_flag, num_types, type_vect,
		  &am_server, &am_debug_server, &app_comm);
  if ( !am_server && !am_debug_server ) /* application process */
  {
    MPI_Comm_size( app_comm, &num_app_nodes );
    MPI_Comm_rank( app_comm, &my_app_rank );
    num_slaves = num_app_nodes - 1;
  }

  rc = MPI_Barrier( MPI_COMM_WORLD );
  start_time = MPI_Wtime();

  if ( am_server ) {
    aprintf( 0, "World rank %d becoming ADLB server\n", my_world_rank );
    ADLB_Server( 3000000, periodic_log_interval );  
    rc = ADLB_Info_get( ADLB_INFO_MALLOC_HWM, &malloc_hwm );
    rc = ADLB_Info_get( ADLB_INFO_AVG_TIME_ON_RQ, &avg_time_on_rq );
    aprintf( 0, "MALLOC_HWM %.0f  AVG_TIME_ON_RQ %f\n", malloc_hwm, avg_time_on_rq );
  }
  else if ( am_debug_server ) {
    aprintf( 1, "World rank %d becoming debug server\n", my_world_rank );
    ADLB_Debug_server( 300.0 );
  }
  else {          /* application process */
    if ( my_app_rank == MASTER_RANK ) {  /* if master app */
      for ( i = 0; i < numprobs; i++ ) {
	rc = ADLB_Put( &i, sizeof(int), -1, -1, PROBLEM, PROBLEM_PRIORITY );
	aprintf( 1, "rc from Put = %d\n", rc );
      }
    }
    MPI_Barrier( app_comm );		/* to give process 0 a chance */
    /* all application processes, including the master, execute this loop */
    done = 0;
    while ( !done ) {
      req_types[0] = -1;
      req_types[1] = req_types[2] = req_types[3] = -1;
      aprintf( 1, "Getting a problem\n" );
      rc = ADLB_Reserve( req_types, &work_type, &work_prio, work_handle,
			 &work_len, &answer_rank); /* work handle is array, so no & */
      aprintf( 1, "rc from getting problem = %d\n", rc );
      if ( rc == ADLB_NO_MORE_WORK ) {
	aprintf( 1, "No more work on reserve\n" );
	break;
      }
      else if (rc == ADLB_DONE_BY_EXHAUSTION) {
	aprintf( 1, "Reserve got exhaustion rc\n" );
	break;
      }
      else if ( work_type != PROBLEM) {
	aprintf( 1, "unexpected work type %d\n", work_type );
	ADLB_Abort( 99 );
      }
      else {			/* reserved good work */
	rc = ADLB_Get_reserved( &problem, work_handle );
	if (rc == ADLB_NO_MORE_WORK) {
	  aprintf( 1, "No more work on get_reserved\n" );
	  break;
	}
	else { 			/* got good work */
	  sleep( 1 );		/* dummy work */
	  num_done++;
	}
      }
    }
  }

  aprintf( 1, "num problems done = %d\n", num_done );
  if ( my_world_rank == MASTER_RANK ) {    /* if master */
    end_time = MPI_Wtime();
    aprintf(1,"total time = %10.2f\n",end_time-start_time);
  }
  if ( my_world_rank == MASTER_RANK )    /* if master */
    aprintf(1,"AT ADLB_FINALIZE\n");
  ADLB_Finalize();
  if ( my_world_rank == MASTER_RANK )    /* if master */
    aprintf(1,"AT MPI_FINALIZE\n");
  MPI_Finalize();
  if ( my_world_rank == MASTER_RANK )    /* if master */
    printf("DONE\n");
  return 0;
}
