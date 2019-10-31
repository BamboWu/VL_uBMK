project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(atomicFSh shopping.c)
set_target_properties(atomicFSh PROPERTIES COMPILE_DEFINITIONS "ATOMIC")
add_microbenchmark(atomicPad shopping.c)
set_target_properties(atomicPad PROPERTIES COMPILE_DEFINITIONS "ATOMIC")
set_target_properties(atomicPad PROPERTIES COMPILE_DEFINITIONS "PADDING")
add_microbenchmark(directFSh shopping.c)
