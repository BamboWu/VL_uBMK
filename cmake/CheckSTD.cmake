
include(CheckCXXCompilerFlag)

if( UNIX )
##
# Check for CXX17 or greater
##
check_cxx_compiler_flag( "-std=gnu++17" COMPILER_SUPPORTS_CXX17 )

if( COMPILER_SUPPORTS_CXX17 )
 set( CMAKE_CXX_STANDARD 17 )
else( COMPILER_SUPPORTS_CXX17 )
 message( FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support. Please use a newer compiler" )
endif( COMPILER_SUPPORTS_CXX17 )

check_cxx_compiler_flag( "-std=gnu++20" COMPILER_SUPPORTS_CXX20 )

if( COMPILER_SUPPORTS_CXX20 )
 set( CMAKE_CXX_STANDARD 20 )
else( COMPILER_SUPPORTS_CXX20 )
 message( STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++20 support. Please use a newer compiler" )
endif( COMPILER_SUPPORTS_CXX20 )

endif( UNIX )
