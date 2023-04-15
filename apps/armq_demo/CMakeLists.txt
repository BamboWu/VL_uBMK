CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(armq_demo CXX)

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip armq_demo.")
else (NOT RaftLib_FOUND)
  if (NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip armq_demo_qt.")
  else (NOT QTHREAD_FOUND)
    add_raftbenchmark(armq_demo_qt_std_orig armq_demo.cpp)
    target_compile_definitions(armq_demo_qt_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1 -DSTDALLOC=1)
    add_raftbenchmark(armq_demo_qt_dyn_orig armq_demo.cpp)
    target_compile_definitions(armq_demo_qt_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1)
    add_raftbenchmark(armq_demo_qt_std_armq armq_demo.cpp)
    target_compile_definitions(armq_demo_qt_std_armq PRIVATE -DUSE_QTHREAD=1)
    add_custom_target(armq_demo_qt)
    add_dependencies(armq_demo_qt armq_demo_qt_std_orig armq_demo_qt_dyn_orig
                     armq_demo_qt_std_armq)
  endif(NOT QTHREAD_FOUND)
  if (NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip search_ut.")
  else (NOT UT_FOUND)
    add_raftbenchmark(armq_demo_ut_std_orig armq_demo.cpp)
    target_compile_definitions(armq_demo_ut_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1 -DSTDALLOC=1)
    add_raftbenchmark(armq_demo_ut_dyn_orig armq_demo.cpp)
    target_compile_definitions(armq_demo_ut_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1)
    add_raftbenchmark(armq_demo_ut_std_armq armq_demo.cpp)
    target_compile_definitions(armq_demo_ut_std_armq PRIVATE -DUSE_UT=1)
    add_custom_target(armq_demo_ut)
    add_dependencies(armq_demo_ut armq_demo_ut_std_orig armq_demo_ut_dyn_orig
                     armq_demo_ut_std_armq)
  endif(NOT UT_FOUND)
endif()
