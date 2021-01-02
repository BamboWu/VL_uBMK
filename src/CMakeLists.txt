project(VL_uBMK)

set(CMAKE_REQUIRED_LIBRARIES "pthread")
check_function_exists(pthread_setaffinity_np HAS_SET_AFFINITY)
check_function_exists(pthread_setname_np HAS_SET_NAME)

add_library(uBMK_util STATIC
  threading.c
  profiling.c
  printmap.cpp
  )
target_link_libraries(uBMK_util pthread)
if(PAPI_STATIC_FOUND OR PAPI_DYNAMIC_FOUND)
  target_include_directories(uBMK_util PRIVATE ${PAPI_INCLUDE_DIR})
  target_link_libraries(uBMK_util ${PAPI_LIBRARY})
else()
  add_definitions(-DNOPAPI)
endif()

if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
  add_library(phish-zmq STATIC
    phish/hashlittle.cpp
    phish/phish-common.cpp
    phish/phish-zmq.cpp)
  add_library(phish-bait-zmq STATIC
    phish/phish-bait-common.cpp
    phish/phish-bait-zmq.cpp)
  target_include_directories(phish-zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_include_directories(phish-zmq PRIVATE ../include/phish)
  target_include_directories(phish-bait-zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_include_directories(phish-bait-zmq PRIVATE ../include/phish)
  target_link_libraries(phish-zmq ${ZMQ_LIBRARY})
  target_link_libraries(phish-bait-zmq ${ZMQ_LIBRARY})
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
  MESSAGE(STATUS "WARNING: No zmq library, skip phish library.")
endif()

if(Boost_LOCKFREE_QUEUE_HPP)
  add_library(boost_qlock STATIC boost_qlock.cc)
  target_include_directories(boost_qlock PRIVATE ${Boost_INCLUDE_DIRS})
endif()
