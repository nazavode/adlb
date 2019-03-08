cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(adlb VERSION 0.1 LANGUAGES C)

option(ADLB_ENABLE_FORTRAN "enable building the Fortran interface (requires Fortran toolchain)" ON)
option(ADLB_ENABLE_EXAMPLES "enable building the bundled example programs" ON)

if(ADLB_ENABLE_FORTRAN)
    include(CheckLanguage)
    check_language(Fortran)
    if(CMAKE_Fortran_COMPILER)
        enable_language(Fortran)
    else()
        message(FATAL_ERROR "Fortran support can't be enabled, \ 
please set ADLB_ENABLE_FORTRAN=OFF or provide a valid Fortran toolchain.")
    endif()
endif()

# Define MPI::MPI_C target
#
find_package(MPI REQUIRED)

include(GNUInstallDirs)
set(ADLB_INSTALL_TARGETS) # add to this list all targets to be installed

# Common locations
#
set(ADLB_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ADLB_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define adlb::adlb target
#
add_library(adlb
    ${ADLB_SRC_PATH}/adlb.c
    ${ADLB_SRC_PATH}/adlb_prof.c
    ${ADLB_SRC_PATH}/xq.c)
add_library(adlb::adlb ALIAS adlb)
set_property(TARGET adlb
    PROPERTY PUBLIC_HEADER ${ADLB_INCLUDE_PATH}/adlb/adlb.h)
target_include_directories(adlb
    PUBLIC    $<BUILD_INTERFACE:${ADLB_INCLUDE_PATH}>
    INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(adlb PUBLIC MPI::MPI_C)
list(APPEND ADLB_INSTALL_TARGETS adlb)

if(ADLB_ENABLE_FORTRAN)
    set(ADLB_GEN_HEADERS_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR})
    # Ensure C/Fortran interoperability and
    # generate mangling header:
    include(FortranCInterface)
    FortranCInterface_VERIFY()
    FortranCInterface_HEADER(
        ${ADLB_GEN_HEADERS_INCLUDE_PATH}/fortran_c_mangling.h
        MACRO_NAMESPACE "ADLB_FC_")
    # Define adlb::adlb_fortran target
    #
    add_library(adlb_fortran ${ADLB_SRC_PATH}/adlbf.c)
    add_library(adlb::adlb_fortran ALIAS adlb_fortran)
    set_property(TARGET adlb_fortran 
        PROPERTY PUBLIC_HEADER ${ADLB_INCLUDE_PATH}/adlb/adlbf.h)
    target_include_directories(adlb_fortran
        PRIVATE   $<BUILD_INTERFACE:${ADLB_GEN_HEADERS_INCLUDE_PATH}>
        PUBLIC    $<BUILD_INTERFACE:${ADLB_INCLUDE_PATH}>
        INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(adlb_fortran
        PUBLIC adlb::adlb)
    list(APPEND ADLB_INSTALL_TARGETS adlb_fortran)
endif()

# Examples
#
if(ADLB_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install and exports
#
install(TARGETS ${ADLB_INSTALL_TARGETS}
    EXPORT adlb-exports
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # Windows needs RUNTIME also for libraries
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/adlb)

install(EXPORT adlb-exports
    FILE AdlbTargets.cmake
    NAMESPACE adlb::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/adlb
)
