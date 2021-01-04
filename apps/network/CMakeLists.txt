project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()
if(CAF_FOUND)
  include_directories(${CAF_INCLUDE_DIR})
endif()

if(NOT VL_FOUND)
  MESSAGE(STATUS "WARNING: No libvl found, skip pipeline_vl.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_vl.")
else()
  add_microbenchmark(pipeline_vl pipeline.cpp)
  target_compile_definitions(pipeline_vl PRIVATE -DVL)
  target_link_libraries(pipeline_vl ${VL_LIBRARY})
endif()

if(NOT CAF_FOUND)
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_caf.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_caf.")
else()
  add_microbenchmark(pipeline_qmd pipeline.cpp)
  add_microbenchmark(pipeline_caf pipeline.cpp)
  target_compile_definitions(pipeline_qmd PRIVATE -DCAF=1)
  target_compile_definitions(pipeline_caf PRIVATE -DCAF=1 -DCAF_PREPUSH)
  target_link_libraries(pipeline_qmd ${CAF_LIBRARY})
  target_link_libraries(pipeline_caf ${CAF_LIBRARY})
endif()