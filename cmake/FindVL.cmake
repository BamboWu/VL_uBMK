# Output variables:
#  VL_LIBRARY           : Library path of libvl.a
#  VL_INCLUDE_DIR       : Include directory for VL headers
#  VL_FOUND             : True if vl/vl.h and libvl.a found.

SET(VL_FOUND FALSE)

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
ELSE(!VL_ROOT)
  FIND_LIBRARY(VL_LIBRARY libvl.a)
  FIND_FILE(VL_H "vl/vl.h")
ENDIF(VL_ROOT)

IF (VL_H AND VL_LIBRARY)
  SET(VL_FOUND TRUE)
  STRING(REPLACE "vl/vl.h" "" VL_INCLUDE_DIR ${VL_H})
  MESSAGE(STATUS "Found vl: inc=${VL_INCLUDE_DIR}, lib=${VL_LIBRARY}")
ENDIF()
