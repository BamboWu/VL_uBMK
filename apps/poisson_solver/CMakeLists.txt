project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

set(POISSON_CORE_SRC
    PoissonMain.cpp
    _2DDirichlet.cpp
    _2DGrid.cpp
    _2DPoissonEquation.cpp)

add_microbenchmark(poisson_gauss_seq ${POISSON_CORE_SRC} _2DSequentialGaussSeidel.cpp)
target_compile_definitions(poisson_gauss_seq PRIVATE -DSEQ_GAUSS -DNO_DEFAULT_MAPPING)
target_compile_options(poisson_gauss_seq PRIVATE -faligned-new)
add_microbenchmark(poisson_gaussrb_seq ${POISSON_CORE_SRC} _2DSequentialGaussSeidelRedBlack.cpp)
target_compile_definitions(poisson_gaussrb_seq PRIVATE -DSEQ_RBGAUSS -DNO_DEFAULT_MAPPING)
target_compile_options(poisson_gaussrb_seq PRIVATE -faligned-new)
add_microbenchmark(poisson_jacobi_seq ${POISSON_CORE_SRC} _2DSequentialJacobi.cpp)
target_compile_definitions(poisson_jacobi_seq PRIVATE -DSEQ_JAC -DNO_DEFAULT_MAPPING)
target_compile_options(poisson_jacobi_seq PRIVATE -faligned-new)
if (RaftLib_FOUND)
   add_raftbenchmark(poisson_jacobi_rl ${POISSON_CORE_SRC} _2DParallelJacobi.cpp)
   target_compile_definitions(poisson_jacobi_rl PRIVATE -DPAR_JAC -DNO_DEFAULT_MAPPING)
   target_compile_options(poisson_jacobi_rl PRIVATE -faligned-new)
   add_raftbenchmark(poisson_gaussrb_rl ${POISSON_CORE_SRC} _2DParallelGaussSeidelRedBlack.cpp)
   target_compile_definitions(poisson_gaussrb_rl PRIVATE -DPAR_GAUSS -DNO_DEFAULT_MAPPING)
   target_compile_options(poisson_gaussrb_rl PRIVATE -faligned-new)

   if (QTHREAD_FOUND)
       add_raftbenchmark(poisson_jacobi_qt_std ${POISSON_CORE_SRC} _2DParallelJacobi.cpp)
       target_compile_definitions(poisson_jacobi_qt_std PRIVATE -DPAR_JAC
                                  -DNO_DEFAULT_MAPPING -DUSE_QTHREAD -DSTDALLOC)
       target_compile_options(poisson_jacobi_qt_std PRIVATE -faligned-new)
       add_raftbenchmark(poisson_jacobi_qt_dyn ${POISSON_CORE_SRC} _2DParallelJacobi.cpp)
       target_compile_definitions(poisson_jacobi_qt_dyn PRIVATE -DPAR_JAC
                                  -DNO_DEFAULT_MAPPING -DUSE_QTHREAD)
       target_compile_options(poisson_jacobi_qt_dyn PRIVATE -faligned-new)

       add_raftbenchmark(poisson_gaussrb_qt_std ${POISSON_CORE_SRC} _2DParallelGaussSeidelRedBlack.cpp)
       target_compile_definitions(poisson_gaussrb_qt_std PRIVATE -DPAR_GAUSS
                                  -DNO_DEFAULT_MAPPING -DUSE_QTHREAD -DSTDALLOC)
       target_compile_options(poisson_gaussrb_qt_std PRIVATE -faligned-new)
       add_raftbenchmark(poisson_gaussrb_qt_dyn ${POISSON_CORE_SRC} _2DParallelGaussSeidelRedBlack.cpp)
       target_compile_definitions(poisson_gaussrb_qt_dyn PRIVATE -DPAR_GAUSS
                                  -DNO_DEFAULT_MAPPING -DUSE_QTHREAD)
       target_compile_options(poisson_gaussrb_qt_dyn PRIVATE -faligned-new)
   else()
       MESSAGE(STATUS "WARNING: No qthread, skip poisson_{jacobi,guassrb}_qt_{std,dyn}.")
   endif()

   if (UT_FOUND)
       add_raftbenchmark(poisson_jacobi_ut_std ${POISSON_CORE_SRC} _2DParallelJacobi.cpp)
       target_compile_definitions(poisson_jacobi_ut_std PRIVATE -DPAR_JAC
                                  -DNO_DEFAULT_MAPPING -DUSE_UT -DSTDALLOC)
       target_compile_options(poisson_jacobi_ut_std PRIVATE -faligned-new)
       add_raftbenchmark(poisson_jacobi_ut_dyn ${POISSON_CORE_SRC} _2DParallelJacobi.cpp)
       target_compile_definitions(poisson_jacobi_ut_dyn PRIVATE -DPAR_JAC
                                  -DNO_DEFAULT_MAPPING -DUSE_UT)
       target_compile_options(poisson_jacobi_ut_dyn PRIVATE -faligned-new)

       add_raftbenchmark(poisson_gaussrb_ut_std ${POISSON_CORE_SRC} _2DParallelGaussSeidelRedBlack.cpp)
       target_compile_definitions(poisson_gaussrb_ut_std PRIVATE -DPAR_GAUSS
                                  -DNO_DEFAULT_MAPPING -DUSE_UT -DSTDALLOC)
       target_compile_options(poisson_gaussrb_ut_std PRIVATE -faligned-new)
       add_raftbenchmark(poisson_gaussrb_ut_dyn ${POISSON_CORE_SRC} _2DParallelGaussSeidelRedBlack.cpp)
       target_compile_definitions(poisson_gaussrb_ut_dyn PRIVATE -DPAR_GAUSS
                                  -DNO_DEFAULT_MAPPING -DUSE_UT)
       target_compile_options(poisson_gaussrb_ut_dyn PRIVATE -faligned-new)
   else()
       MESSAGE(STATUS "WARNING: No libut, skip poisson_{jacobi,guassrb}_ut_{std,dyn}.")
   endif()
else()
   MESSAGE(STATUS "WARNING: No RaftLib, skip poisson_jacobi_rl, poisson_gaussrb_rl.")
endif()
