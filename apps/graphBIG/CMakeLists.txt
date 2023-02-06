project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)
include_directories(common openG)

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
  if (RaftLib_FOUND)
      add_raftbenchmark(tc_rl rl_tc.cpp)
      target_compile_definitions(tc_rl PRIVATE -DUSE_OMP -DNO_PFM)
      target_compile_options(tc_rl PRIVATE ${OpenMP_C_FLAGS})
      target_link_libraries(tc_rl ${OpenMP_C_FLAGS})

      add_raftbenchmark(dc_rl rl_dc.cpp)
      target_compile_definitions(dc_rl PRIVATE -DUSE_OMP -DNO_PFM)
      target_compile_options(dc_rl PRIVATE ${OpenMP_C_FLAGS})
      target_link_libraries(dc_rl ${OpenMP_C_FLAGS})

      add_raftbenchmark(bc_rl rl_bc.cpp)
      target_compile_definitions(bc_rl PRIVATE -DUSE_OMP -DNO_PFM)
      target_compile_options(bc_rl PRIVATE ${OpenMP_C_FLAGS})
      target_link_libraries(bc_rl ${OpenMP_C_FLAGS})

      if (QTHREAD_FOUND)
          add_raftbenchmark(tc_qt_std rl_tc.cpp)
          target_compile_definitions(tc_qt_std PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_QTHREAD -DSTDALLOC)
          target_compile_options(tc_qt_std PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(tc_qt_std ${OpenMP_C_FLAGS})

          add_raftbenchmark(tc_qt_dyn rl_tc.cpp)
          target_compile_definitions(tc_qt_dyn PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_QTHREAD)
          target_compile_options(tc_qt_dyn PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(tc_qt_dyn ${OpenMP_C_FLAGS})

          add_raftbenchmark(dc_qt_std rl_dc.cpp)
          target_compile_definitions(dc_qt_std PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_QTHREAD -DSTDALLOC)
          target_compile_options(dc_qt_std PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(dc_qt_std ${OpenMP_C_FLAGS})

          add_raftbenchmark(dc_qt_dyn rl_dc.cpp)
          target_compile_definitions(dc_qt_dyn PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_QTHREAD)
          target_compile_options(dc_qt_dyn PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(dc_qt_dyn ${OpenMP_C_FLAGS})

          add_raftbenchmark(bc_qt_std rl_bc.cpp)
          target_compile_definitions(bc_qt_std PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_QTHREAD -DSTDALLOC)
          target_compile_options(bc_qt_std PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(bc_qt_std ${OpenMP_C_FLAGS})

          add_raftbenchmark(bc_qt_dyn rl_bc.cpp)
          target_compile_definitions(bc_qt_dyn PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_QTHREAD)
          target_compile_options(bc_qt_dyn PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(bc_qt_dyn ${OpenMP_C_FLAGS})

          if (VL_FOUND)
              add_raftbenchmark(tc_qt_vl rl_tc.cpp)
              target_compile_definitions(tc_qt_vl PRIVATE -DUSE_OMP -DNO_PFM
                                         -DUSE_QTHREAD -DVL)
              target_compile_options(tc_qt_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(tc_qt_vl ${OpenMP_C_FLAGS})

              add_raftbenchmark(dc_qt_vl rl_bc.cpp)
              target_compile_definitions(dc_qt_vl PRIVATE -DUSE_OMP -DNO_PFM
                                         -DUSE_QTHREAD -DVL)
              target_compile_options(dc_qt_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(dc_qt_vl ${OpenMP_C_FLAGS})

              add_raftbenchmark(bc_qt_vl rl_bc.cpp)
              target_compile_definitions(bc_qt_vl PRIVATE -DUSE_OMP -DNO_PFM
                                         -DUSE_QTHREAD -DVL)
              target_compile_options(bc_qt_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(bc_qt_vl ${OpenMP_C_FLAGS})
          else()
              MESSAGE(STATUS
                  "WARNING: No libvl, skip {tc,dc,bc}_qt_vl.")
          endif()
      else()
          MESSAGE(STATUS
              "WARNING: No qthread, skip {tc,dc,bc}_qt_{std,dyn,vl}.")
      endif()

      if (UT_FOUND)
          add_raftbenchmark(tc_ut_std rl_tc.cpp)
          target_compile_definitions(tc_ut_std PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_UT -DSTDALLOC)
          target_compile_options(tc_ut_std PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(tc_ut_std ${OpenMP_C_FLAGS})

          add_raftbenchmark(tc_ut_dyn rl_tc.cpp)
          target_compile_definitions(tc_ut_dyn PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_UT)
          target_compile_options(tc_ut_dyn PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(tc_ut_dyn ${OpenMP_C_FLAGS})

          add_raftbenchmark(dc_ut_std rl_dc.cpp)
          target_compile_definitions(dc_ut_std PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_UT -DSTDALLOC)
          target_compile_options(dc_ut_std PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(dc_ut_std ${OpenMP_C_FLAGS})

          add_raftbenchmark(dc_ut_dyn rl_dc.cpp)
          target_compile_definitions(dc_ut_dyn PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_UT)
          target_compile_options(dc_ut_dyn PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(dc_ut_dyn ${OpenMP_C_FLAGS})

          add_raftbenchmark(bc_ut_std rl_bc.cpp)
          target_compile_definitions(bc_ut_std PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_UT -DSTDALLOC)
          target_compile_options(bc_ut_std PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(bc_ut_std ${OpenMP_C_FLAGS})

          add_raftbenchmark(bc_ut_dyn rl_bc.cpp)
          target_compile_definitions(bc_ut_dyn PRIVATE -DUSE_OMP -DNO_PFM
                                     -DUSE_UT)
          target_compile_options(bc_ut_dyn PRIVATE ${OpenMP_C_FLAGS})
          target_link_libraries(bc_ut_dyn ${OpenMP_C_FLAGS})

          if (VL_FOUND)
              add_raftbenchmark(tc_ut_vl rl_tc.cpp)
              target_compile_definitions(tc_ut_vl PRIVATE -DUSE_OMP -DNO_PFM
                                         -DUSE_UT -DVL)
              target_compile_options(tc_ut_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(tc_ut_vl ${OpenMP_C_FLAGS})

              add_raftbenchmark(dc_ut_vl rl_bc.cpp)
              target_compile_definitions(dc_ut_vl PRIVATE -DUSE_OMP -DNO_PFM
                                         -DUSE_UT -DVL)
              target_compile_options(dc_ut_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(dc_ut_vl ${OpenMP_C_FLAGS})

              add_raftbenchmark(bc_ut_vl rl_bc.cpp)
              target_compile_definitions(bc_ut_vl PRIVATE -DUSE_OMP -DNO_PFM
                                         -DUSE_UT -DVL)
              target_compile_options(bc_ut_vl PRIVATE ${OpenMP_C_FLAGS})
              target_link_libraries(bc_ut_vl ${OpenMP_C_FLAGS})
          else()
              MESSAGE(STATUS
                  "WARNING: No libvl, skip {tc,dc,bc}_ut_vl.")
          endif()
      else()
          MESSAGE(STATUS
              "WARNING: No libut, skip {tc,dc,bc}_ut_{std,dyn,vl}.")
      endif()
  else()
      MESSAGE(STATUS
          "WARNING: No RaftLib, skip {tc,dc,bc}_{qt,ut}_{std,dyn,vl}.")
  endif()
else()
  MESSAGE(STATUS "WARNING: No openmp library, skip graphBIG benchmarks.")
endif()
