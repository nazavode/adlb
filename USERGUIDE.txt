This is the adlb using message-passing and dedicated server
processes instead of separate threads to provide services.

----

Normally, to build, you can just do the usual:
    ./configure
    make

As a convenience, we have some sample sets of args for configure
in the subdir named configargs.  They may be used like this:
    ./configure --with-config-args=sicortex
where sicortex is one of the files of args in configargs.  You can
create arbitrary sets of args for often-used platforms.

If you ever need to rebuild configure from configure.in:
    autoheader ; autoconf

----


Below is a rough outline of how you might proceed looking at the code.

adlb.h might be the best to start looking at because it shows the
interface.  It provides a look at what the user sees and what the
library provides.  Note that it contains prototypes for both the
"real" C procedures and the profiling ones, i.e. ADLB_xxx and
ADLBP_xxx.  adlb_prof.c is merely wrappers around the adlb.c code
which I will suggest looking at later.

orig.pseudocode is the file that we had on the screen in the
conference room.

c1.c is an actual C implementation of the pseudocode, hopefully
with any necessary fixes to get it right.  (f1.f is a Fortran
equivalent but I would probably ignore it unless you are a Fortran
guy.)  c1.c deserves some fairly close examination.

adlb.c is where the adlb logic is.  It is the next one to look at
in detail.  The procedures in there are named ADLBP_ with the P for
profiling.  Wrappers that make them look like the plain ADLB_ version
are in adlb_prof.c.  There is also a version in the adlb_log.c file
created by Anthony.

xq.c contains all code for handling the various queues, e.g. work queue,
i.e. the queue of pieces of work associated data about each, e.g. the
reserving rank if reserved.  rq is a queue for ranks themselves.  It is
where servers queue ranks that have made blocking requests for work
but there is none to give them.  iq is a queue of i-type operations
(e.g. Isends) that have not yet completed.

----
Properties set on files in svn:
    svn propset svn:keywords "Date Revision" adlb.c
----

Using the Debug_server (hang detector):
    Run the program with one extra process that can act as the hang detector.
    That process will have the rank of the last process in the set, e.g. if
    you run with 10 processes, it will have rank 9.
    Inside the program, set the flag to ADLB_Init that indicates you wish
    for one process to become the Debug_server, e.g.:
        use_debug_server = 1;
        ADLB_Init(num_servers,use_debug_server,aprintf_flag,num_types,type_vect,
                  &am_server,&am_debug_server,&app_comm);
    Then, test the flag set by Init for the process that is to play that role
    and execute the correct code:
        if (am_debug_server)
        {
            ADLB_Debug_server(300.0);
        }
    The 300.0 is the number of seconds that the Debug_server should wait
    before timing out the job.  If an amount of time equal to the argument
    passes without the Debug_server hearing from the regular servers, then
    it will abort the job.

----
How to use the periodic stats logging:
    1.  The second parameter to ADLB_Server (a double) determines if any stats
        are logged or not.  If it is 0.0, then no stats are logged.  If it is
        a positive value, say 2.0, then stats are logged approximately every
        2 seconds, as the servers can get them gathered.
    2.  Run the program with whatever option causes the rank number to be
        printed on each output, e.g. -l on sicortex using srun.
    3.  The stats are logged to stderr.  So, if stderr is collected in a file,
        say errfile, then you extract the stats by running:
            get_stats.py errfile 
        Of course if you want to save the output, it is best to redirect the
        stdout of the above cmd to a file as well.
----

How to use David Gingold's MPI debug routines on the ANL sicortex, e.g.
how to build c1:
    $(MPI_CC) $(CFLAGS) -o $@ c1.o /tmp/frost_home/rbutler/gingold_tools/sicortex-scmpi-4.1.    0.18.55116/lib/libmpich.a -lscdma -lpmi -L. -ladlb -lm

----
Some API description:

int ADLB_Init(int nservers, int use_debug_server, int aprintf_flag,
              int ntypes, int type_vect[],
              int *am_server, int *am_debug_server, MPI_Comm *app_comm)
    ADLB_INIT(nservers, use_debug_server, aprintf_flag, ntypes, type_vect, am_server,
              am_debug_server, app_comm, ierr)

    Executed by all processes before any other ADLB functions.
    nservers indicates how many processes should become ADLB servers.
    aprintf_flag turns off prints done by aprintf, usually used internal to adlb
    am_server will be set to 1 if the calling process should become a server, else 0.
    use_debug_server indicates if the last rank should become a debug server.
    am_debug_server will be set to true in the rank that should play that role.
    The program must provide to ADLB a vector of all the types of work that it will
    Put into the work queue.
    type_vect contains an entry for each type. 
    ntypes provides the length of that vector.
    app_comm is a new MPI communicator that processes should use for communications
    instead of MPI_COMM_WORLD.  Many programs do not perform communications other
    than making calls to ADLB.  Nonetheless, app_comm can be used to do MPI calls
    such MPI_Comm_rank.
    Return codes:
         ADLB_SUCCESS
         ADLB_ERROR  (may also generate a printed msg)


int ADLB_Server(double malloc_hwm, double *periodic_log_interval)
    ADLB_SERVER(malloc_hwm, periodic_log_interval, ierr)

    For those processes that are to become ADLB servers, they invoke this function.
    The periodic_log_interval specifies how often, in seconds, we wish to have adlb
    log periodic stats.  If 0.0, then NO logging is done.
    Return codes:
        ADLB_SUCCESS


int ADLB_Begin_batch_put( void* prefix_buf, in len_prefix )
    ADLB_BEGIN_BATCH_PUT( ierr )
int ADLB_End_batch_put()
    ADLB_END_BATCH_PUT( ierr )

    These two functions may enhance performance if an app calls them before and
    after a collections of Put operations that are all performed at about the
    same time.
    The prefix argument is a block of data that appears at the beginning of each
    work unit that will be Put between the Begin and End operations.  This allows
    adlb to just store that block one time.  We have realized significant benefits
    when the data is quite large and fits this model.  If such a prefix does not
    exist, then the pointer can just be NULL.
    Return codes:
        ADLB_SUCCESS


int ADLB_Put(void *work_buf, int work_len, int target_rank, int answer_rank,
             int work_type, int work_prio)
    ADLB_PUT(work_buf, work_len, target_rank, answer_rank, work_type, work_prio, ierr)

    Put work into the queue.
    work_buf points to the work itself.
    work_len is the length of the work in bytes.
    target_rank can be -1 (wild card) if the work can be reserved by any rank.
        If >= 0 the work can only be reserved by the target_rank.
    answer_rank is used by some applications to indicate which rank is interested
    in the results of computations from this work.
    work_type is set by the app but must be one of those registered at Init.
    work_prio is determined by the app.  ADLB makes a non-exhaustive attempt to
    retrieve the highest priority work later.
    Return codes:
        ADLB_SUCCESS
        ADLB_NO_MORE_WORK
        ADLB_DONE_BY_EXHAUSTION
        ADLB_PUT_REJECTED


int ADLB_Reserve(int *req_types, int *work_type, int *work_prio, int *work_handle,
                 int *work_len, int *answer_rank)
    ADLB_RESERVE(req_types, work_type, work_prio, work_handle, work_len, answer_rank, ierr)

    Reserves work but does not retrieve it.  This is useful for finding out how
    big a piece of work is and allocating buffer space before the actual retrieval.
    This call hangs until either work is retrieved or ADLB_NO_MORE_WORK is returned
    as the return code.
    req_types is a vector of up to 4 types (currently) that are ORed together.  ADLB
    will return the highest priority work of one of those types that it can find.
    work_type is set to the type of work actually reserved.
    work_prio is set to the priority of the work reserved.
    work_handle is set to a value that is used to retrieve the work later.
    work_len can be used to allocate buffer space before doing a Get operation.
    answer_rank tells which rank to send answers to (or reserve them for); not 
    necessarily used by all apps.
    Return codes:
        ADLB_SUCCESS
        ADLB_NO_MORE_WORK
        ADLB_DONE_BY_EXHAUSTION


int ADLB_Ireserve(int *req_types, int *work_type, int *work_prio, int *work_handle,
                  int *work_len, int *answer_rank)
    ADLB_IRESERVE(req_types, work_type, work_prio, work_handle, work_len, answer_rank, ierr)

    Same as Reserve, but does not hang.  If return code is < 0, no work was available.
    Return codes:
        ADLB_SUCCESS
        ADLB_NO_MORE_WORK
        ADLB_DONE_BY_EXHAUSTION
        ADLB_NO_CURRENT_WORK (due to non-blocking)


int ADLB_Get_reserved(void *work_buf, int work_handle)
    ADLB_GET_RESERVED(work_buf, work_handle, ierr)

    Use a handle obtained during a previous Reserve/Ireserve to retrieve the work.
    Return codes:
        ADLB_SUCCESS
        ADLB_NO_MORE_WORK
        ADLB_DONE_BY_EXHAUSTION


int ADLB_Set_problem_done()
    ADLB_Set_problem_done( ierr )

    Once this is called, ADLB starts returning ADLB_NO_MORE_WORK return codes for
    Reserve, etc.
    Return codes:
        ADLB_SUCCESS


int ADLB_Finalize()
    ADLB_FINALIZE( ierr )

    Should be called to finalize operations.
    Return codes:
        ADLB_SUCCESS
        ADLB_ERROR  (may also generate printed msg)


int ADLB_Abort( int code )<br>
    ADLB_ABORT( code, ierr )

    This function can be called to abort the program.
    It tries to get the servers to print final stats data.
    Then, it calls MPI_Abort on MPI_COMM_WORLD with the same error code.
    Return codes:
        N/A
