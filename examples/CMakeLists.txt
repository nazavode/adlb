set(examples_c
    tsp.c
    nq.c
    batcher.c
    stats.c
    sudoku.c
    pmcmc.c
    model.c
    c1.c
    c2.c
    c3.c
    c4.c
    skel.c
    add2.c
    grid_daf.c
    grid_old_daf.c
    grid_uni.c
)

set(examples_fortran
    fbatcher.f
    f1.f
)

foreach(example_source IN LISTS examples_c)
    string(REGEX REPLACE "\\.[^.]*$" "" example_target ${example_source})
    add_executable(${example_target} ${example_source})
    target_link_libraries(${example_target} PRIVATE adlb::adlb m)
endforeach()

if(ADLB_ENABLE_FORTRAN)
    foreach(example_source IN LISTS examples_fortran)
        string(REGEX REPLACE "\\.[^.]*$" "" example_target ${example_source})
        add_executable(${example_target} ${example_source})
        target_link_libraries(${example_target} PRIVATE adlb::adlb_fortran MPI::MPI_Fortran)
    endforeach()
endif()
