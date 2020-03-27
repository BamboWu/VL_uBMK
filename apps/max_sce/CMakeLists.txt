project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(max16x16_sce max_sce.c)
target_compile_definitions(max16x16_sce PRIVATE -DMAX_ROUND=16)
add_microbenchmark(max64x16_sce max_sce.c)
target_compile_definitions(max64x16_sce PRIVATE -DMAX_ROUND=64)
add_microbenchmark(max256x16_sce max_sce.c)
target_compile_definitions(max256x16_sce PRIVATE -DMAX_ROUND=256)
add_microbenchmark(max1024x16_sce max_sce.c)
target_compile_definitions(max1024x16_sce PRIVATE -DMAX_ROUND=1024)
add_microbenchmark(max64x32_sce max_sce.c)
target_compile_definitions(max64x32_sce PRIVATE -DMAX_ROUND=64 -DMAX_LEN=32)
add_microbenchmark(max64x64_sce max_sce.c)
target_compile_definitions(max64x64_sce PRIVATE -DMAX_ROUND=64 -DMAX_LEN=64)
add_microbenchmark(max16x16_nosce max_sce.c)
target_compile_definitions(max16x16_nosce PRIVATE -DMAX_ROUND=16 -DNOSCE)
add_microbenchmark(max64x16_nosce max_sce.c)
target_compile_definitions(max64x16_nosce PRIVATE -DMAX_ROUND=64 -DNOSCE)
add_microbenchmark(max256x16_nosce max_sce.c)
target_compile_definitions(max256x16_nosce PRIVATE -DMAX_ROUND=256 -DNOSCE)
add_microbenchmark(max1024x16_nosce max_sce.c)
target_compile_definitions(max1024x16_nosce PRIVATE -DMAX_ROUND=1024 -DNOSCE)

if(NOT TARGET max_sce)
    add_custom_target(max_sce)
    add_dependencies(max_sce
        max16x16_sce max64x16_sce max256x16_sce max1024x16_sce
        max64x32_sce max64x64_sce
        max16x16_nosce max64x16_nosce max256x16_nosce max1024x16_nosce)
endif()
