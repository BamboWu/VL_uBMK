project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_subdirectory(minnow)
set(MINNOW_DIR $(pwd)/apps/phish/minnow)

if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
    add_microbenchmark(filelist_zmq examples.cpp)
    add_microbenchmark(wordcount_zmq examples.cpp)
    target_link_libraries(filelist_zmq phish-bait-zmq)
    target_link_libraries(wordcount_zmq phish-bait-zmq)
    target_compile_definitions(filelist_zmq PRIVATE -DPHISH_EXAMPLE_FILELIST)
    target_compile_definitions(filelist_zmq PRIVATE -DMINNOW_DIR="${CMAKE_CURRENT_BINARY_DIR}/minnow/")
    target_compile_definitions(wordcount_zmq PRIVATE -DPHISH_EXAMPLE_WORDCOUNT)
    target_compile_definitions(wordcount_zmq PRIVATE -DMINNOW_DIR="${CMAKE_CURRENT_BINARY_DIR}/minnow/")
    add_dependencies(filelist_zmq minnows)
    add_dependencies(wordcount_zmq minnows)
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
    MESSAGE(STATUS "WARNING: No zmq library, skip filelist_zmq.")
endif()
