CMAKE_MINIMUM_REQUIRED (VERSION 3.10 FATAL_ERROR)

project(search CXX)

if(USE_MUTEX)
  add_definitions(-DUSE_MUTEX)
endif()

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip search.")
else (NOT RaftLib_FOUND)
  if (NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip search_qt.")
  else (NOT QTHREAD_FOUND)
    add_raftbenchmark(search_qt_std_orig bmh.cpp)
    target_compile_definitions(search_qt_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1 -DSTDALLOC=1)
    add_raftbenchmark(search_qt_dyn_orig bmh.cpp)
    target_compile_definitions(search_qt_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_QTHREAD=1)
    add_raftbenchmark(search_qt_std_pw bmh.cpp)
    target_compile_definitions(search_qt_std_pw PRIVATE -DUSE_QTHREAD=1)
    add_raftbenchmark(search_qt_std_os bmh.cpp)
    target_compile_definitions(search_qt_std_os PRIVATE -DUSE_QTHREAD=1
        -DONESHOT=1)
    add_custom_target(search_qt)
    add_dependencies(search_qt search_qt_std_orig search_qt_dyn_orig
                     search_qt_std_pw search_qt_std_os)
    if (NOT VL_FOUND)
      MESSAGE(STATUS "WARNING: No libvl found, skip search_qt_vl.")
    else()
      add_raftbenchmark(search_qt_vl_orig bmh.cpp)
      target_compile_definitions(search_qt_vl_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_QTHREAD=1 -DVL=1)
      target_link_libraries(search_qt_vl_orig ${VL_LIBRARY})
      add_dependencies(search_qt search_qt_vl_orig)
    endif()
  endif(NOT QTHREAD_FOUND)
  if (NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip search_ut.")
  else (NOT UT_FOUND)
    add_raftbenchmark(search_ut_std_orig bmh.cpp)
    target_compile_definitions(search_ut_std_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1 -DSTDALLOC=1)
    add_raftbenchmark(search_ut_dyn_orig bmh.cpp)
    target_compile_definitions(search_ut_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
        -DUSE_UT=1)
    add_custom_target(search_ut)
    add_dependencies(search_ut search_ut_std_orig search_ut_dyn_orig)
    if (NOT VL_FOUND)
      MESSAGE(STATUS "WARNING: No libvl found, skip search_ut_vl.")
    else()
      add_raftbenchmark(search_ut_vl_orig bmh.cpp)
      target_compile_definitions(search_ut_vl_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_UT=1 -DVL=1)
      target_link_libraries(search_ut_vl_orig ${VL_LIBRARY})
      add_dependencies(search_ut search_ut_vl_orig)
    endif()
  endif(NOT UT_FOUND)
endif()
