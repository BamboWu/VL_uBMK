CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(fluidanimate CXX)

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip fluidanimate.")
else (NOT RaftLib_FOUND)
  if(NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip fluidanimate_qt.")
  else(NOT QTHREAD_FOUND)
    add_raftbenchmark(fluidanimate_qt_std main.cpp raftlib_src.cpp
                      fluidcmp.cpp cellpool.cpp)
    target_compile_definitions(fluidanimate_qt_std PRIVATE
        -DUSEQTHREAD=1 -DSTDALLOC=1 -DUSE_MUTEX)
    add_raftbenchmark(fluidanimate_qt_dyn main.cpp raftlib_src.cpp
                      fluidcmp.cpp cellpool.cpp)
    target_compile_definitions(fluidanimate_qt_dyn PRIVATE
        -DUSEQTHREAD=1 -DUSE_MUTEX)
    add_custom_target(fluidanimate_qt)
    add_dependencies(fluidanimate_qt fluidanimate_qt_std fluidanimate_qt_dyn)
    if (NOT VL_FOUND)
      MESSAGE(STATUS "WARNING: No libvl found, skip fluidanimate_qt_vl.")
    else()
      add_raftbenchmark(fluidanimate_qt_vl main.cpp raftlib_src.cpp
                        fluidcmp.cpp cellpool.cpp)
      target_compile_definitions(fluidanimate_qt_vl PRIVATE
          -DUSEQTHREAD=1 -DVL=1 -DUSE_MUTEX)
      target_link_libraries(fluidanimate_qt_vl ${VL_LIBRARY})
      add_dependencies(fluidanimate_qt fluidanimate_qt_vl)
    endif()
  endif(NOT QTHREAD_FOUND)
  if(NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip fluidanimate_ut.")
  else(NOT UT_FOUND)
    add_raftbenchmark(fluidanimate_ut_std main.cpp raftlib_src.cpp
                      fluidcmp.cpp cellpool.cpp)
    target_compile_definitions(fluidanimate_ut_std PRIVATE
        -DUSEUT=1 -DSTDALLOC=1 -DUSE_MUTEX)
    add_raftbenchmark(fluidanimate_ut_dyn main.cpp raftlib_src.cpp
                      fluidcmp.cpp cellpool.cpp)
    target_compile_definitions(fluidanimate_ut_dyn PRIVATE
        -DUSEUT=1 -DUSE_MUTEX)
    add_custom_target(fluidanimate_ut)
    add_dependencies(fluidanimate_ut fluidanimate_ut_std fluidanimate_ut_dyn)
    if (NOT VL_FOUND)
      MESSAGE(STATUS "WARNING: No libvl found, skip fluidanimate_ut_vl.")
    else()
      add_raftbenchmark(fluidanimate_ut_vl main.cpp raftlib_src.cpp
                        fluidcmp.cpp cellpool.cpp)
      target_compile_definitions(fluidanimate_ut_vl PRIVATE
          -DUSEUT=1 -DVL=1 -DUSE_MUTEX)
      target_link_libraries(fluidanimate_ut_vl ${VL_LIBRARY})
      add_dependencies(fluidanimate_ut fluidanimate_ut_vl)
    endif()
  endif(NOT UT_FOUND)
endif(NOT RaftLib_FOUND)
