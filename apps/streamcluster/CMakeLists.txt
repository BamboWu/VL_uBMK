CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(streamcluster CXX)

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip streamcluster.")
else (NOT RaftLib_FOUND)
  if(NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip streamcluster_qt.")
  else(NOT QTHREAD_FOUND)
    add_raftbenchmark(streamcluster_qt_std main.cpp raftlib_src.cpp)
    target_compile_definitions(streamcluster_qt_std PRIVATE
        -DUSE_QTHREAD=1 -DSTDALLOC=1)
    add_raftbenchmark(streamcluster_qt_dyn main.cpp raftlib_src.cpp)
    target_compile_definitions(streamcluster_qt_dyn PRIVATE -DUSE_QTHREAD=1)
    add_custom_target(streamcluster_qt)
    add_dependencies(streamcluster_qt streamcluster_qt_std
        streamcluster_qt_dyn)
    if (NOT VL_FOUND)
      MESSAGE(STATUS "WARNING: No libvl found, skip streamcluster_qt_vl.")
    else()
      add_raftbenchmark(streamcluster_qt_vl main.cpp raftlib_src.cpp)
      target_compile_definitions(streamcluster_qt_vl PRIVATE
          -DUSE_QTHREAD=1 -DVL=1)
      target_link_libraries(streamcluster_qt_vl ${VL_LIBRARY})
      add_dependencies(streamcluster_qt streamcluster_qt_vl)
    endif()
  endif(NOT QTHREAD_FOUND)
  if(NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip streamcluster_ut.")
  else(NOT UT_FOUND)
    add_raftbenchmark(streamcluster_ut_std main.cpp raftlib_src.cpp)
    target_compile_definitions(streamcluster_ut_std PRIVATE
        -DUSE_UT=1 -DSTDALLOC=1)
    add_raftbenchmark(streamcluster_ut_dyn main.cpp raftlib_src.cpp)
    target_compile_definitions(streamcluster_ut_dyn PRIVATE -DUSE_UT=1)
    add_custom_target(streamcluster_ut)
    add_dependencies(streamcluster_ut streamcluster_ut_std
        streamcluster_ut_dyn)
    if (NOT VL_FOUND)
      MESSAGE(STATUS "WARNING: No libvl found, skip streamcluster_ut_vl.")
    else()
      add_raftbenchmark(streamcluster_ut_vl main.cpp raftlib_src.cpp)
      target_compile_definitions(streamcluster_ut_vl PRIVATE -DUSE_UT=1 -DVL=1)
      target_link_libraries(streamcluster_ut_vl ${VL_LIBRARY})
      add_dependencies(streamcluster_ut streamcluster_ut_vl)
    endif()
  endif(NOT UT_FOUND)
endif(NOT RaftLib_FOUND)
