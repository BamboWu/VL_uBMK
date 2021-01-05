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
  add_microbenchmark(pipeline_vl_1rw1rw pipeline.cpp)
  target_compile_definitions(pipeline_vl_1rw1rw PRIVATE -DVL
                             -DNUM_STAGE1=1 -DNUM_STAGE2=1
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_vl_1rw1rw ${VL_LIBRARY})
  add_microbenchmark(pipeline_vl_3rw3rw pipeline.cpp)
  target_compile_definitions(pipeline_vl_3rw3rw PRIVATE -DVL
                             -DNUM_STAGE1=3 -DNUM_STAGE2=3
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_vl_3rw3rw ${VL_LIBRARY})
  add_microbenchmark(pipeline_vl_7rw7rw pipeline.cpp)
  target_compile_definitions(pipeline_vl_7rw7rw PRIVATE -DVL
                             -DNUM_STAGE1=7 -DNUM_STAGE2=7
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_vl_7rw7rw ${VL_LIBRARY})
  add_custom_target(pipeline_vl)
  add_dependencies(pipeline_vl
                   pipeline_vl_1rw1rw pipeline_vl_3rw3rw pipeline_vl_7rw7rw)
endif()

if(NOT CAF_FOUND)
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_caf.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_caf.")
else()
  add_microbenchmark(pipeline_qmd_1rw1rw pipeline.cpp)
  target_compile_definitions(pipeline_qmd_1rw1rw PRIVATE -DCAF=1
                             -DNUM_STAGE1=1 -DNUM_STAGE2=1
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_qmd_1rw1rw ${CAF_LIBRARY})
  add_microbenchmark(pipeline_qmd_3rw3rw pipeline.cpp)
  target_compile_definitions(pipeline_qmd_3rw3rw PRIVATE -DCAF=1
                             -DNUM_STAGE1=3 -DNUM_STAGE2=3
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_qmd_3rw3rw ${CAF_LIBRARY})
  add_microbenchmark(pipeline_qmd_7rw7rw pipeline.cpp)
  target_compile_definitions(pipeline_qmd_7rw7rw PRIVATE -DCAF=1
                             -DNUM_STAGE1=7 -DNUM_STAGE2=7
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_qmd_7rw7rw ${CAF_LIBRARY})
  add_custom_target(pipeline_qmd)
  add_dependencies(pipeline_qmd
                   pipeline_qmd_1rw1rw pipeline_qmd_3rw3rw pipeline_qmd_7rw7rw)

  add_microbenchmark(pipeline_caf_1rw1rw pipeline.cpp)
  target_compile_definitions(pipeline_caf_1rw1rw PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DNUM_STAGE1=1 -DNUM_STAGE2=1
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_caf_1rw1rw ${CAF_LIBRARY})
  add_microbenchmark(pipeline_caf_3rw3rw pipeline.cpp)
  target_compile_definitions(pipeline_caf_3rw3rw PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DNUM_STAGE1=3 -DNUM_STAGE2=3
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_caf_3rw3rw ${CAF_LIBRARY})
  add_microbenchmark(pipeline_caf_7rw7rw pipeline.cpp)
  target_compile_definitions(pipeline_caf_7rw7rw PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DNUM_STAGE1=7 -DNUM_STAGE2=7
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=112)
  target_link_libraries(pipeline_caf_7rw7rw ${CAF_LIBRARY})
  add_custom_target(pipeline_caf)
  add_dependencies(pipeline_caf
                   pipeline_caf_1rw1rw pipeline_caf_3rw3rw pipeline_caf_7rw7rw)
endif()
