cmake_minimum_required(VERSION 3.20)

project(xdna_npu_miner LANGUAGES CXX)

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

add_library(xdna_contract
    src/xdna/buffers.cpp
    src/xdna/k1.cpp
)

target_include_directories(xdna_contract PUBLIC src)

target_compile_options(xdna_contract PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_executable(xdna_contract_tests tests/xdna_contract_tests.cpp)
target_link_libraries(xdna_contract_tests PRIVATE xdna_contract)
target_compile_options(xdna_contract_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_test(NAME xdna_contract_tests COMMAND xdna_contract_tests)

add_library(k1_test_support
    tests/k1_vectors.cpp
)
target_include_directories(k1_test_support PUBLIC
    src
    tests
)
target_link_libraries(k1_test_support PUBLIC
    bpp9000_reference
    xdna_contract
)
target_compile_options(k1_test_support PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_executable(k1_contract_tests tests/k1_contract_tests.cpp)
target_link_libraries(k1_contract_tests PRIVATE k1_test_support)
target_compile_options(k1_contract_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_test(NAME k1_contract_tests COMMAND k1_contract_tests)

set(XRT_ROOT "$ENV{XILINX_XRT}" CACHE PATH "XRT installation root")
if(NOT XRT_ROOT)
    set(XRT_ROOT "/opt/xilinx/xrt")
endif()

find_path(XRT_INCLUDE_DIR
    NAMES xrt/xrt_device.h
    HINTS "${XRT_ROOT}/include"
)
find_library(XRT_COREUTIL_LIBRARY
    NAMES xrt_coreutil
    HINTS "${XRT_ROOT}/lib64" "${XRT_ROOT}/lib"
)

if(XRT_INCLUDE_DIR AND XRT_COREUTIL_LIBRARY)
    message(STATUS "XDNA runtime enabled using XRT at ${XRT_ROOT}")

    add_library(xdna_runtime
        src/xdna/errors.cpp
        src/xdna/device.cpp
        src/xdna/runtime.cpp
        src/xdna/smoke.cpp
    )
    target_include_directories(xdna_runtime PUBLIC
        src
        "${XRT_INCLUDE_DIR}"
    )
    target_link_libraries(xdna_runtime PUBLIC
        xdna_contract
        "${XRT_COREUTIL_LIBRARY}"
        uuid
        pthread
    )
    target_compile_definitions(xdna_runtime PUBLIC XDNA_RUNTIME_HAS_XRT=1)
    target_compile_options(xdna_runtime PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
        -Wno-conversion
        -Wno-ignored-qualifiers
        -Wno-shadow
    )

    add_executable(xdna_probe src/xdna/probe_main.cpp)
    target_link_libraries(xdna_probe PRIVATE xdna_runtime)
    target_compile_options(xdna_probe PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )

    add_executable(xdna_smoke src/xdna/smoke_main.cpp)
    target_link_libraries(xdna_smoke PRIVATE xdna_runtime)
    target_compile_options(xdna_smoke PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
        -Wno-conversion
        -Wno-ignored-qualifiers
        -Wno-shadow
    )

    add_executable(xdna_k1_differential tests/k1_differential.cpp)
    target_link_libraries(xdna_k1_differential PRIVATE
        k1_test_support
        xdna_runtime
    )
    target_compile_options(xdna_k1_differential PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
        -Wno-conversion
        -Wno-ignored-qualifiers
        -Wno-shadow
    )
else()
    message(WARNING
        "XDNA runtime disabled: XRT headers or libxrt_coreutil were not found. "
        "The CPU reference and pure buffer-contract tests remain available."
    )
endif()
