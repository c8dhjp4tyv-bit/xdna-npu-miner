cmake_minimum_required(VERSION 3.20)

project(bpp9000_cpu_reference LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bpp9000_reference
    src/bpp9000/task.cpp
    src/bpp9000/random.cpp
    src/bpp9000/reference.cpp
)

target_include_directories(bpp9000_reference PUBLIC src)

target_compile_options(bpp9000_reference PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_executable(bpp9000_tests tests/test_main.cpp)
target_link_libraries(bpp9000_tests PRIVATE bpp9000_reference)
target_compile_options(bpp9000_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

enable_testing()
add_test(NAME bpp9000_reference_tests COMMAND bpp9000_tests)
