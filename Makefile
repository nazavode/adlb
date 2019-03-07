CC  = mpicc
FC  = mpif90

AR      = ar
ARFLAGS = crv
RANLIB  = ranlib

INC     = -I. -I$(srcdir)
FINC    = -I. -I$(srcdir)

# No logging
LDFLAGS  = 
CFLAGS  = -g -O2 $(INC)
FFLAGS  =  $(FINC)
# Both MPI and user-defined MPE logging
CFLAGS_MPILOG  = $(CFLAGS) -DLOG_GUESS_USER_STATE 
FFLAGS_MPILOG  = $(FFLAGS) 
# user-defined MPE logging
CFLAGS_LOG  = $(CFLAGS) -DNO_MPI_LOGGING -DLOG_GUESS_USER_STATE 
FFLAGS_LOG  = $(FFLAGS) 

LIBS    = 
# FLIBS   =   -L/nfshome/rbutler/public/courses/pp6430/mpich3i/lib -L/usr/lib/gcc/x86_64-linux-gnu/4.9 -L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu -L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../../lib -L/lib/x86_64-linux-gnu -L/lib/../lib -L/usr/lib/x86_64-linux-gnu -L/usr/lib/../lib -L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../.. -lmpifort -lmpi -lgfortran -lm -lquadmath
FLIBS   = 

LIBDIR  = -L.
FLIBDIR = -L.

OBJS     = adlb.o adlb_prof.o xq.o 
FOBJS    = adlbf.o
ELOBJS   = adlb_prof.eo
MLOBJS   = adlb_prof.mo

SHELL    = /bin/bash
MAKE     = make --no-print-directory
srcdir   = .
VPATH=.:$(srcdir)

BUILD_FORTRAN = yes
BUILD_LOGGING = no

.SUFFIXES: .c .f .o .eo .mo

.c.o:
	$(CC) $(CFLAGS) -c $<

.f.o:
	$(FC) $(FFLAGS) -c $<

# .eo are object file that does only user-defined MPE logging
.c.eo:
	$(CC) $(CFLAGS_LOG) -c -o $*.eo $<

.f.eo:
	$(FC) $(FFLAGS_LOG) -c -o $*.eo $<

# .mo are object file that does both MPI and user-defined MPE logging
.c.mo:
	$(CC) $(CFLAGS_MPILOG) -c -o $*.mo $<

.f.mo:
	$(FC) $(FFLAGS_MPILOG) -c -o $*.mo $<

all: libs

steve: libs
	cd ../mc ; ./makeit 14n m0 adlb

tests: c1 
	@if [ "$(BUILD_FORTRAN)" = "yes" ] ; then \
	     $(MAKE) f1 ; \
	 fi
	@if [ "$(BUILD_LOGGING)" = "yes" ] ; then \
	     $(MAKE) c1log c1mpilog ; \
	     if [ "$(BUILD_FORTRAN)" = "yes" ] ; then \
	         $(MAKE) f1log f1mpilog ; \
	     fi ; \
	 fi

acctests: libs
	cd ../mc ; ./makeit 4he m0 adlb
	cd ../mc ; ./makeit 8be m0 adlb

c1:  c1.o libadlb.a
	$(CC) $(CFLAGS) -o $@ c1.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

c2:  c2.o libadlb.a
	$(CC) $(CFLAGS) -o $@ c2.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

c3:  c3.o libadlb.a
	$(CC) $(CFLAGS) -o $@ c3.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

c4:  c4.o libadlb.a
	$(CC) $(CFLAGS) -o $@ c4.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

t1:  t1.o libadlb.a
	$(CC) $(CFLAGS) -o $@ t1.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

tsp:  libadlb.a tsp.o
	$(CC) $(CFLAGS) -o $@ tsp.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

f1:  f1.o libfadlb.a libadlb.a
	$(FC) $(FFLAGS) -o $@ f1.o $(FLIBDIR) $(LDFLAGS) -lfadlb -ladlb $(FLIBS) -lm

sudoku:  sudoku.o libadlb.a
	$(CC) $(CFLAGS) -o $@ sudoku.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

sudoku_log:  sudoku.o libeladlb.a libadlb.a
	$(CC) $(CFLAGS_LOG) -o $@ sudoku.o $(LIBDIR) -leladlb -ladlb $(LIBS) -lm

sudoku_mpilog:  sudoku.o libmladlb.a libadlb.a
	$(CC) $(CFLAGS_MPILOG) -o $@ sudoku.o $(LIBDIR) -lmladlb -ladlb $(LIBS) -lm

partest:  partest.c
	$(CC) -O0 -o partest partest.c -lm

model:  model.o libadlb.a
	$(CC) $(CFLAGS) -o $@ model.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

nq:  nq.o libadlb.a
	$(CC) $(CFLAGS) -o $@ nq.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

pmcmc:  pmcmc.o libadlb.a
	$(CC) $(CFLAGS) -o $@ pmcmc.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

batcher:  batcher.o libadlb.a
	$(CC) $(CFLAGS) -o $@ batcher.o $(LIBDIR) $(LDFLAGS) -ladlb $(LIBS) -lm

# c1log does only user-defined MPE logging
c1log:  c1.o libeladlb.a libadlb.a
	$(CC) $(CFLAGS_LOG) -o $@ c1.o $(LIBDIR) -leladlb -ladlb $(LIBS) -lm

# c1mpilog does both MPI and user-defined MPE logging
c1mpilog:  c1.o libmladlb.a libadlb.a
	$(CC) $(CFLAGS_MPILOG) -o $@ c1.o $(LIBDIR) -lmladlb -ladlb $(LIBS) -lm

# f1log does only user-defined MPE logging
f1log:  f1.o libfadlb.a libeladlb.a libadlb.a
	$(FC) $(FFLAGS_LOG) -o $@ f1.o $(FLIBDIR) -lfadlb -leladlb -ladlb $(FLIBS) -lm

# f1mpilog does only user-defined MPE logging
f1mpilog:  f1.o libfadlb.a libmladlb.a libadlb.a
	$(FC) $(FFLAGS_MPILOG) -o $@ f1.o $(FLIBDIR) -lfadlb -lmladlb -ladlb $(FLIBS) -lm


libs: libadlb.a
	@if [ "$(BUILD_FORTRAN)" = "yes" ] ; then \
	     $(MAKE) libfadlb.a ; \
	 fi
	@if [ "$(BUILD_LOGGING)" = "yes" ] ; then \
	     $(MAKE) libeladlb.a libmladlb.a ; \
	 fi

prep_for_commit:
	./fix_version.py
	./genfh.py

libadlb.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

libfadlb.a: $(FOBJS)
	$(AR) $(ARFLAGS) $@ $(FOBJS)
	$(RANLIB) $@

# ADLB library, libeladlb.a, that does only user-defined MPE logging
libeladlb.a: $(ELOBJS)
	$(AR) $(ARFLAGS) $@ $(ELOBJS)
	$(RANLIB) $@

# ADLB library, libmladlb.a, that does both MPI and user-defined MPE logging
libmladlb.a: $(MLOBJS)
	$(AR) $(ARFLAGS) $@ $(MLOBJS)
	$(RANLIB) $@


clean:
	rm -f $(OBJS) $(FOBJS) $(ELOBJS) $(MLOBJS) *.o *.eo *.mo

realclean: clean
	rm -f *.o lib*.a
	rm -f c1 c2 c3 c4 f1 c1log c1mpilog f1log f1mpilog tsp sudoku model nq pmcmc batcher
