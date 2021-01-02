# Output variables:
#  VL_LIBRARY           : Library path of libvl.a
#  VL_INCLUDE_DIR       : Include directory for VL headers
#  VL_FOUND             : True if vl/vl.h and libvl.a found.
#  CAF_LIBRARY          : Library path of libcaf.a
#  CAF_INCLUDE_DIR      : Include directory for CAF headers
#  CAF_FOUND            : True if caf.h and libcaf.a found.

SET(VL_FOUND FALSE)
SET(CAF_FOUND FALSE)

IF(VL_ROOT)
  FIND_LIBRARY(VL_LIBRARY libvl.a
    HINTS
    ${VL_ROOT}
    ${VL_ROOT}/libvl
    )
  FIND_FILE(VL_H "vl/vl.h"
    HINTS
    ${VL_ROOT}
    )
  FIND_LIBRARY(CAF_LIBRARY libcaf.a
    HINTS
    ${VL_ROOT}/ext/libcaf
    )
  FIND_FILE(CAF_H "caf.h"
    HINTS
    ${VL_ROOT}/ext/libcaf
    )
ELSE(!VL_ROOT)
  FIND_LIBRARY(VL_LIBRARY libvl.a)
  FIND_FILE(VL_H "vl/vl.h")
  FIND_LIBRARY(CAF_LIBRARY libcaf.a)
  FIND_FILE(CAF_H "caf.h")
ENDIF(VL_ROOT)

IF (VL_H AND VL_LIBRARY)
  SET(VL_FOUND TRUE)
  STRING(REPLACE "vl/vl.h" "" VL_INCLUDE_DIR ${VL_H})
  MESSAGE(STATUS "Found vl: inc=${VL_INCLUDE_DIR}, lib=${VL_LIBRARY}")
ENDIF()
IF (CAF_H AND CAF_LIBRARY)
  SET(CAF_FOUND TRUE)
  STRING(REPLACE "caf.h" "" CAF_INCLUDE_DIR ${CAF_H})
  MESSAGE(STATUS "Found caf: inc=${CAF_INCLUDE_DIR}, lib=${CAF_LIBRARY}")
ENDIF()
