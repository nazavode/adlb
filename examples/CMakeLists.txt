add_executable(tsp tsp.c)
target_link_libraries(tsp PRIVATE adlb::adlb)

add_executable(nq nq.c)
target_link_libraries(nq PRIVATE adlb::adlb)
