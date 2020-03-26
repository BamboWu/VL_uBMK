# Output variables:
#  ZMQ_LIBRARY           : Library path of libzmq.a/libzmq.so
#  ZMQ_INCLUDE_DIR       : Include directory for zmq.h
#  ZMQ_STATIC_FOUND      : True if libzmq.a found.
#  ZMQ_DYNAMIC_FOUND     : True if libzmq.so found.

SET(ZMQ_STATIC_FOUND FALSE)
SET(ZMQ_DYNAMIC_FOUND FALSE)

IF(ZMQ_ROOT)
  FIND_LIBRARY(ZMQ_STATIC_LIBRARY libzmq.a
    HINTS
    ${ZMQ_ROOT}
    ${ZMQ_ROOT}/src/.libs
    )
  FIND_LIBRARY(ZMQ_DYNAMIC_LIBRARY libzmq.so
    HINTS
    ${ZMQ_ROOT}
    ${ZMQ_ROOT}/src/.libs
    )
  FIND_FILE(ZMQ_H "zmq.h" ${ZMQ_ROOT}/include)
  IF (ZMQ_H AND ZMQ_STATIC_LIBRARY)
    SET(ZMQ_STATIC_FOUND TRUE)
    SET(ZMQ_INCLUDE_DIR ${ZMQ_ROOT}/include)
    SET(ZMQ_LIBRARY ${ZMQ_STATIC_LIBRARY})
    MESSAGE(STATUS "Found libzmq.a: lib=${ZMQ_LIBRARY}")
  ELSEIF(ZMQ_H AND ZMQ_DYNAMIC_LIBRARY)
    SET(ZMQ_DYNAMIC_FOUND TRUE)
    SET(ZMQ_INCLUDE_DIR ${ZMQ_ROOT}/include)
    SET(ZMQ_LIBRARY ${ZMQ_DYNAMIC_LIBRARY})
    MESSAGE(STATUS "Found libzmq.so: lib=${ZMQ_LIBRARY}")
  ENDIF()
ENDIF()
