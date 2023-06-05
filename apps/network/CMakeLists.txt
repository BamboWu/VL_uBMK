project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()
if(CAF_FOUND)
  include_directories(${CAF_INCLUDE_DIR})
endif()

if(NOT VL_FOUND)
  MESSAGE(STATUS "WARNING: No libvl found, skip pipeline_vl.")
  MESSAGE(STATUS "WARNING: No libvl found, skip firewall_vl.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_vl.")
  MESSAGE(STATUS "WARNING: No atomic library, skip firewall_vl.")
else()
  add_microbenchmark(pipeline_vl pipeline.cpp)
  target_compile_definitions(pipeline_vl PRIVATE -DVL
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_vl ${VL_LIBRARY})
  add_microbenchmark(firewall_vl firewall.cpp)
  target_compile_definitions(firewall_vl PRIVATE -DVL
                             -DNUM_STAGE2=4
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=56)
  target_link_libraries(firewall_vl ${VL_LIBRARY})
  add_microbenchmark(pipeline_il pipeline.cpp)
  target_compile_definitions(pipeline_il PRIVATE -DVL -DVLINLINE -DNOSYSVL
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_il ${VL_LIBRARY})
  add_microbenchmark(firewall_il firewall.cpp)
  target_compile_definitions(firewall_il PRIVATE -DVL -DVLINLINE -DNOSYSVL
                             -DNUM_STAGE2=4
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=56)
  target_link_libraries(firewall_il ${VL_LIBRARY})
endif()

if(NOT CAF_FOUND)
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_caf.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip firewall_qmd.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip firewall_caf.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_caf.")
  MESSAGE(STATUS "WARNING: No atomic library, skip firewall_qmd.")
  MESSAGE(STATUS "WARNING: No atomic library, skip firewall_caf.")
else()
  add_microbenchmark(pipeline_qmd pipeline.cpp)
  target_compile_definitions(pipeline_qmd PRIVATE -DCAF=1
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=64)
  target_link_libraries(pipeline_qmd ${CAF_LIBRARY})

  add_microbenchmark(pipeline_caf pipeline.cpp)
  target_compile_definitions(pipeline_caf PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=64)
  target_link_libraries(pipeline_caf ${CAF_LIBRARY})

  add_microbenchmark(firewall_qmd firewall.cpp)
  target_compile_definitions(firewall_qmd PRIVATE -DCAF=1
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=56)
  target_link_libraries(firewall_qmd ${CAF_LIBRARY})

  add_microbenchmark(firewall_caf firewall.cpp)
  target_compile_definitions(firewall_caf PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=56)
  target_link_libraries(firewall_caf ${CAF_LIBRARY})
endif()

if(NOT Boost_FOUND OR NOT Boost_LOCKFREE_QUEUE_HPP)
  MESSAGE(STATUS "WARNING: No boost/lockfree/queue.hpp, skip pipeline_boost.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_boost.")
else()
  add_microbenchmark(pipeline_boost pipeline.cpp)
  target_compile_definitions(pipeline_boost PRIVATE -DBLFQ=1
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_boost ${Boost_LIBRARIES})
  add_microbenchmark(firewall_boost firewall.cpp)
  target_compile_definitions(firewall_boost PRIVATE -DBLFQ=1
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=56)
  target_link_libraries(firewall_boost ${Boost_LIBRARIES})
endif()

if(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
  MESSAGE(STATUS "WARNING: No zmq library, skip pipeline_zmq.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_zmq.")
else()
  add_microbenchmark(pipeline_zmq pipeline.cpp)
  target_include_directories(pipeline_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_compile_definitions(pipeline_zmq PRIVATE -DZMQ=1
                             -DNUM_STAGE1=1 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_zmq ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(pipeline_zmq PRIVATE -static -pthread)
  endif()
  add_microbenchmark(firewall_zmq firewall.cpp)
  target_include_directories(firewall_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_compile_definitions(firewall_zmq PRIVATE -DZMQ=1
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=56)
  target_link_libraries(firewall_zmq ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(firewall_zmq PRIVATE -static -pthread)
  endif()
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip pipeline_rl.")
else (NOT RaftLib_FOUND)
  add_custom_target(network_qt)
  add_custom_target(network_ut)
  if (NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip pipeline_qt.")
  else (NOT QTHREAD_FOUND)
    add_raftbenchmark(pipeline_qt_std_pw rl_pipeline.cpp)
    target_compile_definitions(pipeline_qt_std_pw PRIVATE
        -DUSE_QTHREAD=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_qt_dyn_pw rl_pipeline.cpp)
    target_compile_definitions(pipeline_qt_dyn_pw PRIVATE
        -DUSE_QTHREAD=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_qt_std_os rl_pipeline.cpp)
    target_compile_definitions(pipeline_qt_std_os PRIVATE
        -DRAFTLIB_ONESHOT=1 -DUSE_QTHREAD=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_qt_std_mix rl_pipeline.cpp)
    target_compile_definitions(pipeline_qt_std_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_QTHREAD=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_qt_dyn_mix rl_pipeline.cpp)
    target_compile_definitions(pipeline_qt_dyn_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_QTHREAD=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)

    add_custom_target(pipeline_qt)
    add_dependencies(pipeline_qt
                     pipeline_qt_std_pw pipeline_qt_dyn_pw pipeline_qt_std_os
                     pipeline_qt_std_mix pipeline_qt_dyn_mix)
    add_dependencies(network_qt pipeline_qt)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip pipeline_qt_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(pipeline_qt_std_orig rl_pipeline.cpp)
      target_compile_definitions(pipeline_qt_std_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_QTHREAD=1 -DSTDALLOC=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
      add_raftbenchmark(pipeline_qt_dyn_orig rl_pipeline.cpp)
      target_compile_definitions(pipeline_qt_dyn_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_QTHREAD=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    endif (NOT COMPILER_SUPPORTS_CXX20)
  endif(NOT QTHREAD_FOUND)
  if (NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip pipeline_ut.")
  else (NOT UT_FOUND)
    add_raftbenchmark(pipeline_ut_std_pw rl_pipeline.cpp)
    target_compile_definitions(pipeline_ut_std_pw PRIVATE
        -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_ut_dyn_pw rl_pipeline.cpp)
    target_compile_definitions(pipeline_ut_dyn_pw PRIVATE
        -DUSE_UT=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_ut_std_os rl_pipeline.cpp)
    target_compile_definitions(pipeline_ut_std_os PRIVATE
        -DRAFTLIB_ONESHOT=1 -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_ut_std_mix rl_pipeline.cpp)
    target_compile_definitions(pipeline_ut_std_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_ut_dyn_mix rl_pipeline.cpp)
    target_compile_definitions(pipeline_ut_dyn_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_UT=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(pipeline_ut_std_cv rl_pipeline.cpp)
    target_compile_definitions(pipeline_ut_std_cv PRIVATE
        -DRAFTLIB_CV=1 -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)

    add_custom_target(pipeline_ut)
    add_dependencies(pipeline_ut
                     pipeline_ut_std_pw pipeline_ut_dyn_pw pipeline_ut_std_os
                     pipeline_ut_std_mix pipeline_ut_dyn_mix
                     pipeline_ut_std_cv)
    add_dependencies(network_ut pipeline_ut)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip pipeline_ut_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(pipeline_ut_std_orig rl_pipeline.cpp)
      target_compile_definitions(pipeline_ut_std_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_UT=1 -DSTDALLOC=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
      add_raftbenchmark(pipeline_ut_dyn_orig rl_pipeline.cpp)
      target_compile_definitions(pipeline_ut_dyn_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_UT=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    endif (NOT COMPILER_SUPPORTS_CXX20)
  endif(NOT UT_FOUND)
endif()

if (NOT RaftLib_FOUND)
  MESSAGE(STATUS "WARNING: No RaftLib found, skip firewall_rl.")
else (NOT RaftLib_FOUND)
  if (NOT QTHREAD_FOUND)
    MESSAGE(STATUS "WARNING: No qthread found, skip firewall_qt.")
  else (NOT QTHREAD_FOUND)
    add_raftbenchmark(firewall_qt_std_pw rl_firewall.cpp)
    target_compile_definitions(firewall_qt_std_pw PRIVATE
        -DUSE_QTHREAD=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_qt_dyn_pw rl_firewall.cpp)
    target_compile_definitions(firewall_qt_dyn_pw PRIVATE
        -DUSE_QTHREAD=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_qt_std_os rl_firewall.cpp)
    target_compile_definitions(firewall_qt_std_os PRIVATE
        -DRAFTLIB_ONESHOT=1 -DUSE_QTHREAD=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_qt_std_mix rl_firewall.cpp)
    target_compile_definitions(firewall_qt_std_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_QTHREAD=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_qt_dyn_mix rl_firewall.cpp)
    target_compile_definitions(firewall_qt_dyn_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_QTHREAD=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)

    add_custom_target(firewall_qt)
    add_dependencies(firewall_qt
                     firewall_qt_std_pw firewall_qt_dyn_pw firewall_qt_std_os
                     firewall_qt_std_mix firewall_qt_dyn_mix)
    add_dependencies(network_qt firewall_qt)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip firewall_qt_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(firewall_qt_std_orig rl_firewall.cpp)
      target_compile_definitions(firewall_qt_std_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_QTHREAD=1 -DSTDALLOC=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
      add_raftbenchmark(firewall_qt_dyn_orig rl_firewall.cpp)
      target_compile_definitions(firewall_qt_dyn_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_QTHREAD=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    endif (NOT COMPILER_SUPPORTS_CXX20)
  endif(NOT QTHREAD_FOUND)
  if (NOT UT_FOUND)
    MESSAGE(STATUS "WARNING: No libut found, skip firewall_ut.")
  else (NOT UT_FOUND)
    add_raftbenchmark(firewall_ut_std_pw rl_firewall.cpp)
    target_compile_definitions(firewall_ut_std_pw PRIVATE
        -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_ut_dyn_pw rl_firewall.cpp)
    target_compile_definitions(firewall_ut_dyn_pw PRIVATE
        -DUSE_UT=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_ut_std_os rl_firewall.cpp)
    target_compile_definitions(firewall_ut_std_os PRIVATE
        -DRAFTLIB_ONESHOT=1 -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_ut_std_mix rl_firewall.cpp)
    target_compile_definitions(firewall_ut_std_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_ut_dyn_mix rl_firewall.cpp)
    target_compile_definitions(firewall_ut_dyn_mix PRIVATE
        -DRAFTLIB_MIX=1 -DUSE_UT=1 -DARMQ_DYNAMIC_ALLOC=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    add_raftbenchmark(firewall_ut_std_cv rl_firewall.cpp)
    target_compile_definitions(firewall_ut_std_cv PRIVATE
        -DRAFTLIB_CV=1 -DUSE_UT=1 -DPOOL_SIZE=420
        -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)

    add_custom_target(firewall_ut)
    add_dependencies(firewall_ut
                     firewall_ut_std_pw firewall_ut_dyn_pw firewall_ut_std_os
                     firewall_ut_std_mix firewall_ut_dyn_mix
                     firewall_ut_std_cv)
    add_dependencies(network_ut firewall_ut)
    if (NOT COMPILER_SUPPORTS_CXX20)
      MESSAGE(STATUS "WARNING: No C++20 support, skip firewall_ut_*_orig")
    else (NOT COMPILER_SUPPORTS_CXX20)
      add_raftbenchmark(firewall_ut_std_orig rl_firewall.cpp)
      target_compile_definitions(firewall_ut_std_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_UT=1 -DSTDALLOC=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
      add_raftbenchmark(firewall_ut_dyn_orig rl_firewall.cpp)
      target_compile_definitions(firewall_ut_dyn_orig PRIVATE
          -DRAFTLIB_ORIG=1 -DUSE_UT=1 -DPOOL_SIZE=420
          -DSTAGE1_READ=1 -DSTAGE1_WRITE=1 -DSTAGE2_READ=1 -DSTAGE2_WRITE=1)
    endif (NOT COMPILER_SUPPORTS_CXX20)
  endif(NOT UT_FOUND)
endif()
