project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

set(CMAKE_MODULE_PATH ${VL_uBMK_SOURCE_DIR}/cmake)

include(CheckLibraryExists)
include(CheckFunctionExists)
include(ExternalProject)
include(CheckCXXSourceCompiles)

include_directories(${VL_uBMK_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(COMPILER_FLAGS "-Wall -g")

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
endmacro(add_microbenchmark)
