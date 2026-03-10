function(tuya_add_host_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE unity cmock)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/../vendor/cmock/src
        ${CMAKE_SOURCE_DIR}/../../src/common/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_system/include
        ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/utilities/include
    )
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()
