This file gives the basic algorithm for the GFMC program written by
Steve Pieper.  We have rough approximations to it in c1,c2,c3,c4.c
where c4.c is currently the closest to what is described here.  We 
refer to it as a mini-app.
The basic algorithm has changed over time.  Currently, it creates
packages of 4 types which are Put into the ADLB system.  We use the
simple names A,B,C,D to refer to the packets in the test program.

Meanings of these work packages in context of GFMC:

    A - GFMC propagation
    B - Energy calculation
    C - Pair-potential
    D - Wave function

Package sizes managed and used by the program:

    A  4 Mbytes, all specific for both package and answer
    B  4 Mbytes, no answer
    C  9 Mbytes common (prefix), 13 Mbytes specific (suffix); 1 kbytes answer
    D  1 kbytes (when batch is used, .5 common and .5 specific); 4 Mbytes answer

Priorities for packet types:
    
    A, B         low      (we use 1)
    C            medium   (we use 2)
    D            high     (we use 3)
    any answer   highest  (we use 9)

Work times for packet types (also in the pseudocode below):

    A   0.5 seconds
    B   0.5 seconds
    C   0.1 seconds
    D   0.5 seconds


5% of the ranks are ADLB servers (default 1; we override this on the command line currently)

Of the "app" ranks:
    1 rank is the master
    2% of the ranks are walkers - ranks 1 ... N/50  (we require at least 1 rank)
    The rest are general workers


The master (in this dummy case) just does the following:

    loop 40*M times (see below for M; 40 is default multiplier - NUM_BS_PER_BATCH below)
        get MPI message gen'd after handling w.p. B (see below) (B_ANSWER)
    end loop
    adlb_set_no_more_work (adlb_set_problem_done is new name)


The walkers do the following:

    loop M times - as much as you want  M_OUTER_IDX

        loop 20 times                   INNER_IDX
            do batch put of 40 A w.p.   NUM_AS_PER_BATCH
            loop until 40 A answers and 40 A' answers have been received  (NOTE A' is just a new A)
                wait for answer packet for A (or A') 
                for A answer, make another A packet (called A') (num A' should not exceed As)
                for A' answer, do nothing (except increment count of number received)
                (OPTIONALLY based on load, gfmc sometimes processes A packets here also,
                 however our code currently always processes As here when available)
            end A & A' loop
        end 20 loop
        make 40 B's                     NUM_BS_PER_BATCH (same as As here)

    end loop M times

    switch to being a general worker as below


The other workers do the following:

    loop until no more work

        accept A,B,C,D work package

        A package:
            make 1 D w.p. and wait for answer (can ALSO HANDLE Ds while waiting for D answer)
            about 0.5 second work
            send A answer

        B package:
            make 72 D packages at once (batch)    NUM_DS_PER_BATCH
            loop
                collect 72 answers (ALSO HANDLE Ds while waiting for D answers)
            end loop
            make 15 C packages in one batch       NUM_CS_PER_BATCH
                for more realism, the 15 should be a random number for each case
                say uniform from 5 to 25 or better yet, a normal distribution
                with centroid 15 and 1/2 width 5  (0 is allowed)
            loop
                collect 15 (or random number) C answers (ALSO HANDLE Cs while waiting for C answers)
            end loop
            about 0.5 sec processing
            send message to master  (not a real send; just Put B answer)

        C package
            make 3 D w.p. in a batch              JUST HARD-CODE for 3 here
            collect 3 D answers (ALSO HANDLE Ds while waiting for D answers)
            about 0.1 sec work
            send C answer

        D package
           about 0.5 sec work
           send D answer

    end loop until no more work
