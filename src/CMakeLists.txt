project(VL_uBMK)

set(CMAKE_REQUIRED_LIBRARIES "pthread")
check_function_exists(pthread_setaffinity_np HAS_SET_AFFINITY)
check_function_exists(pthread_setname_np HAS_SET_NAME)

add_library(uBMK_util STATIC
  threading.c
  )
target_link_libraries(uBMK_util pthread)
