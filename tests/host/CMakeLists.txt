cmake_minimum_required(VERSION 3.16)
project(tuyaopen_host_tests C)

enable_testing()

add_subdirectory(../vendor/unity tests_vendor_unity)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/AddTuyaHostTest.cmake)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../src/common/test/host common_host)
