#include "adlb.h"

/* we either log adlb internals or a guess at user state */
/* when guessing the user state, we assume that the user is processing
 * a piece of work of a given type if they have done a Get_reserved
 * for that type.  We also assume they do the Get_reserverd for the most
 * recent Reserve.
 */
/* #define LOG_ADLB_INTERNALS 1 */
/* #define LOG_GUESS_USER_STATE 1 */

#if defined( LOG_ADLB_INTERNALS ) || defined( LOG_GUESS_USER_STATE )
static int my_log_rank;
static int inita, initb, puta, putb, reservea, reserveb, ireservea, ireserveb,
           geta, getb, getat, getbt, nomoreworka, nomoreworkb,
           beginbatchputa, beginbatchputb, endbatchputa, endbatchputb,
           finalizea, finalizeb, probea, probeb;
static int user_prev_type, user_curr_type, user_num_types,
           *user_state_start, *user_state_end, *user_types;
static int log_user_state_first_time = 1;
static char user_state_descr[256];
#endif

int ADLB_Init(int num_servers, int use_debug_server, int aprintf_flag, int num_types, int *types,
              int *am_server, int *am_debug_server, MPI_Comm *app_comm)
{
    int rc;

#   if defined( LOG_GUESS_USER_STATE )
    int i;
#   endif

#   if defined( LOG_ADLB_INTERNALS ) || defined( LOG_GUESS_USER_STATE )
    PMPI_Comm_rank(MPI_COMM_WORLD,&my_log_rank);
#   endif

    /* MPE_Init_log() & MPE_Finish_log() are NOT needed when liblmpe.a is linked
       because MPI_Init() would have called MPE_Init_log() already.
    */
#   if defined( NO_MPI_LOGGING )
    MPE_Init_log();
#   endif

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_get_state_eventIDs(&inita,&initb);
    MPE_Log_get_state_eventIDs(&puta,&putb);
    MPE_Log_get_state_eventIDs(&reservea,&reserveb);
    MPE_Log_get_state_eventIDs(&ireservea,&ireserveb);
    MPE_Log_get_state_eventIDs(&geta,&getb);
    MPE_Log_get_state_eventIDs(&getat,&getbt);
    MPE_Log_get_state_eventIDs(&beginbatchputa,&beginbatchputb);
    MPE_Log_get_state_eventIDs(&endbatchputa,&endbatchputb);
    MPE_Log_get_state_eventIDs(&nomoreworka,&nomoreworkb);
    MPE_Log_get_state_eventIDs(&finalizea,&finalizeb);
    MPE_Log_get_state_eventIDs(&probea,&probeb);
    if ( my_log_rank == 0 ) {
        MPE_Describe_state( inita, initb, "ADLB_Init", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( puta, putb, "ADLB_Put", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( reservea, reserveb, "ADLB_Reserve", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( ireservea, ireserveb, "ADLB_Ireserve", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( geta, getb, "ADLB_Get", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( getat, getbt, "ADLB_GetTimed", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( nomoreworka, nomoreworkb, "ADLB_NoMoreWork", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( beginbatchputa, endbatchputb, "ADLB_BatchPut", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( finalizea, finalizeb, "ADLB_Finalize", "MPE_CHOOSE_COLOR" );
        MPE_Describe_state( probea, probeb, "adlb_Probe", "MPE_CHOOSE_COLOR" );
    }
#   endif

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(inita,0,NULL);
#   endif

#   if defined( LOG_GUESS_USER_STATE )
    user_state_start = malloc(num_types * sizeof(int) );
    user_state_end   = malloc(num_types * sizeof(int) );
    user_types       = malloc(num_types * sizeof(int) );
    user_num_types   = num_types;
    for (i=0; i < num_types; i++)
    {
        user_types[i] = types[i];
        MPE_Log_get_state_eventIDs(&user_state_start[i],&user_state_end[i]);
        if ( my_log_rank == 0 )
        {
            sprintf(user_state_descr,"user_state_%d",types[i]);
            MPE_Describe_state( user_state_start[i], user_state_end[i], 
                                user_state_descr, "MPE_CHOOSE_COLOR" );
        }
    }
#   endif

    rc = ADLBP_Init(num_servers,use_debug_server,aprintf_flag,
                    num_types,types,am_server,am_debug_server,app_comm);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(initb,0,NULL);
#   endif

    return rc;
}

int ADLB_Server(double hi_malloc, double periodic_log_interval)
{
    int rc;
    rc = ADLBP_Server(hi_malloc,periodic_log_interval);
    return rc;
}

int ADLB_Debug_server(double timeout)
{
    int rc;
    rc = ADLBP_Debug_server(timeout);
    return rc;
}

int ADLB_Put(void *work_buf, int work_len, int reserve_rank, int answer_rank,
             int work_type, int work_prio)
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(puta,0,NULL);
#   endif

    rc = ADLBP_Put(work_buf,work_len,reserve_rank,answer_rank,work_type,work_prio);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(putb,0,NULL);
#   endif

    return rc;
}

int ADLB_Reserve(int *req_types, int *work_type, int *work_prio, int *work_handle,
                 int *work_len, int *answer_rank)
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(reservea,0,NULL);
#   endif

    rc = ADLBP_Reserve(req_types,work_type,work_prio,work_handle,work_len,answer_rank);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(reserveb,0,NULL);
#   endif

#   if defined( LOG_GUESS_USER_STATE )
    user_prev_type = user_curr_type;
    user_curr_type = *work_type;
#   endif

    return rc;
}

int ADLB_Ireserve(int *req_types, int *work_type, int *work_prio, int *work_handle,
                  int *work_len, int *answer_rank)
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(ireservea,0,NULL);
#   endif

    rc = ADLBP_Ireserve(req_types,work_type,work_prio,work_handle,work_len,answer_rank);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(ireserveb,0,NULL);
#   endif

#   if defined( LOG_GUESS_USER_STATE )
    if (rc > 0)
    {
        user_prev_type = user_curr_type;
        user_curr_type = *work_type;
    }
#   endif

    return rc;
}

int ADLB_Get_reserved(void *work_buf, int *work_handle)
{
    int rc;

#   if defined( LOG_GUESS_USER_STATE )
    int i;
#   endif

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(geta,0,NULL);
#   endif

#   if defined( LOG_GUESS_USER_STATE )
    if (log_user_state_first_time)
        log_user_state_first_time = 0;
    else
    {
        for (i=0; i < user_num_types; i++)
            if (user_prev_type == user_types[i])
                break;
        if (i >= user_num_types)
        {
            aprintf(1,"** invalid type while logging: %d\n",user_prev_type);
            ADLBP_Abort(-1);
        }
#if defined( PRINT_LOGGING )
        aprintf(1111,"LOGGING end %d\n",user_prev_type);
#endif
        MPE_Log_event(user_state_end[i],user_prev_type,NULL);
    }
    for (i=0; i < user_num_types; i++)
        if (user_curr_type == user_types[i])
            break;
    if (i >= user_num_types)
    {
        aprintf(1,"** invalid type while logging: %d\n",user_curr_type);
        ADLBP_Abort(-1);
    }
#if defined( PRINT_LOGGING )
    aprintf(1111,"LOGGING start %d\n",user_curr_type);
#endif
    MPE_Log_event(user_state_start[i],user_curr_type,NULL);
#   endif

    rc = ADLBP_Get_reserved(work_buf,work_handle);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(getb,0,NULL);
#   endif

    return rc;
}

int ADLB_Get_reserved_timed(void *work_buf, int *work_handle, double *qtime)
{
    int rc;

#   if defined( LOG_GUESS_USER_STATE )
    int i;
#   endif

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(getat,0,NULL);
#   endif

#   if defined( LOG_GUESS_USER_STATE )
    if (log_user_state_first_time)
        log_user_state_first_time = 0;
    else     {
        for (i=0; i < user_num_types; i++)
            if (user_prev_type == user_types[i])
                break;
        if (i >= user_num_types)
        {
            aprintf(1,"** invalid type while logging: %d\n",user_prev_type);
            ADLBP_Abort(-1);
        }
#if defined( PRINT_LOGGING )
        aprintf(1111,"LOGGING end %d\n",user_prev_type);
#endif
        MPE_Log_event(user_state_end[i],user_prev_type,NULL);
    }
    for (i=0; i < user_num_types; i++)
        if (user_curr_type == user_types[i])
            break;
    if (i >= user_num_types)
    {
        aprintf(1,"** invalid type while logging: %d\n",user_curr_type);
        ADLBP_Abort(-1);
    }
#if defined( PRINT_LOGGING )
    aprintf(1111,"LOGGING start %d\n",user_curr_type);
#endif
    MPE_Log_event(user_state_start[i],user_curr_type,NULL);
#   endif

    rc = ADLBP_Get_reserved_timed(work_buf,work_handle,qtime);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(getbt,0,NULL);
#   endif

    return rc;
}

int ADLB_Begin_batch_put(void *common_buf, int len_common)
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(beginbatchputa,0,NULL);
#   endif

    rc = ADLBP_Begin_batch_put(common_buf,len_common);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(beginbatchputb,0,NULL);
#   endif

    return rc;
}

int ADLB_End_batch_put()
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(endbatchputa,0,NULL);
#   endif

    rc = ADLBP_End_batch_put();

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(endbatchputb,0,NULL);
#   endif

    return rc;
}

int ADLB_Begin_batch_put_2(void *common_buf, int len_common)
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(beginbatchputa,0,NULL);
#   endif

    rc = ADLBP_Begin_batch_put(common_buf,len_common);  /* **** JUST CALL THE OLD BATCH **** */

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(beginbatchputb,0,NULL);
#   endif

    return rc;
}

int ADLB_End_batch_put_2()
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(endbatchputa,0,NULL);
#   endif

    rc = ADLBP_End_batch_put();  /* **** JUST CALL THE OLD BATCH **** */

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(endbatchputb,0,NULL);
#   endif

    return rc;
}

int ADLB_Set_no_more_work()  // deprecated
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(nomoreworka,0,NULL);
#   endif

    rc = ADLBP_Set_no_more_work();

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(nomoreworkb,0,NULL);
#   endif

    return rc;
}

int ADLB_Set_problem_done()
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(nomoreworka,0,NULL);
#   endif

    rc = ADLBP_Set_problem_done();

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(nomoreworkb,0,NULL);
#   endif

    return rc;
}

int ADLB_Info_get(int key, double *val)
{
    int rc;

    rc = ADLBP_Info_get(key,val);

    return rc;
}

int ADLB_Info_num_work_units(int work_type, int *max_prio, int *num_max_prio_type, int *num_type)
{
    int rc;
    rc = ADLBP_Info_num_work_units(work_type,max_prio,num_max_prio_type,num_type);
    return rc;
}

int ADLB_Finalize()
{
    int rc;

#   if defined( LOG_GUESS_USER_STATE )
    int i;
#   endif

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(finalizea,0,NULL);
#   endif

    rc = ADLBP_Finalize();

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(finalizeb,0,NULL);
#   endif

#   if defined( LOG_GUESS_USER_STATE )
    if ( ! log_user_state_first_time)
    {
        for (i=0; i < user_num_types; i++)
            if (user_prev_type == user_types[i])
                break;
        if (i >= user_num_types)
        {
            aprintf(1,"** invalid type while logging: %d\n",user_prev_type);
            ADLBP_Abort(-1);
        }
#if defined( PRINT_LOGGING )
        aprintf(1111,"LOGGING end %d\n",user_prev_type);
#endif
        MPE_Log_event(user_state_end[i],user_prev_type,NULL);
    }
#   endif

#   if defined( NO_MPI_LOGGING )
    MPE_Finish_log( "adlb" );
#   endif

    return rc;
}

int ADLB_Abort(int code)
{
    int rc;
    rc = ADLBP_Abort(code);
    return rc;
}

int adlb_Probe(int dest, int tag, MPI_Comm comm, MPI_Status *status)
{
    int rc;

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(probea,0,NULL);
#   endif

    rc = adlbp_Probe(dest,tag,comm,status);

#   if defined( LOG_ADLB_INTERNALS )
    MPE_Log_event(probeb,0,NULL);
#   endif

    return rc;
}
