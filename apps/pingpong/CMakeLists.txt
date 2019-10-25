project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(pingpong pingpong.cpp)
target_link_libraries(pingpong uBMK_util atomic)
