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

  add_set_of_graphbig_raftbenchmarks(tc)
  add_set_of_graphbig_raftbenchmarks(dc)
  add_set_of_graphbig_raftbenchmarks(bc)

  #if (RaftLib_FOUND)
  #    add_raftbenchmark(tc_rl rl_tc.cpp)
  #    target_compile_definitions(tc_rl PRIVATE -DUSE_OMP -DNO_PFM -DRAFTLIB_ORIG=1)
  #    target_compile_options(tc_rl PRIVATE ${OpenMP_C_FLAGS})
  #    target_link_libraries(tc_rl ${OpenMP_C_FLAGS})

  #    add_raftbenchmark(dc_rl rl_dc.cpp)
  #    target_compile_definitions(dc_rl PRIVATE -DUSE_OMP -DNO_PFM -DRAFTLIB_ORIG=1)
  #    target_compile_options(dc_rl PRIVATE ${OpenMP_C_FLAGS})
  #    target_link_libraries(dc_rl ${OpenMP_C_FLAGS})

  #    add_raftbenchmark(bc_rl rl_bc.cpp)
  #    target_compile_definitions(bc_rl PRIVATE -DUSE_OMP -DNO_PFM -DRAFTLIB_ORIG=1)
  #    target_compile_options(bc_rl PRIVATE ${OpenMP_C_FLAGS})
  #    target_link_libraries(bc_rl ${OpenMP_C_FLAGS})

  #    if (QTHREAD_FOUND)
  #        add_raftbenchmark(tc_qt_std_orig rl_tc.cpp)
  #        target_compile_definitions(tc_qt_std_orig PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_QTHREAD -DSTDALLOC -DRAFTLIB_ORIG=1)
  #        target_compile_options(tc_qt_std_orig PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(tc_qt_std_orig ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(tc_qt_dyn_orig rl_tc.cpp)
  #        target_compile_definitions(tc_qt_dyn_orig PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_QTHREAD -DRAFTLIB_ORIG=1 -DRAFTLIB_ORIG=1)
  #        target_compile_options(tc_qt_dyn PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(tc_qt_dyn ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(dc_qt_std rl_dc.cpp)
  #        target_compile_definitions(dc_qt_std PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_QTHREAD -DSTDALLOC)
  #        target_compile_options(dc_qt_std PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(dc_qt_std ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(dc_qt_dyn rl_dc.cpp)
  #        target_compile_definitions(dc_qt_dyn PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_QTHREAD)
  #        target_compile_options(dc_qt_dyn PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(dc_qt_dyn ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(bc_qt_std rl_bc.cpp)
  #        target_compile_definitions(bc_qt_std PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_QTHREAD -DSTDALLOC)
  #        target_compile_options(bc_qt_std PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(bc_qt_std ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(bc_qt_dyn rl_bc.cpp)
  #        target_compile_definitions(bc_qt_dyn PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_QTHREAD)
  #        target_compile_options(bc_qt_dyn PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(bc_qt_dyn ${OpenMP_C_FLAGS})

  #        if (VL_FOUND)
  #            add_raftbenchmark(tc_qt_vl rl_tc.cpp)
  #            target_compile_definitions(tc_qt_vl PRIVATE -DUSE_OMP -DNO_PFM
  #                                       -DUSE_QTHREAD -DVL)
  #            target_compile_options(tc_qt_vl PRIVATE ${OpenMP_C_FLAGS})
  #            target_link_libraries(tc_qt_vl ${OpenMP_C_FLAGS})

  #            add_raftbenchmark(dc_qt_vl rl_bc.cpp)
  #            target_compile_definitions(dc_qt_vl PRIVATE -DUSE_OMP -DNO_PFM
  #                                       -DUSE_QTHREAD -DVL)
  #            target_compile_options(dc_qt_vl PRIVATE ${OpenMP_C_FLAGS})
  #            target_link_libraries(dc_qt_vl ${OpenMP_C_FLAGS})

  #            add_raftbenchmark(bc_qt_vl rl_bc.cpp)
  #            target_compile_definitions(bc_qt_vl PRIVATE -DUSE_OMP -DNO_PFM
  #                                       -DUSE_QTHREAD -DVL)
  #            target_compile_options(bc_qt_vl PRIVATE ${OpenMP_C_FLAGS})
  #            target_link_libraries(bc_qt_vl ${OpenMP_C_FLAGS})
  #        else()
  #            MESSAGE(STATUS
  #                "WARNING: No libvl, skip {tc,dc,bc}_qt_vl.")
  #        endif()
  #    else()
  #        MESSAGE(STATUS
  #            "WARNING: No qthread, skip {tc,dc,bc}_qt_{std,dyn,vl}.")
  #    endif()

  #    if (UT_FOUND)
  #        add_raftbenchmark(tc_ut_std rl_tc.cpp)
  #        target_compile_definitions(tc_ut_std PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_UT -DSTDALLOC)
  #        target_compile_options(tc_ut_std PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(tc_ut_std ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(tc_ut_dyn rl_tc.cpp)
  #        target_compile_definitions(tc_ut_dyn PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_UT)
  #        target_compile_options(tc_ut_dyn PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(tc_ut_dyn ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(dc_ut_std rl_dc.cpp)
  #        target_compile_definitions(dc_ut_std PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_UT -DSTDALLOC)
  #        target_compile_options(dc_ut_std PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(dc_ut_std ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(dc_ut_dyn rl_dc.cpp)
  #        target_compile_definitions(dc_ut_dyn PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_UT)
  #        target_compile_options(dc_ut_dyn PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(dc_ut_dyn ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(bc_ut_std rl_bc.cpp)
  #        target_compile_definitions(bc_ut_std PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_UT -DSTDALLOC)
  #        target_compile_options(bc_ut_std PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(bc_ut_std ${OpenMP_C_FLAGS})

  #        add_raftbenchmark(bc_ut_dyn rl_bc.cpp)
  #        target_compile_definitions(bc_ut_dyn PRIVATE -DUSE_OMP -DNO_PFM
  #                                   -DUSE_UT)
  #        target_compile_options(bc_ut_dyn PRIVATE ${OpenMP_C_FLAGS})
  #        target_link_libraries(bc_ut_dyn ${OpenMP_C_FLAGS})

  #        if (VL_FOUND)
  #            add_raftbenchmark(tc_ut_vl rl_tc.cpp)
  #            target_compile_definitions(tc_ut_vl PRIVATE -DUSE_OMP -DNO_PFM
  #                                       -DUSE_UT -DVL)
  #            target_compile_options(tc_ut_vl PRIVATE ${OpenMP_C_FLAGS})
  #            target_link_libraries(tc_ut_vl ${OpenMP_C_FLAGS})

  #            add_raftbenchmark(dc_ut_vl rl_bc.cpp)
  #            target_compile_definitions(dc_ut_vl PRIVATE -DUSE_OMP -DNO_PFM
  #                                       -DUSE_UT -DVL)
  #            target_compile_options(dc_ut_vl PRIVATE ${OpenMP_C_FLAGS})
  #            target_link_libraries(dc_ut_vl ${OpenMP_C_FLAGS})

  #            add_raftbenchmark(bc_ut_vl rl_bc.cpp)
  #            target_compile_definitions(bc_ut_vl PRIVATE -DUSE_OMP -DNO_PFM
  #                                       -DUSE_UT -DVL)
  #            target_compile_options(bc_ut_vl PRIVATE ${OpenMP_C_FLAGS})
  #            target_link_libraries(bc_ut_vl ${OpenMP_C_FLAGS})
  #        else()
  #            MESSAGE(STATUS
  #                "WARNING: No libvl, skip {tc,dc,bc}_ut_vl.")
  #        endif()
  #    else()
  #        MESSAGE(STATUS
  #            "WARNING: No libut, skip {tc,dc,bc}_ut_{std,dyn,vl}.")
  #    endif()
  #else()
  #    MESSAGE(STATUS
  #        "WARNING: No RaftLib, skip {tc,dc,bc}_{qt,ut}_{std,dyn,vl}.")
  #endif()
else()
  MESSAGE(STATUS "WARNING: No openmp library, skip graphBIG benchmarks.")
endif()
