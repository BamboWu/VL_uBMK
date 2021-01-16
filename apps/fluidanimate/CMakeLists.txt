CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(fluidanimate CXX)

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip fluidanimate.")
elseif(NOT QTHREAD_FOUND)
  MESSAGE(STATUS "WARNING: No qthread found, skip fluidanimate.")
else()
  add_raftbenchmark(fluidanimate_std main.cpp raftlib_src.cpp
                    fluidcmp.cpp cellpool.cpp)
  target_compile_definitions(fluidanimate_std PRIVATE -DSTDALLOC=1 -DUSE_MUTEX)
  add_raftbenchmark(fluidanimate_dyn main.cpp raftlib_src.cpp
                    fluidcmp.cpp cellpool.cpp)
  target_compile_definitions(fluidanimate_std PRIVATE -DUSE_MUTEX)
  if (NOT VL_FOUND)
    MESSAGE(STATUS "WARNING: No libvl found, skip fluidanimate_vl.")
  else()
    add_raftbenchmark(fluidanimate_vl main.cpp raftlib_src.cpp
                      fluidcmp.cpp cellpool.cpp)
    target_compile_definitions(fluidanimate_vl PRIVATE -DVL=1 -DUSE_MUTEX)
    target_link_libraries(fluidanimate_vl ${VL_LIBRARY})
  endif()
endif()
