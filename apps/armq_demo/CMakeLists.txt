CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(armq_demo CXX)

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip armq_demo.")
else (NOT RaftLib_FOUND)
  if (NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip demo_qt.")
  else (NOT QTHREAD_FOUND)
    add_raftbenchmark(demo0_qt_std_orig armq_demo.cpp)
    target_compile_definitions(demo0_qt_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1 -DSTDALLOC=1)
    add_raftbenchmark(demo0_qt_dyn_orig armq_demo.cpp)
    target_compile_definitions(demo0_qt_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1)
    add_raftbenchmark(demo0_qt_std_armq armq_demo.cpp)
    target_compile_definitions(demo0_qt_std_armq PRIVATE -DUSE_QTHREAD=1)
    add_raftbenchmark(demo0_qt_dyn_armq armq_demo.cpp)
    target_compile_definitions(demo0_qt_dyn_armq PRIVATE -DUSE_QTHREAD=1
        -DARMQ_DYNAMIC_ALLOC=1)

    add_raftbenchmark(demo1_qt_std_orig armq_demo.cpp)
    target_compile_definitions(demo1_qt_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1 -DSTDALLOC=1 -DDUPLICATE_FILTERS=1)
    add_raftbenchmark(demo1_qt_dyn_orig armq_demo.cpp)
    target_compile_definitions(demo1_qt_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1 -DDUPLICATE_FILTERS=1)
    add_raftbenchmark(demo1_qt_std_armq armq_demo.cpp)
    target_compile_definitions(demo1_qt_std_armq PRIVATE -DUSE_QTHREAD=1
        -DDUPLICATE_FILTERS=1)
    add_raftbenchmark(demo1_qt_dyn_armq armq_demo.cpp)
    target_compile_definitions(demo1_qt_dyn_armq PRIVATE -DUSE_QTHREAD=1
        -DDUPLICATE_FILTERS=1 -DARMQ_DYNAMIC_ALLOC=1)
    add_custom_target(demo_qt)
    add_dependencies(demo_qt
                     demo0_qt_std_orig demo0_qt_dyn_orig
                     demo0_qt_std_armq demo0_qt_dyn_armq
                     demo1_qt_std_orig demo1_qt_dyn_orig
                     demo1_qt_std_armq demo1_qt_dyn_armq)
  endif(NOT QTHREAD_FOUND)
  if (NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip search_ut.")
  else (NOT UT_FOUND)
    add_raftbenchmark(demo0_ut_std_orig armq_demo.cpp)
    target_compile_definitions(demo0_ut_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1 -DSTDALLOC=1)
    add_raftbenchmark(demo0_ut_dyn_orig armq_demo.cpp)
    target_compile_definitions(demo0_ut_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1)
    add_raftbenchmark(demo0_ut_std_armq armq_demo.cpp)
    target_compile_definitions(demo0_ut_std_armq PRIVATE -DUSE_UT=1)
    add_raftbenchmark(demo0_ut_dyn_armq armq_demo.cpp)
    target_compile_definitions(demo0_ut_dyn_armq PRIVATE -DUSE_UT=1
        -DARMQ_DYNAMIC_ALLOC=1)

    add_raftbenchmark(demo1_ut_std_orig armq_demo.cpp)
    target_compile_definitions(demo1_ut_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1 -DSTDALLOC=1 -DDUPLICATE_FILTERS=1)
    add_raftbenchmark(demo1_ut_dyn_orig armq_demo.cpp)
    target_compile_definitions(demo1_ut_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1 -DDUPLICATE_FILTERS=1)
    add_raftbenchmark(demo1_ut_std_armq armq_demo.cpp)
    target_compile_definitions(demo1_ut_std_armq PRIVATE -DUSE_UT=1
        -DDUPLICATE_FILTERS=1)
    add_raftbenchmark(demo1_ut_dyn_armq armq_demo.cpp)
    target_compile_definitions(demo1_ut_dyn_armq PRIVATE -DUSE_UT=1
        -DDUPLICATE_FILTERS=1 -DARMQ_DYNAMIC_ALLOC=1)
    add_custom_target(demo_ut)
    add_dependencies(demo_ut
                     demo0_ut_std_orig demo0_ut_dyn_orig
                     demo0_ut_std_armq demo0_ut_dyn_armq
                     demo1_ut_std_orig demo1_ut_dyn_orig
                     demo1_ut_std_armq demo1_ut_dyn_armq)
  endif(NOT UT_FOUND)
endif()
