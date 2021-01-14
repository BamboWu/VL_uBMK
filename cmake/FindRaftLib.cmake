# Output variables:
#  RaftLib_LIBRARIES         : Libraries to link to use RaftLib
#  RaftLib_LIBRARY_DIRS      : Libraries pathes to link to use RaftLib
#  RaftLib_INCLUDE_DIRS      : Include directories for raft
#  RaftLib_FOUND             : True if libraft.a found.

SET(RaftLib_FOUND FALSE)

find_package(PkgConfig QUIET)

IF(PkgConfig_FOUND)
  PKG_CHECK_MODULES(RaftLib QUIET raftlib)
ENDIF()

IF(NOT RaftLib_FOUND)
  IF(RAFT_ROOT)
    FIND_LIBRARY(RaftLib_LIBRARY libraft.a
      HINTS
      ${RAFT_ROOT}
      ${RAFT_ROOT}/build/src
      ${RAFT_ROOT}/lib
      )
    FIND_FILE(RaftLib_H "raft"
      HINTS
      ${RAFT_ROOT}
      ${RAFT_ROOT}/include
      )
    SET(RaftLib_LIBRARIES ${RaftLib_LIBRARY}
        -lshm -lrt -laffinity -lpthread -ldemangle -lcmdargs)
    SET(RaftLib_LIBRARY_DIRS /usr/local/lib)
  ENDIF(RAFT_ROOT)
ENDIF(NOT RaftLib_FOUND)

IF (RaftLib_H AND RaftLib_LIBRARY)
  SET(RaftLib_FOUND TRUE)
  STRING(REPLACE "raft" "" RaftLib_INCLUDE_DIRS ${RaftLib_H})
  MESSAGE(STATUS "Found raft: inc=${RaftLib_INCLUDE_DIRS}, lib=${RaftLib_LIBRARIES}")
ELSEIF(RaftLib_FOUND)
  MESSAGE(STATUS "Found raft: inc=${RaftLib_INCLUDE_DIRS}, lib=${RaftLib_LIBRARIES}")
ENDIF()
