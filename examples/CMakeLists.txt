add_executable(tsp tsp.c)
target_link_libraries(tsp PRIVATE adlb::adlb)

add_executable(nq nq.c)
target_link_libraries(nq PRIVATE adlb::adlb)

add_executable(batcher batcher.c)
target_link_libraries(batcher PRIVATE adlb::adlb)

add_executable(stats stats.c)
target_link_libraries(stats PRIVATE adlb::adlb)

add_executable(sudoku sudoku.c)
target_link_libraries(sudoku PRIVATE adlb::adlb)

add_executable(pmcmc pmcmc.c)
target_link_libraries(pmcmc PRIVATE adlb::adlb)

add_executable(model model.c)
target_link_libraries(model PRIVATE adlb::adlb)

add_executable(c1 c1.c)
target_link_libraries(c1 PRIVATE adlb::adlb)

add_executable(c2 c2.c)
target_link_libraries(c2 PRIVATE adlb::adlb)

add_executable(c3 c3.c)
target_link_libraries(c3 PRIVATE adlb::adlb)

add_executable(c4 c4.c)
target_link_libraries(c4 PRIVATE adlb::adlb)

add_executable(skel skel.c)
target_link_libraries(skel PRIVATE adlb::adlb)

add_executable(add2 add2.c)
target_link_libraries(add2 PRIVATE adlb::adlb)

if(ADLB_ENABLE_FORTRAN)
    add_executable(fbatcher fbatcher.f)
    target_link_libraries(fbatcher
        PRIVATE adlb::adlb_fortran MPI::MPI_Fortran)

    add_executable(f1 f1.f)
    target_link_libraries(f1
        PRIVATE adlb::adlb_fortran MPI::MPI_Fortran)
endif()
