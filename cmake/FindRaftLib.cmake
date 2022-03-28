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
    STRING(REPLACE "/libraft.a" "" RaftLib_LIBRARY_DIRS ${RaftLib_LIBRARY})
    IF(VL_FOUND)
      SET(RaftLib_LDFLAGS ${RaftLib_LIBRARY}
          -L${RaftLib_LIBRARY_DIRS}
          -lshm -lrt -laffinity -lpthread -ldemangle -lcmdargs
          ${VL_LIBRARY})
    ELSE()
      SET(RaftLib_LDFLAGS ${RaftLib_LIBRARY}
          -L${RaftLib_LIBRARY_DIRS}
          -lshm -lrt -laffinity -lpthread -ldemangle -lcmdargs)
    ENDIF()
    SET(RaftLib_CFLAGS -std=c++14 -DSTRING_NAMES=1 -DL1D_CACHE_LINE_SIZE=64)
  ENDIF(RAFT_ROOT)
ENDIF(NOT RaftLib_FOUND)

IF (RaftLib_H AND RaftLib_LIBRARY)
  SET(RaftLib_FOUND TRUE)
  STRING(REGEX REPLACE "raft$" "" RaftLib_INCLUDE_DIRS ${RaftLib_H})
  MESSAGE(STATUS "Found raft: inc=${RaftLib_INCLUDE_DIRS}, lib=${RaftLib_LDFLAGS}")
ELSEIF(RaftLib_FOUND)
  MESSAGE(STATUS "Found raft: inc=${RaftLib_INCLUDE_DIRS}, lib=${RaftLib_LDFLAGS}")
ENDIF()
