project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

find_package(Boost QUIET COMPONENTS atomic)
if(Boost_ATOMIC_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  add_microbenchmark(pingpong pingpong.cpp)
  target_link_libraries(pingpong ${Boost_LIBRARIES})
else()
  MESSAGE(STATUS "WARNING: No boost library, skip pingpong.")
endif()
