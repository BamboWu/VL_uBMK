project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

add_microbenchmark(bitonic_seq seq.cpp)

if((ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND) AND GCCLIBATOMIC_FOUND)
  add_microbenchmark(bitonic_zmq_1S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_zmq_3S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_zmq_7S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_zmq_15S_Exp10F32 bitonic.cpp utils.cpp)
  target_include_directories(bitonic_zmq_1S_Exp10F32 PRIVATE ${ZMQ_INCLUDE_DIR})
  target_include_directories(bitonic_zmq_3S_Exp10F32 PRIVATE ${ZMQ_INCLUDE_DIR})
  target_include_directories(bitonic_zmq_7S_Exp10F32 PRIVATE ${ZMQ_INCLUDE_DIR})
  target_include_directories(bitonic_zmq_15S_Exp10F32 PRIVATE ${ZMQ_INCLUDE_DIR})
  target_compile_definitions(bitonic_zmq_1S_Exp10F32 PRIVATE -DZMQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=1)
  target_compile_definitions(bitonic_zmq_3S_Exp10F32 PRIVATE -DZMQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=3)
  target_compile_definitions(bitonic_zmq_7S_Exp10F32 PRIVATE -DZMQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=7)
  target_compile_definitions(bitonic_zmq_15S_Exp10F32 PRIVATE -DZMQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=15)
  target_link_libraries(bitonic_zmq_1S_Exp10F32 ${ZMQ_LIBRARY})
  target_link_libraries(bitonic_zmq_3S_Exp10F32 ${ZMQ_LIBRARY})
  target_link_libraries(bitonic_zmq_7S_Exp10F32 ${ZMQ_LIBRARY})
  target_link_libraries(bitonic_zmq_15S_Exp10F32 ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(bitonic_zmq_1S_Exp10F32 PRIVATE -static -pthread)
    target_compile_options(bitonic_zmq_3S_Exp10F32 PRIVATE -static -pthread)
    target_compile_options(bitonic_zmq_7S_Exp10F32 PRIVATE -static -pthread)
    target_compile_options(bitonic_zmq_15S_Exp10F32 PRIVATE -static -pthread)
  endif()
  if(NOT Boost_THREAD_FOUND)
    # fall back to use pthread library find in the top CMakeLists.txt
    target_compile_definitions(bitonic_zmq_1S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_zmq_3S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_zmq_7S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_zmq_15S_Exp10F32 PRIVATE -DSTDTHREAD)
  else()
    target_link_libraries(bitonic_zmq_1S_Exp10F32 ${Boost_LIBRARIES})
    target_link_libraries(bitonic_zmq_3S_Exp10F32 ${Boost_LIBRARIES})
    target_link_libraries(bitonic_zmq_7S_Exp10F32 ${Boost_LIBRARIES})
    target_link_libraries(bitonic_zmq_15S_Exp10F32 ${Boost_LIBRARIES})
  endif()
  add_custom_target(bitonic_zmq)
  add_dependencies(bitonic_zmq
                   bitonic_zmq_1S_Exp10F32
                   bitonic_zmq_3S_Exp10F32
                   bitonic_zmq_7S_Exp10F32
                   bitonic_zmq_15S_Exp10F32)
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
  MESSAGE(STATUS "WARNING: No zmq library, skip bitonic_zmq.")
else()
  MESSAGE(STATUS "WARNING: No atomic library, skip bitonic_zmq.")
endif()

if(NOT Boost_LOCKFREE_QUEUE_HPP)
  MESSAGE(STATUS "WARNING: No boost/lockfree/queue.hpp, skip bitonic_boost.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip bitonic_boost.")
else()
  add_microbenchmark(bitonic_boost_1S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_boost_3S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_boost_7S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_boost_15S_Exp10F32 bitonic.cpp utils.cpp)
  target_link_libraries(bitonic_boost_1S_Exp10F32 ${Boost_LIBRARIES})
  target_link_libraries(bitonic_boost_3S_Exp10F32 ${Boost_LIBRARIES})
  target_link_libraries(bitonic_boost_7S_Exp10F32 ${Boost_LIBRARIES})
  target_link_libraries(bitonic_boost_15S_Exp10F32 ${Boost_LIBRARIES})
  target_compile_definitions(bitonic_boost_1S_Exp10F32 PRIVATE -DBLFQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=1)
  target_compile_definitions(bitonic_boost_3S_Exp10F32 PRIVATE -DBLFQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=3)
  target_compile_definitions(bitonic_boost_7S_Exp10F32 PRIVATE -DBLFQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=7)
  target_compile_definitions(bitonic_boost_15S_Exp10F32 PRIVATE -DBLFQ=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=15)
  if(NOT Boost_THREAD_FOUND)
    # fall back to use pthread library find in the top CMakeLists.txt
    target_compile_definitions(bitonic_boost_1S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_boost_3S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_boost_7S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_boost_15S_Exp10F32 PRIVATE -DSTDTHREAD)
  endif()
  add_custom_target(bitonic_boost)
  add_dependencies(bitonic_boost
                   bitonic_boost_1S_Exp10F32
                   bitonic_boost_3S_Exp10F32
                   bitonic_boost_7S_Exp10F32
                   bitonic_boost_15S_Exp10F32)
endif()

if(NOT VL_FOUND)
  MESSAGE(STATUS "WARNING: No libvl found, skip bitonic_vl.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip bitonic_vl.")
else()
  add_microbenchmark(bitonic_vl_1S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_vl_3S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_vl_7S_Exp10F32 bitonic.cpp utils.cpp)
  add_microbenchmark(bitonic_vl_15S_Exp10F32 bitonic.cpp utils.cpp)
  target_link_libraries(bitonic_vl_1S_Exp10F32 ${VL_LIBRARY})
  target_link_libraries(bitonic_vl_3S_Exp10F32 ${VL_LIBRARY})
  target_link_libraries(bitonic_vl_7S_Exp10F32 ${VL_LIBRARY})
  target_link_libraries(bitonic_vl_15S_Exp10F32 ${VL_LIBRARY})
  target_compile_definitions(bitonic_vl_1S_Exp10F32 PRIVATE -DVL=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=1)
  target_compile_definitions(bitonic_vl_3S_Exp10F32 PRIVATE -DVL=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=3)
  target_compile_definitions(bitonic_vl_7S_Exp10F32 PRIVATE -DVL=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=7)
  target_compile_definitions(bitonic_vl_15S_Exp10F32 PRIVATE -DVL=1
                             -DMINI_TASK_EXP=10 -DMAX_ON_THE_FLY=32
                             -DNUM_SLAVES=15)
  if(NOT Boost_THREAD_FOUND)
    # fall back to use pthread library find in the top CMakeLists.txt
    target_compile_definitions(bitonic_vl_1S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_vl_3S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_vl_7S_Exp10F32 PRIVATE -DSTDTHREAD)
    target_compile_definitions(bitonic_vl_15S_Exp10F32 PRIVATE -DSTDTHREAD)
  endif()
  add_custom_target(bitonic_vl)
  add_dependencies(bitonic_vl
                   bitonic_vl_1S_Exp10F32
                   bitonic_vl_3S_Exp10F32
                   bitonic_vl_7S_Exp10F32
                   bitonic_vl_15S_Exp10F32)
endif()
