project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

add_microbenchmark(max1024x16_nosce max_sce.c)
target_compile_definitions(max1024x16_nosce PRIVATE -DMAX_ROUND=1024 -DNOSCE=1)
add_microbenchmark(max32768x16_nosce max_sce.c)
target_compile_definitions(max32768x16_nosce PRIVATE -DMAX_ROUND=32768 -DNOSCE=1)
add_microbenchmark(max65536x16_nosce max_sce.c)
target_compile_definitions(max65536x16_nosce PRIVATE -DMAX_ROUND=65536 -DNOSCE=1)

add_microbenchmark(max1024x16_civac max_sce.c)
target_compile_definitions(max1024x16_civac PRIVATE -DMAX_ROUND=1024 -DSCE_CIVAC=1)
add_microbenchmark(max32768x16_civac max_sce.c)
target_compile_definitions(max32768x16_civac PRIVATE -DMAX_ROUND=32768 -DSCE_CIVAC=1)
add_microbenchmark(max65536x16_civac max_sce.c)
target_compile_definitions(max65536x16_civac PRIVATE -DMAX_ROUND=65536 -DSCE_CIVAC=1)

add_microbenchmark(max1024x16_cvac max_sce.c)
target_compile_definitions(max1024x16_cvac PRIVATE -DMAX_ROUND=1024 -DSCE_CVAC=1)
add_microbenchmark(max32768x16_cvac max_sce.c)
target_compile_definitions(max32768x16_cvac PRIVATE -DMAX_ROUND=32768 -DSCE_CVAC=1)
add_microbenchmark(max65536x16_cvac max_sce.c)
target_compile_definitions(max65536x16_cvac PRIVATE -DMAX_ROUND=65536 -DSCE_CVAC=1)

add_microbenchmark(max1024x16_civac_unrelated max_sce.c)
target_compile_definitions(max1024x16_civac_unrelated PRIVATE -DMAX_ROUND=1024 -DSCE_CIVAC=1 -DUNRELATED_CACHELINE=1)
add_microbenchmark(max1024x16_cvac_unrelated max_sce.c)
target_compile_definitions(max1024x16_cvac_unrelated PRIVATE -DMAX_ROUND=1024 -DSCE_CVAC=1 -DUNRELATED_CACHELINE=1)
add_microbenchmark(max32768x16_civac_unrelated max_sce.c)
target_compile_definitions(max32768x16_civac_unrelated PRIVATE -DMAX_ROUND=32768 -DSCE_CIVAC=1 -DUNRELATED_CACHELINE=1)
add_microbenchmark(max32768x16_cvac_unrelated max_sce.c)
target_compile_definitions(max32768x16_cvac_unrelated PRIVATE -DMAX_ROUND=32768 -DSCE_CVAC=1 -DUNRELATED_CACHELINE=1)
add_microbenchmark(max65536x16_civac_unrelated max_sce.c)
target_compile_definitions(max65536x16_civac_unrelated PRIVATE -DMAX_ROUND=65536 -DSCE_CIVAC=1 -DUNRELATED_CACHELINE=1)
add_microbenchmark(max65536x16_cvac_unrelated max_sce.c)
target_compile_definitions(max65536x16_cvac_unrelated PRIVATE -DMAX_ROUND=65536 -DSCE_CVAC=1 -DUNRELATED_CACHELINE=1)


add_microbenchmark(max1024x32_nosce max_sce.c)
target_compile_definitions(max1024x32_nosce PRIVATE -DMAX_ROUND=64 -DMAX_LEN=32 -DNOSCE=1)
add_microbenchmark(max1024x64_nosce max_sce.c)
target_compile_definitions(max1024x64_nosce PRIVATE -DMAX_ROUND=64 -DMAX_LEN=64 -DNOSCE=1)
add_microbenchmark(max1024x32_civac max_sce.c)
target_compile_definitions(max1024x32_civac PRIVATE -DMAX_ROUND=64 -DMAX_LEN=32 -DSCE_CIVAC=1)
add_microbenchmark(max1024x64_civac max_sce.c)
target_compile_definitions(max1024x64_civac PRIVATE -DMAX_ROUND=64 -DMAX_LEN=64 -DSCE_CIVAC=1)
add_microbenchmark(max1024x32_cvac max_sce.c)
target_compile_definitions(max1024x32_cvac PRIVATE -DMAX_ROUND=64 -DMAX_LEN=32 -DSCE_CVAC=1)
add_microbenchmark(max1024x64_cvac max_sce.c)
target_compile_definitions(max1024x64_cvac PRIVATE -DMAX_ROUND=64 -DMAX_LEN=64 -DSCE_CVAC=1)

if(NOT TARGET max_sce)
    add_custom_target(max_sce)
    add_dependencies(max_sce
        max1024x16_nosce max32768x16_nosce max65536x16_nosce
        max1024x16_civac max32768x16_civac max65536x16_civac
        max1024x16_cvac max32768x16_cvac max65536x16_cvac
        max1024x16_civac_unrelated max1024x16_cvac_unrelated
        max32768x16_civac_unrelated max32768x16_cvac_unrelated
        max65536x16_civac_unrelated max65536x16_cvac_unrelated
        max1024x32_nosce max1024x32_civac max1024x32_cvac
        max1024x64_nosce max1024x64_civac max1024x64_cvac)
endif()
