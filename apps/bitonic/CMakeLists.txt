project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

add_microbenchmark(bitonic_seq seq.cpp)

if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
    add_microbenchmark(bitonic_zmq zmq.cpp)
    target_include_directories(bitonic_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
    target_link_libraries(bitonic_zmq ${ZMQ_LIBRARY})
    if (ZMQ_STATIC_FOUND)
      target_compile_options(bitonic_zmq PRIVATE -static -pthread)
    endif()
    if(NOT Boost_THREAD_FOUND)
      # fall back to use pthread library find in the top CMakeLists.txt
      target_compile_definitions(bitonic_zmq PRIVATE -DSTDTHREAD)
    endif()
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
    MESSAGE(STATUS "WARNING: No zmq library, skip bitonic_zmq.")
endif()

if(NOT Boost_LOCKFREE_QUEUE_HPP)
  MESSAGE(STATUS "WARNING: No boost/lockfree/queue.hpp, skip bitonic_boost.")
else()
  add_microbenchmark(bitonic_boost boost.cpp)
  target_link_libraries(bitonic_boost ${Boost_LIBRARIES})
  if(NOT Boost_THREAD_FOUND)
    # fall back to use pthread library find in the top CMakeLists.txt
    target_compile_definitions(bitonic_boost PRIVATE -DSTDTHREAD)
  endif()
endif()
