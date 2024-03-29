# Output variables:
#  RaftLib_LIBRARIES         : Libraries to link to use RaftLib
#  RaftLib_LIBRARY_DIRS      : Libraries pathes to link to use RaftLib
#  RaftLib_INCLUDE_DIRS      : Include directories for raft
#  RaftLib_CFLAGS            : Compiler flags for raft
#  RaftLib_FOUND             : True if libraft.a found.

SET(RaftLib_FOUND FALSE)

find_package(PkgConfig QUIET)

IF(PkgConfig_FOUND)
  PKG_CHECK_MODULES(RaftLib QUIET raftlib)
  MESSAGE(STATUS ${RaftLib_CFLAGS})
ENDIF()

IF(RaftLib_FOUND)
  IF(QTHREAD_FOUND)
      # cmake/FindQthread.cmake set QTHREAD_FOUND only when libqthread.a
      # is found, and set QTHREAD_LIBRARIES, which statically links to
      # libqthread.a so should get the priority before RaftLib's LDFLAGS
      SET(RaftLib_LDFLAGS ${QTHREAD_LIBRARIES} ${RaftLib_LDFLAGS})
  ENDIF()
  MESSAGE(STATUS "Found raft: inc=${RaftLib_INCLUDE_DIRS}, lib=${RaftLib_LDFLAGS}")
ENDIF()
