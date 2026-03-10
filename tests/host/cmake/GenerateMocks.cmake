find_program(TUYA_RUBY_EXECUTABLE ruby)
set(TUYA_CMOCK_SCRIPT "${CMAKE_SOURCE_DIR}/../vendor/cmock/lib/cmock.rb")
set(TUYA_CMOCK_CONFIG "${CMAKE_SOURCE_DIR}/support/cmock/cmock_config.yml")

function(tuya_generate_mock header out_dir out_var)
    if(NOT TUYA_RUBY_EXECUTABLE)
        message(FATAL_ERROR "ruby is required to run CMock")
    endif()

    get_filename_component(header_name "${header}" NAME_WE)
    set(mock_c "${out_dir}/mock_${header_name}.c")
    set(mock_h "${out_dir}/mock_${header_name}.h")

    add_custom_command(
        OUTPUT "${mock_c}" "${mock_h}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
        COMMAND "${TUYA_RUBY_EXECUTABLE}" "${TUYA_CMOCK_SCRIPT}" "-o${TUYA_CMOCK_CONFIG}" "${header}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/mock_${header_name}.c" "${mock_c}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/mock_${header_name}.h" "${mock_h}"
        DEPENDS "${header}" "${TUYA_CMOCK_CONFIG}"
        VERBATIM
    )

    set(${out_var} "${mock_c}" PARENT_SCOPE)
endfunction()
