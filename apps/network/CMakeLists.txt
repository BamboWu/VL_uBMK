project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()
if(CAF_FOUND)
  include_directories(${CAF_INCLUDE_DIR})
endif()

if(NOT VL_FOUND)
  MESSAGE(STATUS "WARNING: No libvl found, skip pipeline_vl.")
  MESSAGE(STATUS "WARNING: No libvl found, skip firewall_vl.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_vl.")
  MESSAGE(STATUS "WARNING: No atomic library, skip firewall_vl.")
else()
  add_microbenchmark(pipeline_vl pipeline.cpp)
  target_compile_definitions(pipeline_vl PRIVATE -DVL
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_vl ${VL_LIBRARY})
  add_microbenchmark(firewall_vl firewall.cpp)
  target_compile_definitions(firewall_vl PRIVATE -DVL
                             -DNUM_STAGE2=4
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=56)
  target_link_libraries(firewall_vl ${VL_LIBRARY})
  add_microbenchmark(pipeline_il pipeline.cpp)
  target_compile_definitions(pipeline_il PRIVATE -DVL -DVLINLINE -DNOSYSVL
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_il ${VL_LIBRARY})
  add_microbenchmark(firewall_il firewall.cpp)
  target_compile_definitions(firewall_il PRIVATE -DVL -DVLINLINE -DNOSYSVL
                             -DNUM_STAGE2=4
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=7 -DPOOL_SIZE=56)
  target_link_libraries(firewall_il ${VL_LIBRARY})
endif()

if(NOT CAF_FOUND)
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip pipeline_caf.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip firewall_qmd.")
  MESSAGE(STATUS "WARNING: No libcaf found, skip firewall_caf.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_qmd.")
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_caf.")
  MESSAGE(STATUS "WARNING: No atomic library, skip firewall_qmd.")
  MESSAGE(STATUS "WARNING: No atomic library, skip firewall_caf.")
else()
  add_microbenchmark(pipeline_qmd pipeline.cpp)
  target_compile_definitions(pipeline_qmd PRIVATE -DCAF=1
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=64)
  target_link_libraries(pipeline_qmd ${CAF_LIBRARY})

  add_microbenchmark(pipeline_caf pipeline.cpp)
  target_compile_definitions(pipeline_caf PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=64)
  target_link_libraries(pipeline_caf ${CAF_LIBRARY})

  add_microbenchmark(firewall_qmd firewall.cpp)
  target_compile_definitions(firewall_qmd PRIVATE -DCAF=1
                             -DNUM_STAGE2=4
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=56)
  target_link_libraries(firewall_qmd ${CAF_LIBRARY})

  add_microbenchmark(firewall_caf firewall.cpp)
  target_compile_definitions(firewall_caf PRIVATE -DCAF=1 -DCAF_PREPUSH
                             -DNUM_STAGE2=4
                             -DCORRECT_READ -DCORRECT_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=56)
  target_link_libraries(firewall_caf ${CAF_LIBRARY})
endif()

if(NOT Boost_FOUND OR NOT Boost_LOCKFREE_QUEUE_HPP)
  MESSAGE(STATUS "WARNING: No boost/lockfree/queue.hpp, skip pipeline_boost.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_boost.")
else()
  add_microbenchmark(pipeline_boost pipeline.cpp)
  target_compile_definitions(pipeline_boost PRIVATE -DBLFQ=1
                             -DNUM_STAGE1=4 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_boost ${Boost_LIBRARIES})
endif()

if(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
  MESSAGE(STATUS "WARNING: No zmq library, skip bitonic_zmq.")
elseif(NOT GCCLIBATOMIC_FOUND)
  MESSAGE(STATUS "WARNING: No atomic library, skip pipeline_zmq.")
else()
  add_microbenchmark(pipeline_zmq pipeline.cpp)
  target_include_directories(pipeline_zmq PRIVATE ${ZMQ_INCLUDE_DIR})
  target_compile_definitions(pipeline_zmq PRIVATE -DZMQ=1
                             -DNUM_STAGE1=1 -DNUM_STAGE2=4
                             -DSTAGE1_READ -DSTAGE1_WRITE
                             -DSTAGE2_READ -DSTAGE2_WRITE
                             -DBULK_SIZE=8 -DPOOL_SIZE=420)
  target_link_libraries(pipeline_zmq ${ZMQ_LIBRARY})
  if (ZMQ_STATIC_FOUND)
    target_compile_options(pipeline_zmq PRIVATE -static -pthread)
  endif()
endif()
