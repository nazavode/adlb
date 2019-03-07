
		     The "batcher" ADLB Application


The program "batcher" is an MPI/ADLB parallel program that executes
shell scripts in parallel.  To use it, one prepares a file of shell
commands, one per line, and runs batcher as an MPI program (with
mpirun or mpiexec or whatever starts MPI jobs on your system) passing
the name of the command file as an argument.  Here are the steps to
running an example:

0.  Set up the Makefile:

    ./configure

    If you don't have a Fortran environment on your machine (none is
    needed for batcher), use

    ./configure --disable-f77

1.  Compile and link the ADLB library and the batcher program in this
directory: 

    make batcher

2.  Make a directory for the output, to keep it separate from this
source directory:

    mkdir cmdsout

3.  Prepare a file of shell commands.  Here is an included one, called
"cmds2":

(echo "job1"; echo `date`; sleep 1; echo `date`) > cmdsout/file1
(echo "job2"; echo `date`; sleep 2; echo `date`) > cmdsout/file2
(echo "job3"; echo `date`; sleep 3; echo `date`) > cmdsout/file3
(echo "job4"; echo `date`; sleep 4; echo `date`) > cmdsout/file4
(echo "job5"; echo `date`; sleep 5; echo `date`) > cmdsout/file5
(echo "job6"; echo `date`; sleep 6; echo `date`) > cmdsout/file6
(echo "job7"; echo `date`; sleep 7; echo `date`) > cmdsout/file7
(echo "job8"; echo `date`; sleep 8; echo `date`) > cmdsout/file8
(echo "job9"; echo `date`; sleep 9; echo `date`) > cmdsout/file9

Note that each job runs for a different number of seconds, and prints
timestamps for when it starts and finishes.  The output goes to a
separate file for each job in the output directory.

4.  Run it:

    mpiexec -n 3 batcher cmds2

    Currently the command lines are echoed as they are read in.  If
    you don't like this, comment out the relevant line in batcher.c
    and recompile.

5.  Look at the output:

    wuzzle% foreach i (1 2 3 4 5 6 7 8 9)
    foreach? cat cmdsout/file$i
    foreach? end
    job1
    Wed Aug 12 15:40:45 CDT 2009
    Wed Aug 12 15:40:46 CDT 2009
    job2
    Wed Aug 12 15:40:45 CDT 2009
    Wed Aug 12 15:40:47 CDT 2009
    job3
    Wed Aug 12 15:40:46 CDT 2009
    Wed Aug 12 15:40:49 CDT 2009
    job4
    Wed Aug 12 15:40:47 CDT 2009
    Wed Aug 12 15:40:51 CDT 2009
    job5
    Wed Aug 12 15:40:49 CDT 2009
    Wed Aug 12 15:40:54 CDT 2009
    job6
    Wed Aug 12 15:40:51 CDT 2009
    Wed Aug 12 15:40:57 CDT 2009
    job7
    Wed Aug 12 15:40:54 CDT 2009
    Wed Aug 12 15:41:01 CDT 2009
    job8
    Wed Aug 12 15:40:57 CDT 2009
    Wed Aug 12 15:41:05 CDT 2009
    job9
    Wed Aug 12 15:41:01 CDT 2009
    Wed Aug 12 15:41:10 CDT 2009
    wuzzle% 

    Note that for each x, jobx runs for x seconds, as it it supposed
    to.  Parallelism is 2-way for a 3-process MPI job since ADLB takes
    one process to be its server.  Examination of the time stamps
    gives the details.  Note that the total time of all the jobs was
    45 seconds, but that the elapsed time was only 25 seconds, a
    speedup of approximately 2, as expected.

6.  Homework problem:

    Better speedup can be obtained, as in any load-balancing
    operation, by running the bigger jobs first.  Reverse the order of
    the lines in your command file.  Now what is the elapsed time?
