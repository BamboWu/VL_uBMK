project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)
include_directories(common openG)

macro(add_set_of_graphbig_raftbenchmarks BMK)
  if (RaftLib_FOUND)
      add_raftbenchmark(${BMK}_rl rl_${BMK}.cpp)
      target_compile_definitions(${BMK}_rl PRIVATE
                                 -DUSE_OMP -DNO_PFM -DRAFTLIB_ORIG=1)
      target_compile_options(${BMK}_rl PRIVATE ${OpenMP_C_FLAGS})
      target_link_libraries(${BMK}_rl ${OpenMP_C_FLAGS})

      if (QTHREAD_FOUND)

          add_raftbenchmark(${BMK}_qt_std_pw rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_qt_std_pw PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1)
          target_compile_options(${BMK}_qt_std_pw PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_qt_std_pw ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_qt_dyn_pw rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_qt_dyn_pw PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                     -DARMQ_DYNAMIC_ALLOC=1)
          target_compile_options(${BMK}_qt_dyn_pw PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_qt_dyn_pw ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_qt_std_os rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_qt_std_os PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                     -DRAFTLIB_ONESHOT=1)
          target_compile_options(${BMK}_qt_std_os PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_qt_std_os ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_qt_std_mix rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_qt_std_mix PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                     -DRAFTLIB_MIX=1)
          target_compile_options(${BMK}_qt_std_mix PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_qt_std_mix ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_qt_dyn_mix rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_qt_dyn_mix PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                     -DARMQ_DYNAMIC_ALLOC=1
                                     -DRAFTLIB_MIX=1)
          target_compile_options(${BMK}_qt_dyn_mix PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_qt_dyn_mix ${OpenMP_C_FLAGS})
          add_custom_target(${BMK}_qt)
          add_dependencies(${BMK}_qt
                           ${BMK}_qt_std_pw ${BMK}_qt_dyn_pw ${BMK}_qt_std_os
                           ${BMK}_qt_std_mix ${BMK}_qt_dyn_mix)
          add_dependencies(graph_qt ${BMK}_qt)

          if (NOT COMPILER_SUPPORTS_CXX20)
            MESSAGE(STATUS "WARNING: No C++20 support, skip ${BMK}_qt_*_orig")
          else (NOT COMPILER_SUPPORTS_CXX20)
            add_raftbenchmark(${BMK}_qt_std_orig rl_${BMK}.cpp)
            target_compile_definitions(${BMK}_qt_std_orig PRIVATE
                                       -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                       -DSTDALLOC -DRAFTLIB_ORIG=1)
            target_compile_options(${BMK}_qt_std_orig PRIVATE ${OpenMP_C_FLAGS})
            target_link_libraries(${BMK}_qt_std_orig ${OpenMP_C_FLAGS})

            add_raftbenchmark(${BMK}_qt_dyn_orig rl_${BMK}.cpp)
            target_compile_definitions(${BMK}_qt_dyn_orig PRIVATE
                                       -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                       -DRAFTLIB_ORIG=1)
            target_compile_options(${BMK}_qt_dyn_orig PRIVATE ${OpenMP_C_FLAGS})
            target_link_libraries(${BMK}_qt_dyn_orig ${OpenMP_C_FLAGS})
          endif (NOT COMPILER_SUPPORTS_CXX20)

          if (VL_FOUND)
              add_raftbenchmark(${BMK}_qt_vl rl_${BMK}.cpp)
              target_compile_definitions(${BMK}_qt_vl PRIVATE
                                         -DUSE_OMP -DNO_PFM -DUSE_QTHREAD=1
                                         -DVL -DRAFTLIB_ORIG=1)
              target_compile_options(${BMK}_qt_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(${BMK}_qt_vl ${OpenMP_C_FLAGS})
          else()
              MESSAGE(STATUS "WARNING: No libvl, skip ${BMK}_qt_vl.")
          endif()
      else()
          MESSAGE(STATUS "WARNING: No qthread, skip ${BMK}_qt.")
      endif()

      if (UT_FOUND)

          add_raftbenchmark(${BMK}_ut_std_pw rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_ut_std_pw PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_UT=1)
          target_compile_options(${BMK}_ut_std_pw PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_ut_std_pw ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_ut_dyn_pw rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_ut_dyn_pw PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_UT=1
                                     -DARMQ_DYNAMIC_ALLOC=1)
          target_compile_options(${BMK}_ut_dyn_pw PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_ut_dyn_pw ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_ut_std_os rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_ut_std_os PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_UT=1
                                     -DRAFTLIB_ONESHOT=1)
          target_compile_options(${BMK}_ut_std_os PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_ut_std_os ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_ut_std_mix rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_ut_std_mix PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_UT=1
                                     -DRAFTLIB_MIX=1)
          target_compile_options(${BMK}_ut_std_mix PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_ut_std_mix ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_ut_dyn_mix rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_ut_dyn_mix PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_UT=1
                                     -DARMQ_DYNAMIC_ALLOC=1
                                     -DRAFTLIB_MIX=1)
          target_compile_options(${BMK}_ut_dyn_mix PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_ut_dyn_mix ${OpenMP_C_FLAGS})

          add_raftbenchmark(${BMK}_ut_std_cv rl_${BMK}.cpp)
          target_compile_definitions(${BMK}_ut_std_cv PRIVATE
                                     -DUSE_OMP -DNO_PFM -DUSE_UT=1
                                     -DRAFTLIB_CV=1)
          target_compile_options(${BMK}_ut_std_cv PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(${BMK}_ut_std_cv ${OpenMP_C_FLAGS})
          add_custom_target(${BMK}_ut)
          add_dependencies(${BMK}_ut
                           ${BMK}_ut_std_pw ${BMK}_ut_dyn_pw ${BMK}_ut_std_cv
                           ${BMK}_ut_std_os ${BMK}_ut_std_mix ${BMK}_ut_dyn_mix)
          add_dependencies(graph_ut ${BMK}_ut)

          if (NOT COMPILER_SUPPORTS_CXX20)
            MESSAGE(STATUS "WARNING: No C++20 support, skip ${BMK}_qt_*_orig")
          else (NOT COMPILER_SUPPORTS_CXX20)
            add_raftbenchmark(${BMK}_ut_std_orig rl_${BMK}.cpp)
            target_compile_definitions(${BMK}_ut_std_orig PRIVATE
                                       -DUSE_OMP -DNO_PFM -DUSE_UT=1 -DSTDALLOC
                                       -DRAFTLIB_ORIG=1)
            target_compile_options(${BMK}_ut_std_orig PRIVATE ${OpenMP_C_FLAGS}
                                   -DRAFTLIB_ORIG=1)
            target_link_libraries(${BMK}_ut_std_orig ${OpenMP_C_FLAGS})

            add_raftbenchmark(${BMK}_ut_dyn_orig rl_${BMK}.cpp)
            target_compile_definitions(${BMK}_ut_dyn_orig PRIVATE
                                       -DUSE_OMP -DNO_PFM -DUSE_UT=1
                                       -DRAFTLIB_ORIG=1)
            target_compile_options(${BMK}_ut_dyn_orig PRIVATE ${OpenMP_C_FLAGS})
            target_link_libraries(${BMK}_ut_dyn_orig ${OpenMP_C_FLAGS})
          endif (NOT COMPILER_SUPPORTS_CXX20)

          if (VL_FOUND)
              add_raftbenchmark(${BMK}_ut_vl rl_${BMK}.cpp)
              target_compile_definitions(tc_ut_vl PRIVATE
                                         -DUSE_OMP -DNO_PFM -DUSE_UT=1 -DVL)
              target_compile_options(${BMK}_ut_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(${BMK}_ut_vl ${OpenMP_C_FLAGS})
          else()
              MESSAGE(STATUS "WARNING: No libvl, skip ${BMK}_ut_vl.")
          endif()
      else()
          MESSAGE(STATUS "WARNING: No libut, skip ${BMK}_ut.")
      endif()
  else()
      MESSAGE(STATUS
          "WARNING: No RaftLib, skip ${BMK}_{qt,ut}.")
  endif()
endmacro(add_set_of_graphbig_raftbenchmarks)

find_package(OpenMP QUIET)
if (OPENMP_FOUND)
  add_microbenchmark(tc_omp bench_tc.cpp)
  target_compile_definitions(tc_omp PRIVATE -DUSE_OMP -DNO_PFM)
  target_compile_options(tc_omp PRIVATE ${OpenMP_C_FLAGS})
  target_link_libraries(tc_omp ${OpenMP_C_FLAGS})

  add_microbenchmark(dc_omp bench_dc.cpp)
  target_compile_definitions(dc_omp PRIVATE -DUSE_OMP -DNO_PFM)
  target_compile_options(dc_omp PRIVATE ${OpenMP_C_FLAGS})
  target_link_libraries(dc_omp ${OpenMP_C_FLAGS})

  add_microbenchmark(bc_omp bench_bc.cpp)
  target_compile_definitions(bc_omp PRIVATE -DUSE_OMP -DNO_PFM)
  target_compile_options(bc_omp PRIVATE ${OpenMP_C_FLAGS})
  target_link_libraries(bc_omp ${OpenMP_C_FLAGS})

  add_custom_target(graph_qt)
  add_custom_target(graph_ut)

  add_set_of_graphbig_raftbenchmarks(tc)
  add_set_of_graphbig_raftbenchmarks(dc)
  add_set_of_graphbig_raftbenchmarks(bc)

else()
  MESSAGE(STATUS "WARNING: No openmp library, skip graphBIG benchmarks.")
endif()
