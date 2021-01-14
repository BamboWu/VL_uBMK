CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(fluidanimate CXX)

if(USE_MUTEX)
  add_definitions(-DUSE_MUTEX)
endif()

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip search.")
else()
  add_raftbenchmark(search_std bmh.cpp)
  target_compile_definitions(search_std PRIVATE -DSTDALLOC=1)
  add_raftbenchmark(search_dyn bmh.cpp)
  if (VL_FOUND)
    add_raftbenchmark(search_vl bmh.cpp)
    target_compile_definitions(search_vl PRIVATE -DVL=1)
    target_link_libraries(search_vl ${VL_LIBRARY})
  endif()
endif()
