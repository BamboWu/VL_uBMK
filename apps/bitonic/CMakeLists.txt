project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

find_package(Zmq QUIET)

add_microbenchmark(bitonic_seq seq.cpp)

if(ZMQ_STATIC_FOUND)
    add_microbenchmark(bitonic_zmq zmq.cpp)
    target_include_directories(bitonic_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
    target_link_libraries(bitonic_zmq ${ZMQ_LIBRARY})
    target_compile_options(bitonic_zmq PRIVATE -static -pthread)
elseif(ZMQ_DYNAMIC_FOUND)
    add_microbenchmark(bitonic_zmq zmq.cpp)
    target_include_directories(bitonic_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
    target_link_libraries(bitonic_zmq ${ZMQ_LIBRARY})
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
    MESSAGE(STATUS "WARNING: No zmq library, skip bitonic_zmq.")
endif()
