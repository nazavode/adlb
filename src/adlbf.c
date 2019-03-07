
#include "adlb_conf.h"

#if defined(F77_NAME_UPPER)
#define adlb_init_ ADLB_INIT
#define adlb_server_ ADLB_SERVER
#define adlb_put_ ADLD_PUT
#define adlb_reserve_ ADLB_RESERVE
#define adlb_ireserve_ ADLB_IRESERVE
#define adlb_get_reserved_ ADLB_GET_RESERVED
#define adlb_get_reserved__timed_ ADLB_GET_RESERVED_TIMED
#define adlb_set_no_more_work_ ADLB_SET_NO_MORE_WORK
#define adlb_set_problem_done_ ADLB_SET_PROBLEM_DONE
#define adlb_begin_batch_put_ ADLB_BEGIN_BATCH_PUT
#define adlb_begin_batch_put_2_ ADLB_BEGIN_BATCH_PUT_2
#define adlb_end_batch_put_ ADLB_END_BATCH_PUT
#define adlb_end_batch_put_2_ ADLB_END_BATCH_PUT_2
#define adlb_info_get_ ADLB_INFO_GET
#define adlb_info_num_work_units_ ADLB_INFO_NUM_WORK_UNITS
#define adlb_finalize_ ADLB_FINALIZE
#define adlb_debug_server_ ADLB_DEBUG_SERVER
#define adlb_abort_ ADLB_ABORT
#elif defined(F77_NAME_LOWER_2USCORE)
#define adlb_init_ adlb_init__
#define adlb_server_ adlb_server__
#define adlb_put_ adlb_put__
#define adlb_reserve_ adlb_reserve__
#define adlb_ireserve_ adlb_ireserve__
#define adlb_get_reserved_ adlb_get_reserved__
#define adlb_get_reserved_timed_ adlb_get_reserved_timed__
#define adlb_set_no_more_work_ adlb_set_no_more_work__
#define adlb_set_problem_done_ adlb_set_problem_done__
#define adlb_begin_batch_put_ adlb_begin_batch_put__
#define adlb_begin_batch_put_2_ adlb_begin_batch_put_2__
#define adlb_end_batch_put_ adlb_end_batch_put__
#define adlb_end_batch_put_2_ adlb_end_batch_put_2__
#define adlb_info_get_ adlb_info_get__
#define adlb_info_num_work_units_ adlb_info_num_work_units__
#define adlb_finalize_ adlb_finalize__
#define adlb_debug_server_ adlb_debug_server__
#define adlb_abort_ adlb_abort__
#elif defined(F77_NAME_LOWER)
#define adlb_init_ adlb_init
#define adlb_server_ adlb_server
#define adlb_put_ adlb_put
#define adlb_reserve_ adlb_reserve
#define adlb_ireserve_ adlb_ireserve
#define adlb_get_reserved_ adlb_get_reserved
#define adlb_get_reserved_timed_ adlb_get_reserved_timed
#define adlb_set_no_more_work_ adlb_set_no_more_work
#define adlb_set_problem_done_ adlb_set_problem_done
#define adlb_begin_batch_put_ adlb_begin_batch_put
#define adlb_begin_batch_put_2_ adlb_begin_batch_put_2
#define adlb_end_batch_put_ adlb_end_batch_put
#define adlb_end_batch_put_2_ adlb_end_batch_put_2
#define adlb_info_get_ adlb_info_get
#define adlb_info_num_work_units_ adlb_info_num_work_units
#define adlb_finalize_ adlb_finalize
#define adlb_debug_server_ adlb_debug_server
#define adlb_abort_ adlb_abort
#endif

#include "adlb.h"

void adlb_init_( int *num_servers, int *use_debug_server, int *aprintf_flag, int *ntypes,
                 int *type_vect, int *am_server, int *am_debug_server, MPI_Fint *app_comm, int *ierr );
void adlb_init_( int *num_servers, int *use_debug_server, int *aprintf_flag, int *ntypes,
                 int *type_vect, int *am_server, int *am_debug_server, MPI_Fint *app_comm, int *ierr )
{
    MPI_Comm app_comm_out;
    *ierr = ADLB_Init( *num_servers, *use_debug_server, *aprintf_flag, *ntypes, type_vect,
                       am_server, am_debug_server, &app_comm_out );
    *app_comm = MPI_Comm_c2f(app_comm_out);
}

void adlb_server_( double *hi_malloc, double *periodic_log_interval, int *ierr );
void adlb_server_( double *hi_malloc, double *periodic_log_interval, int *ierr )
{
    *ierr = ADLB_Server(*hi_malloc, *periodic_log_interval);
}

void adlb_put_( void *work_buf, int *work_len, int *reserve_rank, int *answer_rank,
                int *work_type, int *work_prio, int *ierr );
void adlb_put_( void *work_buf, int *work_len, int *reserve_rank, int *answer_rank,
                int *work_type, int *work_prio, int *ierr )
{
    *ierr = ADLB_Put( work_buf, *work_len, *reserve_rank, *answer_rank, *work_type, *work_prio);
}

void adlb_reserve_( int *req_types, int *work_type, int *work_prio, int *work_handle,
                    int *work_len, int *answer_rank, int *ierr);
void adlb_reserve_( int *req_types, int *work_type, int *work_prio, int *work_handle,
                    int *work_len, int *answer_rank, int *ierr)
{
    *ierr = ADLB_Reserve( req_types, work_type, work_prio, work_handle, work_len, answer_rank);
}

void adlb_ireserve_( int *req_types, int *work_type, int *work_prio, int *work_handle,
                     int *work_len, int *answer_rank, int *ierr);
void adlb_ireserve_( int *req_types, int *work_type, int *work_prio, int *work_handle,
                     int *work_len, int *answer_rank, int *ierr)
{
    *ierr = ADLB_Ireserve( req_types, work_type, work_prio, work_handle, work_len, answer_rank);
}

void adlb_get_reserved_( void *work_buf, int *work_handle, int *ierr);
void adlb_get_reserved_( void *work_buf, int *work_handle, int *ierr)
{
    *ierr = ADLB_Get_reserved( work_buf, work_handle );
}

void adlb_get_reserved_timed_( void *work_buf, int *work_handle, double *qtime, int *ierr);
void adlb_get_reserved_timed_( void *work_buf, int *work_handle, double *qtime, int *ierr)
{
    *ierr = ADLB_Get_reserved_timed( work_buf, work_handle, qtime );
}

void adlb_begin_batch_put_( void *common_buf, int *len_common, int *ierr );
void adlb_begin_batch_put_( void *common_buf, int *len_common, int *ierr )
{
    *ierr = ADLB_Begin_batch_put( common_buf, *len_common);
}

void adlb_end_batch_put_( int *ierr );
void adlb_end_batch_put_( int *ierr )
{
    *ierr = ADLB_End_batch_put();
}
void adlb_begin_batch_put_2_( void *common_buf, int *len_common, int *ierr );
void adlb_begin_batch_put_2_( void *common_buf, int *len_common, int *ierr )
{
    *ierr = ADLB_Begin_batch_put( common_buf, *len_common);
}

void adlb_end_batch_put_2_( int *ierr );
void adlb_end_batch_put_2_( int *ierr )
{
    *ierr = ADLB_End_batch_put();
}

void adlb_set_no_more_work_( int *ierr );  // deprecated
void adlb_set_no_more_work_( int *ierr )
{
    *ierr = ADLB_Set_no_more_work();
}

void adlb_set_problem_done_( int *ierr );
void adlb_set_problem_done_( int *ierr )
{
    *ierr = ADLB_Set_problem_done();
}

void adlb_info_get_( int *key, double *val, int *ierr );
void adlb_info_get_( int *key, double *val, int *ierr )
{
    *ierr = ADLB_Info_get( *key, val );
}

void adlb_info_num_work_units_(int *work_type, int *max_prio, int *num_max_prio_type,
                               int *num_type, int *ierr);
void adlb_info_num_work_units_(int *work_type, int *max_prio, int *num_max_prio_type,
                               int *num_type, int *ierr)
{
    *ierr = ADLB_Info_num_work_units(*work_type, max_prio, num_max_prio_type, num_type);
}

void adlb_finalize_( int *ierr );
void adlb_finalize_( int *ierr )
{
    *ierr = ADLB_Finalize();
}

void adlb_debug_server_( double *timeout, int *ierr );
void adlb_debug_server_( double *timeout, int *ierr )
{
    *ierr = ADLB_Debug_server(*timeout);
}

void adlb_abort_( int *code, int *ierr );
void adlb_abort_( int *code, int *ierr )
{
    *ierr = ADLB_Abort(*code);
}
