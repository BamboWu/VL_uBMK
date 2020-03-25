project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(max_sce max_sce.c)
if(GEM5_ROOT)
  target_link_libraries(max_sce m5)
endif()
