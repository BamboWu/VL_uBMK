project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

set(CMAKE_MODULE_PATH ${VL_uBMK_SOURCE_DIR}/cmake)

include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(ExternalProject)
include(CheckCXXSourceCompiles)

include_directories(${VL_uBMK_SOURCE_DIR}/include)
if(GEM5_ROOT)
  include_directories(${GEM5_ROOT}/include)
  link_directories(${GEM5_ROOT}/util/m5)
else()
  add_definitions(-DNOGEM5)
endif()

# check libraries
find_package(Boost QUIET COMPONENTS atomic thread chrono)
find_package(GccAtomic QUIET)
find_package(VL QUIET)
find_package(ZMQ QUIET)
find_package(PAPI QUIET)
find_package(Numa QUIET)
find_package(QThread QUIET)
find_package(RaftLib QUIET)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

execute_process(COMMAND grep -l 'rdtscp' /proc/cpuinfo
    OUTPUT_VARIABLE CPU_HAS_RDTSCP)
if(CPU_HAS_RDTSCP)
  add_definitions(-DHAS_RDTSCP=1)
else()
  add_definitions(-DHAS_RDTSCP=0)
endif()

execute_process(COMMAND ../scripts/try_PMU_on_Raspi4.sh
    RESULT_VARIABLE TRY_PMU_ERROR)
if(0 EQUAL TRY_PMU_ERROR)
  MESSAGE(STATUS "INFO: User space PMU avaialble, ARM_USER_PMU_V3 = 1.")
  add_definitions(-DARM_USER_PMU_V3=1)
else()
  MESSAGE(STATUS "WARNING: User space no access to PMU, ARM_USER_PMU_V3 = 0.")
  if("255" EQUAL TRY_PMU_ERROR)
    MESSAGE(STATUS "INFO: Try pmuenable kernel module in src/ if have sudo.")
  endif()
  add_definitions(-DARM_USER_PMU_V3=0)
endif()

add_compile_options(-g -Wall)

subdirs(apps src)

macro(add_microbenchmark NAME)
  add_executable(${NAME} ${ARGN})
  target_link_libraries(${NAME} uBMK_util)
  add_dependencies(${NAME} uBMK_util)
  if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(${NAME} PUBLIC "-pthread")
  endif()
  if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(${NAME} "${CMAKE_THREAD_LIBS_INIT}")
  endif()
  if(GEM5_ROOT)
    target_link_libraries(${NAME} m5)
  endif()
endmacro(add_microbenchmark)

macro(add_raftbenchmark NAME)
  add_executable(${NAME} ${ARGN})
  target_compile_options(${NAME} PRIVATE -faligned-new)
  target_compile_definitions(${NAME} PRIVATE
      -DL1D_CACHE_LINE_SIZE=64 -DUSEQTHREADS=1)
  target_include_directories(${NAME} PRIVATE
      ${QTHREAD_INCLUDE_DIRS} ${RaftLib_INCLUDE_DIRS})
  target_link_libraries(${NAME} uBMK_util
      ${RaftLib_LDFLAGS} ${QTHREAD_LIBRARIES})
  add_dependencies(${NAME} uBMK_util)
  if(GEM5_ROOT)
    target_link_libraries(${NAME} m5)
  endif()
endmacro(add_raftbenchmark)
