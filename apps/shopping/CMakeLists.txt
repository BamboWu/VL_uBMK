project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(atomicFSh shopping.c)
target_compile_definitions(atomicFSh PRIVATE "ATOMIC")
add_microbenchmark(atomicPad shopping.c)
target_compile_definitions(atomicPad PRIVATE "ATOMIC" PRIVATE "PADDING")
add_microbenchmark(directFSh shopping.c)

if(NUMA_FOUND)
  add_definitions(-DNUMA_AVAILABLE)
  target_link_libraries(atomicFSh "numa")
  target_link_libraries(atomicPad "numa")
  target_link_libraries(directFSh "numa")
endif()
