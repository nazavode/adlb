# ADLB - Asynchronous, Dynamic Load-Balancing for MPI applications

ADLB provides a parallel, distributed (via MPI) task queue to let
applications overcome unbalance and scalability issues via asynchronous,
dynamic load balancing and work stealing.

ADLB provides both `C` and `Fortran` APIs.

**This is a fork of the [original ADLB project](https://www.cs.mtsu.edu/~rbutler/adlb/)
by [Prof. Ralph Butler](https://www.cs.mtsu.edu/~rbutler).
Please find upstream repo here: http://svn.cs.mtsu.edu/svn/adlbm/trunk**

## Improvements over upstream

* Portable build system generated by `CMake`.
* Code clean up, removed unused and outdated sources.
* Code clean up, reduced public interface and header inclusions flowing downstream to users.
* Auto-generated `Fortran` linkage mangling.
* Bug fixes.

# License

The code in this repo tries really hard to abide by the original license
from upstream. While no proper licesing information is shipped with the original
code, a reference to MPICH licesing model is reported on the
[project's home page](https://www.cs.mtsu.edu/~rbutler/adlb/): library authors have been contacted to find a proper license for this repo while in the meantime a
license similar to MPICH's (a BSD-like) has been tentatively adopted. Please note
that a relicense could be necessary in the near future.
