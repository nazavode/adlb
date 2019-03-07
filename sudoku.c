#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "adlb.h"

#define BOARD       1
#define SOLUTION    2

#define BOARD_PRIORITY    1
#define SOLUTION_PRIORITY 2

#define MASTER_RANK 0

int num_types = 2;
int type_vect[3] = {BOARD, SOLUTION};

double start_time, end_time;

/* board 3 from the list, not one of the hardest */

char input_board[81] = "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....";


char digits[9] = "123456789";

int row[9][9];
int col[9][9];
int box[9][9];

int myrow( int index )
{
  return index / 9;
}

int mycol( int index )
{
  return index % 9;
}

int mybox( int index )
{
  int i,j;
  
  i = myrow( index ) / 3;
  j = mycol( index ) / 3;
  return( i * 3 + j );
}

int inbox( char c, int b, char inboard[81] )
{
  int i;

  for ( i = 0; i < 9; i++ ) {
    if ( c == inboard[box[b][i]] )
      return( 1 );
  }
  return( 0 );
}

int inrow( char c, int b, char inboard[81] )
{
  int i;

  for ( i = 0; i < 9; i++ ) {
    if ( c == inboard[row[b][i]] )
      return( 1 );
  }
  return( 0 );
}

int incol( char c, int b, char inboard[81] )
{
  int i;

  for ( i = 0; i < 9; i++ ) {
    if ( c == inboard[col[b][i]] )
      return( 1 );
  }
  return( 0 );
}

/* if we need these, add board as input parameter, since not global
void printrow( int id )
{
  int index;
  for ( index = 0; index < 9; index++ )
    putchar( board[row[id][index]] );
  putchar( '\n' );
}

void printcol( int id )
{
  int index;
  for ( index = 0; index < 9; index++ )
    putchar( board[col[id][index]] );
  putchar( '\n' );
}

void printbox( int id )
{
  int index;
  for ( index = 0; index < 9; index++ )
    putchar( board[box[id][index]] );
  putchar( '\n' );
}

*/

void printboard( char * board )
{
  int i;

  for ( i = 0; i < 81; i++ ) {
    putchar( ' ' );
    putchar( board[i] );
    if ( (i+1) % 9 == 0 )
      putchar( '\n' );
  }
} 

int init( char * board )
{
  int i,j,m,n,corner,start,k,index;

  /*
  printf( "problem board:\n" );
  printboard( board );
  */
  for (i = 0; i < 9; i++) {
    for (j = 0; j < 9; j++) {
      row[i][j] = i*9 + j;
      col[i][j] = i + j*9;
    }
  }
  k = 0;  /* runs through the box numbers */
  for (m = 0; m < 3; m++) {
    for (n = 0; n < 3; n++) {
      corner = m*27 + n*3;
      index = 0;  /* runs through the indexes in a box */
      start = corner;
      for (i = 0; i < 3; i++) {
	for (j = 0; j < 3; j++) {
	  box[k][index++] = start + j;
	}
	start = start + 9;
      }
      k++; /* done a box */
    }
  }
}

int findempty( char * board )
{
  int i;

  for ( i = 0; i < 81; i++ ) {
    if ( board[i] == '.' )
      return( i );
  }
  return( -1 );
}

int countfills( char * board )
{
  int i, count;
  
  count = 0;
  for ( i = 0; i < 81; i++ ) {
    if ( board[i] != '.' )
      count++;
  }
  return count;
}


void copyboard( char * board, char * newboard )
{
  int i;

  for (i = 0; i < 81; i++)
    newboard[i] = board[i];
}


int main(int argc, char *argv[])
{
  int i, j, k, count, rc, am_server, done;
  int num_servers, num_slaves, num_world_nodes, my_world_rank, aprintf_flag;
  int my_app_rank, num_app_nodes, num_done = 0, total_done;
  int req_types[4], work_type, answer_rank, work_len, num_delay, time_unit_this_A;
  int work_prio, orig_rank, work_handle[ADLB_HANDLE_SIZE], num_C_answers, msg_available;
  int use_debug_server, am_debug_server;
  double malloc_hwm, avg_time_on_rq, periodic_log_interval;
  char c, board[81], newboard[81];
  MPI_Comm app_comm;
  MPI_Status status;

  /* defaults for command-line arguments */
  num_servers = 1;
  use_debug_server = 0; 
  aprintf_flag = 1;
  periodic_log_interval = 0.0;
  for ( i=1; i < argc; i++ ) {
    if ( strcmp(argv[i], "-nservers" ) == 0 )
      num_servers = atoi( argv[i+1] );
    else if ( strcmp(argv[i], "-debug" ) == 0 )
      use_debug_server = atoi( argv[i+1] );
    else if ( strcmp(argv[i], "-plog" ) == 0 )
      periodic_log_interval = atof( argv[i+1] );
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
    aprintf( 1, "World rank %d becoming ADLB server\n", my_world_rank );
    ADLB_Server( 3000000, periodic_log_interval );  
    rc = ADLB_Info_get( ADLB_INFO_MALLOC_HWM, &malloc_hwm );
    rc = ADLB_Info_get( ADLB_INFO_AVG_TIME_ON_RQ, &avg_time_on_rq );
    aprintf( 1, "MALLOC_HWM %.0f  AVG_TIME_ON_RQ %f\n", malloc_hwm, avg_time_on_rq );
  }
  else if ( am_debug_server ) {
    aprintf( 1, "World rank %d becoming debug server\n", my_world_rank );
    ADLB_Debug_server( 300.0 );
  }
  else {                                 /* application process */
    if ( my_app_rank == MASTER_RANK ) {  /* if master app */
      printf( "input problem board:\n" );
      printboard( input_board );
      count = countfills( input_board );
      aprintf( 1, "fills in input board = %d\n", count );
      rc = ADLB_Put( input_board, 81*sizeof(char), -1, -1, BOARD, count ); 
      aprintf( 1, "put input board, rc = %d\n", rc );
    }

    /* all application processes, including the master, execute this loop */
    done = 0;
    while ( !done ) {
      req_types[0] = -1;
      req_types[1] = req_types[2] = req_types[3] = -1;
      /* aprintf( 1, "Getting a board\n" ); */
      rc = ADLB_Reserve( req_types, &work_type, &work_prio, work_handle,
			 &work_len, &answer_rank); /* work handle is array, so no & */
      /* aprintf( 1, "rc from getting board = %d\n", rc ); */
      if ( rc == ADLB_NO_MORE_WORK ) {
	aprintf( 1, "No more work on reserve\n" );
	break;
      }
      else if (rc < 0) {
	aprintf( 1, "Reserve failed, rc = %d\n", rc );
	exit(-1);
      }
      else if ( work_type != BOARD) {
	aprintf( 1, "unexpected work type %d\n", work_type );
	ADLB_Abort( 99 );
      }
      else {			/* reserved good work */
	rc = ADLB_Get_reserved( board, work_handle );
	if (rc == ADLB_NO_MORE_WORK) {
	  aprintf( 1, "No more work on get_reserved\n" );
	  break;
	}
	else { 			/* got good work */
	  num_done++;
	  init( board );
	  k = findempty( board );
	  if ( k == -1 ) {
	    printf( "solution:\n" );
	    printboard( board );
	    ADLB_Set_no_more_work();
	    done = 1;
	  }
	  else {
	    /* aprintf( 1, "first blank at %d\n", k ); */
	    copyboard( board, newboard );
	    for ( i = 0; i < 9; i++ ) {
	      c = digits[i];
	      if ( !inbox( c, mybox( k ), newboard ) &&
		   !inrow( c, myrow( k ), newboard ) &&
		   !incol( c, mycol( k ), newboard ) ) {
		/* printf( "%c is a candidate at position %d\n", c , k ); */
		newboard[k] = c;
		count = countfills( newboard );
		rc = ADLB_Put( newboard, 81*sizeof(char), -1, -1, BOARD, count ); 
		/* aprintf( 1, "put a board, rc = %d\n", rc ); */
                if ( rc == ADLB_NO_MORE_WORK ) {
	          aprintf( 1, "No more work on put\n" );
                  done = 1;
	          break;  /* only breaks out of for loop */
                }
	      }
	      else {
		/*
		aprintf( 1, "no candidate at position %d\n", k );
		*/
	      }
	    }
	  }
	}
      }
    }
  }

  if ( !am_server )
    MPI_Reduce( &num_done, &total_done, 1, MPI_INT, MPI_SUM, 0, app_comm );
  if ( my_world_rank == MASTER_RANK ) {    /* if master */
    end_time = MPI_Wtime();
    printf( "total number of subproblems: %d\n", total_done );
    printf( "total time = %10.2f\n",end_time - start_time);
  }
  if ( my_world_rank == MASTER_RANK )    /* if master */
    aprintf(1,"AT ADLB_FINALIZE\n");
  ADLB_Finalize();
  if ( my_world_rank == MASTER_RANK )    /* if master */
    aprintf(1,"AT MPI_FINALIZE\n");
  MPI_Finalize();
  if ( my_world_rank == MASTER_RANK )    /* if master */
    aprintf(1, "DONE\n");
  if ( !am_server )
    printf( "number of subproblems done by rank %d:  %d\n", my_world_rank, num_done );
  return 0;

}
