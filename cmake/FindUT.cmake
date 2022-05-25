# Output variables:
#  UT_LIBRARIES         : Libraries to link to use ut
#  UT_LIBRARY_DIRS      : Libraries pathes to link to use ut
#  UT_INCLUDE_DIRS      : Include directories for ut.h
#  UT_FOUND             : True if libut is found

SET(UT_FOUND FALSE)

find_library( UT_BASE_LIBRARY
              NAMES base
              PATHS
              ${CMAKE_LIBRARY_PATH}
              $ENV{UT_LIB}
              $ENV{UT_PATH}/lib
              /usr/local/lib )

find_library( UT_RUNTIME_LIBRARY
              NAMES runtime
              PATHS
              ${CMAKE_LIBRARY_PATH}
              $ENV{UT_LIB}
              $ENV{UT_PATH}/lib
              /usr/local/lib )

find_library( UT_RT_LIBRARY
              NAMES rt++
              PATHS
              ${CMAKE_LIBRARY_PATH}
              $ENV{UT_LIB}
              $ENV{UT_PATH}/lib
              $ENV{UT_PATH}/binding/cc
              /usr/local/lib )

find_path( UT_INCLUDE
           NAMES ut.h
           PATHS
           ${CMAKE_INCLUDE_PATH}
           $ENV{UT_INC}/
           $ENV{UT_PATH}/include/ut
           /usr/local/include/ut )

if( UT_BASE_LIBRARY AND UT_RUNTIME_LIBRARY AND UT_RT_LIBRARY AND UT_INCLUDE )
    set( UT_FOUND TRUE )
    message( STATUS "Using libut threading library" ) 
    message( STATUS "LIBRARY: ${UT_RT_LIBRARY} ${UT_RUNTIME_LIBRARY} ${UT_BASE_LIBRARY}" ) 
    message( STATUS "INCLUDE: ${UT_INCLUDE}" ) 
else( UT_BASE_LIBRARY AND UT_RUNTIME_LIBRARY AND UT_RT_LIBRARY AND UT_INCLUDE )
    message( WARNING "Couldn't find libut library" )
endif( UT_BASE_LIBRARY AND UT_RUNTIME_LIBRARY AND UT_RT_LIBRARY AND UT_INCLUDE )
