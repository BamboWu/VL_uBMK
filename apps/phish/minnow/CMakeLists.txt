project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

set( MINNOWS
    count
    file2words
    filegen
    print
    sort
    )

if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
  foreach(MINNOW ${MINNOWS})
    add_executable(${MINNOW} ${MINNOW}.cpp)
    target_compile_options(${MINNOW} PRIVATE -static -pthread)
    target_link_libraries(${MINNOW} phish-zmq)
    if(CMAKE_THREAD_LIBS_INIT)
      target_link_libraries(${MINNOW} "${CMAKE_THREAD_LIBS_INIT}")
    endif()
  endforeach()
  add_custom_target(minnows)
  add_dependencies(minnows ${MINNOWS})
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
    MESSAGE(STATUS "WARNING: No zmq library, skip all minnows.")
endif()
