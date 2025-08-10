cmake_minimum_required (VERSION 3.20.3)
project (cppcon CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_compile_options(-Wall -Wextra -Werror -std=c++26)
add_compile_options(-nostdinc++ -I /Users/huixie/repos/libc++/llvm-project/build_main/libcxx/test-suite-install/include/c++/v1)
add_compile_options(-nostdlib++ )
add_link_options(-L /Users/huixie/repos/libc++/llvm-project/build_main/libcxx/test-suite-install/lib -Wl,-rpath,/Users/huixie/repos/libc++/llvm-project/build_main/libcxx/test-suite-install/lib -lc++)


find_package(benchmark REQUIRED)

add_subdirectory(segmented_bench)