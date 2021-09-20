project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
  add_microbenchmark(max_vl max_vl.cpp)
  target_link_libraries(max_vl ${VL_LIBRARY})
  add_microbenchmark(max_vl_next max_vl.cpp)
  target_link_libraries(max_vl_next ${VL_LIBRARY})
  target_compile_definitions(max_vl_next PRIVATE -DALWAYS_NEXT)
endif()
