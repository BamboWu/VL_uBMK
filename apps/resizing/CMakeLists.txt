project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)


if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip resizing.")
else (NOT RaftLib_FOUND)
  add_raftbenchmark(resizing_armq resizing.cpp)
  add_raftbenchmark(resizing_orig resizing.cpp)
  target_compile_definitions(resizing_orig PRIVATE -DRAFTLIB_ORIG=1)
endif()
