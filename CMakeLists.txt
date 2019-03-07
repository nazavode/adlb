cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(adlb VERSION 0.1 LANGUAGES C)

option(ADLB_ENABLE_FORTRAN "enable building the Fortran interface (requires Fortran toolchain)" ON)

if(ADLB_ENABLE_FORTRAN)
    include(CheckLanguage)
    check_language(Fortran)
    if(CMAKE_Fortran_COMPILER)
        enable_language(Fortran)
        include(FortranCInterface)
        FortranCInterface_VERIFY()
    else()
        message(FATAL_ERROR "Fortran support can't be enabled, \ 
please set ADLB_ENABLE_FORTRAN=OFF or provide a valid Fortran toolchain.")
    endif()
endif()

# Define adlb::mpi target
#
find_package(MPI REQUIRED)

# Common locations
#
set(ADLB_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ADLB_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define adlb::adlb target
#
set(sources 
    ${ADLB_SRC_PATH}/adlb.c
    ${ADLB_SRC_PATH}/xq.c)
set(headers 
    ${ADLB_INCLUDE_PATH}/adlb/adlb.h
    ${ADLB_INCLUDE_PATH}/adlb/xq.h)
add_library(adlb ${sources})
add_library(adlb::adlb ALIAS adlb)
set_property(TARGET adlb PROPERTY PUBLIC_HEADER ${headers})
target_include_directories(adlb PUBLIC ${ADLB_INCLUDE_PATH})
target_link_libraries(adlb PUBLIC MPI::MPI_C)

if(ADLB_ENABLE_FORTRAN)
    FortranCInterface_HEADER(${CMAKE_CURRENT_BINARY_DIR}/fortran_c_mangling.h MACRO_NAMESPACE "ADLB_FC_")
    # Define adlb::adlb_fortran target
    #
    set(sources 
        ${ADLB_SRC_PATH}/adlbf.c)
    set(headers 
        ${ADLB_INCLUDE_PATH}/adlb/adlbf.h)
    add_library(adlb_fortran ${sources})
    add_library(adlb::adlb_fortran ALIAS adlb_fortran)
    set_property(TARGET adlb_fortran PROPERTY PUBLIC_HEADER ${headers})
    target_include_directories(adlb_fortran
        PUBLIC ${ADLB_INCLUDE_PATH}
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(adlb_fortran
        PUBLIC adlb::adlb)
endif()
