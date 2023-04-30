project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()
if(CAF_FOUND)
  include_directories(${CAF_INCLUDE_DIR})
endif()

if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
  add_microbenchmark(sweep2d_zmq ember.cpp)
  target_compile_definitions(sweep2d_zmq PRIVATE -DZMQ -DEMBER_SWEEP2D)
  target_include_directories(sweep2d_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_link_libraries(sweep2d_zmq ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(sweep2d_zmq PRIVATE -static -pthread)
  endif()
  add_microbenchmark(halo2d_zmq ember.cpp)
  target_compile_definitions(halo2d_zmq PRIVATE -DZMQ -DEMBER_HALO2D)
  target_include_directories(halo2d_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_link_libraries(halo2d_zmq ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(halo2d_zmq PRIVATE -static -pthread)
  endif()
  add_microbenchmark(incast_zmq ember.cpp)
  target_compile_definitions(incast_zmq PRIVATE -DZMQ -DEMBER_INCAST)
  target_include_directories(incast_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_link_libraries(incast_zmq ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(incast_zmq PRIVATE -static -pthread)
  endif()
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
  MESSAGE(STATUS "WARNING: No zmq library, skip sweep2d_zmq, halo2d_zmq, incast_zmq.")
endif()

if(NOT Boost_FOUND OR NOT Boost_LOCKFREE_QUEUE_HPP)
  MESSAGE(STATUS "WARNING: No boost/lockfree/queue.hpp, skip sweep2d_boost, halo2d_boost, incast_vl.")
else()
  add_microbenchmark(sweep2d_boost ember.cpp)
  target_link_libraries(sweep2d_boost ${Boost_LIBRARIES})
  target_compile_definitions(sweep2d_boost PRIVATE -DBOOST -DEMBER_SWEEP2D)
  add_microbenchmark(halo2d_boost ember.cpp)
  target_link_libraries(halo2d_boost ${Boost_LIBRARIES})
  target_compile_definitions(halo2d_boost PRIVATE -DBOOST -DEMBER_HALO2D)
  add_microbenchmark(incast_boost ember.cpp)
  target_link_libraries(incast_boost ${Boost_LIBRARIES})
  target_compile_definitions(incast_boost PRIVATE -DBOOST -DEMBER_INCAST)
endif()

if(NOT VL_FOUND)
  MESSAGE(STATUS "WARNING: No libvl found, skip sweep2d_vl, halo2d_vl, incast_vl.")
else()
  add_microbenchmark(sweep2d_vl ember.cpp)
  target_compile_definitions(sweep2d_vl PRIVATE -DVL -DEMBER_SWEEP2D)
  target_link_libraries(sweep2d_vl ${VL_LIBRARY})
  add_microbenchmark(halo2d_vl ember.cpp)
  target_compile_definitions(halo2d_vl PRIVATE -DVL -DEMBER_HALO2D)
  target_link_libraries(halo2d_vl ${VL_LIBRARY})
  add_microbenchmark(incast_vl ember.cpp)
  target_compile_definitions(incast_vl PRIVATE -DVL -DEMBER_INCAST)
  target_link_libraries(incast_vl ${VL_LIBRARY})
  add_microbenchmark(sweep2d_il ember.cpp)
  target_compile_definitions(sweep2d_il PRIVATE -DVL -DEMBER_SWEEP2D
                             -DVLINLINE -DNOSYSVL)
  target_link_libraries(sweep2d_il ${VLI_LIBRARY})
  add_microbenchmark(halo2d_il ember.cpp)
  target_compile_definitions(halo2d_il PRIVATE -DVL -DEMBER_HALO2D
                             -DVLINLINE -DNOSYSVL)
  target_link_libraries(halo2d_il ${VLI_LIBRARY})
  add_microbenchmark(incast_il ember.cpp)
  target_compile_definitions(incast_il PRIVATE -DVL -DEMBER_INCAST
                             -DVLINLINE -DNOSYSVL)
  target_link_libraries(incast_il ${VLI_LIBRARY})
endif()

if(NOT CAF_FOUND)
  MESSAGE(STATUS "WARNING: No libcaf found, skip sweep2d_qmd, halo2d_qmd, incast_qmd.")
else()
  add_microbenchmark(sweep2d_qmd ember.cpp)
  target_compile_definitions(sweep2d_qmd PRIVATE -DCAF -DEMBER_SWEEP2D)
  target_link_libraries(sweep2d_qmd ${CAF_LIBRARY})
  add_microbenchmark(halo2d_qmd ember.cpp)
  target_compile_definitions(halo2d_qmd PRIVATE -DCAF -DEMBER_HALO2D)
  target_link_libraries(halo2d_qmd ${CAF_LIBRARY})
  add_microbenchmark(incast_qmd ember.cpp)
  target_compile_definitions(incast_qmd PRIVATE -DCAF -DEMBER_INCAST)
  target_link_libraries(incast_qmd ${CAF_LIBRARY})
  add_microbenchmark(sweep2d_caf ember.cpp)
  target_compile_definitions(sweep2d_caf PRIVATE -DCAF -DCAF_PREPUSH=1
                             -DEMBER_SWEEP2D)
  target_link_libraries(sweep2d_caf ${CAF_LIBRARY})
  add_microbenchmark(halo2d_caf ember.cpp)
  target_compile_definitions(halo2d_caf PRIVATE -DCAF -DCAF_PREPUSH=1
                             -DEMBER_HALO2D)
  target_link_libraries(halo2d_caf ${CAF_LIBRARY})
  add_microbenchmark(incast_caf ember.cpp)
  target_compile_definitions(incast_caf PRIVATE -DCAF -DCAF_PREPUSH=1
                             -DEMBER_INCAST)
  target_link_libraries(incast_caf ${CAF_LIBRARY})
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip outcast/incast_rl.")
else (NOT RaftLib_FOUND)
  add_custom_target(ember_qt)
  add_custom_target(ember_ut)
  if (NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip outcast/incast_qt.")
  else (NOT QTHREAD_FOUND)
    add_raftbenchmark(incast_qt_std_pw rl_ember.cpp)
    target_compile_definitions(incast_qt_std_pw PRIVATE -DUSE_QTHREAD=1
        -DEMBER_INCAST=1)
    add_raftbenchmark(incast_qt_dyn_pw rl_ember.cpp)
    target_compile_definitions(incast_qt_dyn_pw PRIVATE -DUSE_QTHREAD=1
        -DARMQ_DYNAMIC_ALLOC=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_qt_std_os rl_ember.cpp)
    target_compile_definitions(incast_qt_std_os PRIVATE -DUSE_QTHREAD=1
        -DRAFTLIB_ONESHOT=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_qt_std_mix rl_ember.cpp)
    target_compile_definitions(incast_qt_std_mix PRIVATE -DUSE_QTHREAD=1
        -DRAFTLIB_MIX=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_qt_dyn_mix rl_ember.cpp)
    target_compile_definitions(incast_qt_dyn_mix PRIVATE -DUSE_QTHREAD=1
        -DRAFTLIB_MIX=1 -DARMQ_DYNAMIC_ALLOC=1 -DEMBER_INCAST=1)
    add_custom_target(incast_qt)
    add_dependencies(incast_qt
                     incast_qt_std_pw incast_qt_dyn_pw incast_qt_std_os
                     incast_qt_std_mix incast_qt_dyn_mix)
    add_dependencies(ember_qt incast_qt)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip incast_qt_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(incast_qt_std_orig rl_ember.cpp)
      target_compile_definitions(incast_qt_std_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_QTHREAD=1 -DSTDALLOC=1 -DEMBER_INCAST=1)
      add_raftbenchmark(incast_qt_dyn_orig rl_ember.cpp)
      target_compile_definitions(incast_qt_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_QTHREAD=1 -DEMBER_INCAST=1)
      if (NOT VL_FOUND)
        MESSAGE(STATUS "WARNING: No libvl found, skip incast_qt_vl_orig.")
      else()
        add_raftbenchmark(incast_qt_vl_orig rl_ember.cpp)
        target_compile_definitions(incast_qt_vl_orig PRIVATE -DRAFTLIB_ORIG=1
            -DUSE_QTHREAD=1 -DVL=1 -DEMBER_INCAST=1)
        target_link_libraries(incast_qt_vl_orig ${VL_LIBRARY})
      endif()
    endif (NOT COMPILER_SUPPORTS_CXX20)

    #outcast
    add_raftbenchmark(outcast_qt_std_pw rl_ember.cpp)
    target_compile_definitions(outcast_qt_std_pw PRIVATE -DUSE_QTHREAD=1)
    add_raftbenchmark(outcast_qt_dyn_pw rl_ember.cpp)
    target_compile_definitions(outcast_qt_dyn_pw PRIVATE -DUSE_QTHREAD=1
        -DARMQ_DYNAMIC_ALLOC=1)
    add_raftbenchmark(outcast_qt_std_os rl_ember.cpp)
    target_compile_definitions(outcast_qt_std_os PRIVATE -DUSE_QTHREAD=1
        -DRAFTLIB_ONESHOT=1)
    add_raftbenchmark(outcast_qt_std_mix rl_ember.cpp)
    target_compile_definitions(outcast_qt_std_mix PRIVATE -DUSE_QTHREAD=1
        -DRAFTLIB_MIX=1)
    add_raftbenchmark(outcast_qt_dyn_mix rl_ember.cpp)
    target_compile_definitions(outcast_qt_dyn_mix PRIVATE -DUSE_QTHREAD=1
        -DRAFTLIB_MIX=1 -DARMQ_DYNAMIC_ALLOC=1)
    add_custom_target(outcast_qt)
    add_dependencies(outcast_qt
                     outcast_qt_std_pw outcast_qt_dyn_pw outcast_qt_std_os
                     outcast_qt_std_mix outcast_qt_dyn_mix)
    add_dependencies(ember_qt outcast_qt)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip outcast_qt_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(outcast_qt_std_orig rl_ember.cpp)
      target_compile_definitions(outcast_qt_std_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_QTHREAD=1 -DSTDALLOC=1)
      add_raftbenchmark(outcast_qt_dyn_orig rl_ember.cpp)
      target_compile_definitions(outcast_qt_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_QTHREAD=1)
      if (NOT VL_FOUND)
        MESSAGE(STATUS "WARNING: No libvl found, skip outcast_qt_vl_orig.")
      else()
        add_raftbenchmark(outcast_qt_vl_orig rl_ember.cpp)
        target_compile_definitions(outcast_qt_vl_orig PRIVATE -DRAFTLIB_ORIG=1
            -DUSE_QTHREAD=1 -DVL=1)
        target_link_libraries(outcast_qt_vl_orig ${VL_LIBRARY})
      endif()
    endif (NOT COMPILER_SUPPORTS_CXX20)

  endif(NOT QTHREAD_FOUND)

  if (NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip outcast/incast_ut.")
  else (NOT UT_FOUND)
    add_raftbenchmark(incast_ut_std_pw rl_ember.cpp)
    target_compile_definitions(incast_ut_std_pw PRIVATE -DUSE_UT=1
        -DEMBER_INCAST=1)
    add_raftbenchmark(incast_ut_dyn_pw rl_ember.cpp)
    target_compile_definitions(incast_ut_dyn_pw PRIVATE -DUSE_UT=1
        -DARMQ_DYNAMIC_ALLOC=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_ut_std_cv rl_ember.cpp)
    target_compile_definitions(incast_ut_std_cv PRIVATE -DUSE_UT=1
        -DRAFTLIB_CV=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_ut_std_os rl_ember.cpp)
    target_compile_definitions(incast_ut_std_os PRIVATE -DUSE_UT=1
        -DRAFTLIB_ONESHOT=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_ut_std_mix rl_ember.cpp)
    target_compile_definitions(incast_ut_std_mix PRIVATE -DUSE_UT=1
        -DRAFTLIB_MIX=1 -DEMBER_INCAST=1)
    add_raftbenchmark(incast_ut_dyn_mix rl_ember.cpp)
    target_compile_definitions(incast_ut_dyn_mix PRIVATE -DUSE_UT=1
        -DRAFTLIB_MIX=1 -DARMQ_DYNAMIC_ALLOC=1 -DEMBER_INCAST=1)
    add_custom_target(incast_ut)
    add_dependencies(incast_ut
                     incast_ut_std_pw incast_ut_dyn_pw incast_ut_std_cv
                     incast_ut_std_os incast_ut_std_mix incast_ut_dyn_mix)
    add_dependencies(ember_ut incast_ut)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip incast_ut_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(incast_ut_std_orig rl_ember.cpp)
      target_compile_definitions(incast_ut_std_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_UT=1 -DSTDALLOC=1 -DEMBER_INCAST=1)
      add_raftbenchmark(incast_ut_dyn_orig rl_ember.cpp)
      target_compile_definitions(incast_ut_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_UT=1 -DEMBER_INCAST=1)
      if (NOT VL_FOUND)
        MESSAGE(STATUS "WARNING: No libvl found, skip incast_ut_vl.")
      else()
        add_raftbenchmark(incast_ut_vl_orig rl_ember.cpp)
        target_compile_definitions(incast_ut_vl_orig PRIVATE -DRAFTLIB_ORIG=1
            -DUSE_UT=1 -DVL=1 -DEMBER_INCAST=1)
        target_link_libraries(incast_ut_vl_orig ${VL_LIBRARY})
      endif()
    endif (NOT COMPILER_SUPPORTS_CXX20)

    #outcast
    add_raftbenchmark(outcast_ut_std_pw rl_ember.cpp)
    target_compile_definitions(outcast_ut_std_pw PRIVATE -DUSE_UT=1)
    add_raftbenchmark(outcast_ut_dyn_pw rl_ember.cpp)
    target_compile_definitions(outcast_ut_dyn_pw PRIVATE -DUSE_UT=1
        -DARMQ_DYNAMIC_ALLOC=1)
    add_raftbenchmark(outcast_ut_std_cv rl_ember.cpp)
    target_compile_definitions(outcast_ut_std_cv PRIVATE -DUSE_UT=1
        -DRAFTLIB_CV=1)
    add_raftbenchmark(outcast_ut_std_os rl_ember.cpp)
    target_compile_definitions(outcast_ut_std_os PRIVATE -DUSE_UT=1
        -DRAFTLIB_ONESHOT=1)
    add_raftbenchmark(outcast_ut_std_mix rl_ember.cpp)
    target_compile_definitions(outcast_ut_std_mix PRIVATE -DUSE_UT=1
        -DRAFTLIB_MIX=1)
    add_raftbenchmark(outcast_ut_dyn_mix rl_ember.cpp)
    target_compile_definitions(outcast_ut_dyn_mix PRIVATE -DUSE_UT=1
        -DRAFTLIB_MIX=1 -DARMQ_DYNAMIC_ALLOC=1)
    add_custom_target(outcast_ut)
    add_dependencies(outcast_ut
                     outcast_ut_std_pw outcast_ut_dyn_pw outcast_ut_std_cv
                     outcast_ut_std_os outcast_ut_std_mix outcast_ut_dyn_mix)
    add_dependencies(ember_ut outcast_ut)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip outcast_ut_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(outcast_ut_std_orig rl_ember.cpp)
      target_compile_definitions(outcast_ut_std_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_UT=1 -DSTDALLOC=1)
      add_raftbenchmark(outcast_ut_dyn_orig rl_ember.cpp)
      target_compile_definitions(outcast_ut_dyn_orig PRIVATE -DRAFTLIB_ORIG=1
          -DUSE_UT=1)
      if (NOT VL_FOUND)
        MESSAGE(STATUS "WARNING: No libvl found, skip outcast_ut_vl_orig.")
      else()
        add_raftbenchmark(outcast_ut_vl_orig rl_ember.cpp)
        target_compile_definitions(outcast_ut_vl_orig PRIVATE -DRAFTLIB_ORIG=1
            -DUSE_UT=1 -DVL=1)
        target_link_libraries(outcast_ut_vl_orig ${VL_LIBRARY})
      endif()
    endif (NOT COMPILER_SUPPORTS_CXX20)
  endif(NOT UT_FOUND)
endif()
