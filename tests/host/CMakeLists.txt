cmake_minimum_required(VERSION 3.16)
project(tuyaopen_host_tests C)

enable_testing()

option(TUYA_ENABLE_COVERAGE "Enable coverage" OFF)
option(TUYA_ENABLE_ASAN "Enable ASAN" OFF)
option(TUYA_ENABLE_UBSAN "Enable UBSAN" OFF)

if(TUYA_ENABLE_COVERAGE)
    add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

if(TUYA_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(TUYA_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

add_subdirectory(../vendor/unity tests_vendor_unity)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/AddTuyaHostTest.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/GenerateMocks.cmake)

add_custom_target(codegen)

if(EXISTS "${CMAKE_SOURCE_DIR}/../../src/common/test/host/CMakeLists.txt")
    add_subdirectory(${CMAKE_SOURCE_DIR}/../../src/common/test/host common_host)
endif()
