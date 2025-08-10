
file(GLOB_RECURSE segmented_bench_src
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    *.cpp
)
add_executable(segmented-bench)
target_sources(segmented-bench PRIVATE ${segmented_bench_src})
target_include_directories(segmented-bench PRIVATE segmented )
target_link_libraries(segmented-bench PRIVATE benchmark::benchmark)
target_link_libraries(segmented-bench PRIVATE benchmark::benchmark_main)
target_compile_options(segmented-bench PRIVATE -O3)
