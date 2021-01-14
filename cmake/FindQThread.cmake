# Output variables:
#  QTHREAD_LIBRARIES         : Libraries to link to use qthread
#  QTHREAD_LIBRARY_DIRS      : Libraries pathes to link to use qthread
#  QTHREAD_INCLUDE_DIRS      : Include directories for qthread.h
#  QTHREAD_FOUND             : True if qthread is found

SET(QTHREAD_FOUND FALSE)

find_package(PkgConfig QUIET)

IF(PkgConfig_FOUND)
    PKG_CHECK_MODULES(QTHREAD QUIET raftlib)
ENDIF()

IF(NOT QTHREAD_FOUND)
  IF(QTHREAD_ROOT)
    FIND_LIBRARY(QTHREAD_LIBRARY libqthread.a
      HINTS
      ${QTHREAD_ROOT}
      ${QTHREAD_ROOT}/src/.libs
      ${QTHREAD_ROOT}/lib
      )
    FIND_FILE(QTHREAD_H "qthread/qthread.h"
      HINTS
      ${QTHREAD_ROOT}/include
      )
    SET(QTHREAD_LIBRARIES ${QTHREAD_LIBRARY} -lhwloc)
    SET(QTHREAD_LIBRARY_DIRS /usr/local/lib)
  ENDIF(QTHREAD_ROOT)
ENDIF(NOT QTHREAD_FOUND)

IF (QTHREAD_H AND QTHREAD_LIBRARY)
  SET(QTHREAD_FOUND TRUE)
  STRING(REPLACE "qthread/qthread.h" "" QTHREAD_INCLUDE_DIRS ${QTHREAD_H})
  MESSAGE(STATUS "Found qthread: inc=${QTHREAD_INCLUDE_DIRS}, lib=${QTHREAD_LIBRARIES}")
ELSEIF(QTHREAD_FOUND)
  MESSAGE(STATUS "Found qthread: inc=${QTHREAD_INCLUDE_DIRS}, lib=${QTHREAD_LIBRARIES}")
ENDIF()
