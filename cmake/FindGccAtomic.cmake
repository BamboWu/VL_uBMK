# This scripts finds gcc's built-in atomic shared library (libatomic.so).

# Note that this is a shared library. Every atomic operation now causes some overhead to
# go through another module, but hopefully it's negligible.

# Output variables (as the methods are gcc-builtin, no includes):
#  GCCLIBATOMIC_LIBRARY   : Library path of libatomic.so
#  GCCLIBATOMIC_FOUND     : True if found.

FIND_LIBRARY(GCCLIBATOMIC_LIBRARY NAMES atomic atomic.so.1 libatomic.so.1
  HINTS
  $ENV{HOME}/local/lib64
  $ENV{HOME}/local/lib
  /usr/local/lib64
  /usr/local/lib
  /opt/local/lib64
  /opt/local/lib
  /usr/lib64
  /usr/lib
  /lib64
  /lib
  )

IF(GCCLIBATOMIC_LIBRARY)
  SET(GCCLIBATOMIC_FOUND TRUE)
  MESSAGE(STATUS "Found GCC's libatomic.so: lib=${GCCLIBATOMIC_LIBRARY}")
ELSE()
  SET(GCCLIBATOMIC_FOUND FALSE)
  MESSAGE(STATUS "GCC's libatomic.so not found.")
ENDIF()
