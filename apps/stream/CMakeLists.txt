project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(stream_c stream.c)

find_package(OpenMP QUIET)
if (OPENMP_FOUND)
  add_microbenchmark(stream_omp stream.c)
  target_compile_options(stream_omp PRIVATE ${OpenMP_C_FLAGS})
  target_link_libraries(stream_omp ${OpenMP_C_FLAGS})
else()
  MESSAGE(STATUS "WARNING: No openmp library, skip stream_omp.")
endif()
