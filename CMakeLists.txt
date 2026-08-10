cmake_minimum_required(VERSION 3.20)

project(xdna_npu_miner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(XDNA_ENABLE_PRODUCTION_CRYPTO
    "Build the pinned K12/FourQ production crypto provider and its KATs"
    OFF
)

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
    src/xdna/m4.cpp
    src/xdna/m5.cpp
    src/xdna/verification.cpp
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

add_library(m4_test_support
    tests/m4_vectors.cpp
)
target_include_directories(m4_test_support PUBLIC
    src
    tests
)
target_link_libraries(m4_test_support PUBLIC
    bpp9000_reference
    xdna_contract
)
target_compile_options(m4_test_support PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_executable(m4_contract_tests tests/m4_contract_tests.cpp)
target_link_libraries(m4_contract_tests PRIVATE m4_test_support)
target_compile_options(m4_contract_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)
add_test(NAME m4_contract_tests COMMAND m4_contract_tests)

add_executable(m5_contract_tests tests/m5_contract_tests.cpp)
target_link_libraries(m5_contract_tests PRIVATE m4_test_support)
target_compile_options(m5_contract_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)
add_test(NAME m5_contract_tests COMMAND m5_contract_tests)

add_library(qubic_direct_node
    src/qubic/direct_node.cpp
)

target_include_directories(qubic_direct_node PUBLIC src)
target_link_libraries(qubic_direct_node PUBLIC bpp9000_reference)
target_compile_options(qubic_direct_node PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

add_executable(qubic_direct_node_tests tests/qubic_direct_node_tests.cpp)
target_link_libraries(qubic_direct_node_tests PRIVATE qubic_direct_node)
target_compile_options(qubic_direct_node_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)
add_test(NAME qubic_direct_node_tests COMMAND qubic_direct_node_tests)

add_executable(qubic_live_probe src/qubic/live_probe_main.cpp)
target_link_libraries(qubic_live_probe PRIVATE qubic_direct_node)
target_compile_options(qubic_live_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Werror
)

if(XDNA_ENABLE_PRODUCTION_CRYPTO)
    # These are intentionally opt-in so the normal CPU/NPU contract build has
    # no network or third-party crypto dependency. The exact revisions and
    # selected files are recorded in docs/UPSTREAM.md.
    enable_language(C)
    include(FetchContent)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    FetchContent_Declare(xdna_fourq_source
        GIT_REPOSITORY https://github.com/microsoft/FourQlib.git
        GIT_TAG 1031567f23278e1135b35cc04e5d74c2ac88c029
        GIT_SHALLOW FALSE
    )
    FetchContent_Declare(xdna_k12_source
        GIT_REPOSITORY https://github.com/XKCP/K12.git
        GIT_TAG f95b0b73e29fe75fe99fbbb24c8000d9fcf0b40e
        GIT_SHALLOW FALSE
    )
    FetchContent_GetProperties(xdna_fourq_source)
    if(NOT xdna_fourq_source_POPULATED)
        FetchContent_Populate(xdna_fourq_source)
    endif()
    FetchContent_GetProperties(xdna_k12_source)
    if(NOT xdna_k12_source_POPULATED)
        FetchContent_Populate(xdna_k12_source)
    endif()

    set(XDNA_FOURQ_DIR
        "${xdna_fourq_source_SOURCE_DIR}/FourQ_64bit_and_portable"
    )
    add_library(xdna_fourq STATIC
        "${XDNA_FOURQ_DIR}/eccp2.c"
        "${XDNA_FOURQ_DIR}/eccp2_no_endo.c"
        "${XDNA_FOURQ_DIR}/eccp2_core.c"
        "${XDNA_FOURQ_DIR}/crypto_util.c"
        "${XDNA_FOURQ_DIR}/schnorrq.c"
        "${XDNA_FOURQ_DIR}/kex.c"
        "${xdna_fourq_source_SOURCE_DIR}/random/random.c"
    )
    target_include_directories(xdna_fourq SYSTEM PUBLIC
        "${XDNA_FOURQ_DIR}"
        "${xdna_fourq_source_SOURCE_DIR}"
    )
    target_compile_definitions(xdna_fourq PUBLIC
        _AMD64_
        __LINUX__
        _GENERIC_
        USE_ENDO
    )
    target_compile_options(xdna_fourq PRIVATE -fwrapv)

    set(XDNA_K12_DIR "${xdna_k12_source_SOURCE_DIR}/lib")
    add_library(xdna_k12 STATIC
        "${XDNA_K12_DIR}/KangarooTwelve.c"
        "${XDNA_K12_DIR}/KangarooTwelve-threading.c"
        "${XDNA_K12_DIR}/Optimized64/KeccakP-1600-opt64.c"
    )
    target_include_directories(xdna_k12 SYSTEM PUBLIC
        "${XDNA_K12_DIR}/Plain64"
        "${XDNA_K12_DIR}"
    )
    target_compile_definitions(xdna_k12 PRIVATE KeccakP1600_disableParallelism)

    add_library(qubic_production_crypto
        src/qubic/production_crypto.cpp
        src/qubic/local_identity.cpp
    )
    target_include_directories(qubic_production_crypto PUBLIC src)
    target_link_libraries(qubic_production_crypto PUBLIC
        qubic_direct_node
        xdna_fourq
        xdna_k12
    )
    target_compile_options(qubic_production_crypto PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )

    add_executable(qubic_crypto_tests tests/qubic_crypto_tests.cpp)
    target_link_libraries(qubic_crypto_tests PRIVATE qubic_production_crypto)
    target_compile_options(qubic_crypto_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )
    add_test(NAME qubic_crypto_tests COMMAND qubic_crypto_tests)

    add_executable(m6_task_verify src/qubic/m6_task_verify.cpp)
    target_link_libraries(m6_task_verify PRIVATE qubic_production_crypto)
    target_compile_options(m6_task_verify PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )

    add_executable(m6_identity_tool src/qubic/m6_identity_tool.cpp)
    target_link_libraries(m6_identity_tool PRIVATE qubic_production_crypto)
    target_compile_options(m6_identity_tool PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )

    add_executable(m6_authorization_check src/qubic/m6_authorization_check.cpp)
    target_link_libraries(m6_authorization_check PRIVATE qubic_production_crypto)
    target_compile_options(m6_authorization_check PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )

    add_executable(m6_read_only_diagnostics src/qubic/m6_read_only_diagnostics.cpp)
    target_link_libraries(m6_read_only_diagnostics PRIVATE qubic_production_crypto)
    target_compile_options(m6_read_only_diagnostics PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )
endif()

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
        src/xdna/m4_score.cpp
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

    add_executable(xdna_m4_differential tests/m4_differential.cpp)
    target_link_libraries(xdna_m4_differential PRIVATE
        m4_test_support
        xdna_runtime
    )
    target_compile_options(xdna_m4_differential PRIVATE
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

    add_executable(xdna_m5_differential tests/m5_differential.cpp)
    target_link_libraries(xdna_m5_differential PRIVATE
        m4_test_support
        xdna_runtime
    )
    target_compile_options(xdna_m5_differential PRIVATE
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
