cmake_minimum_required(VERSION 3.10)

project(MyProject)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

# enable cJSON
set(CONFIG_ENABLE_CJSON "Enable cJSON" ON)
set(CONFIG_ENABLE_HTTP_CLIENT "Enable http_client" ON)
set(CONFIG_ENABLE_MQTT_CLIENT "Enable mqtt_client" ON)
set(CONFIG_ENABLE_TAL_SYSTEM "Enable tal_system" ON)
set(CONFIG_ENABLE_TAL_KV "Enable KV" ON)
set(CONFIG_ENABLE_TAL_SECURITY "Enable tal_security" ON)
set(CONFIG_ENABLE_MBEDTLS "Enable mbedtls" ON)
set(CONFIG_ENABLE_TAL_COMMUNICATION "Enable tal_communication" ON)
set(CONFIG_ENABLE_UTILITIES "Enable utilities" ON)

add_executable(${PROJECT_NAME} 
    main.c
)

target_include_directories(${PROJECT_NAME} PUBLIC .)

set(COMPONENT_LIBS "")

add_subdirectory(components)
add_subdirectory(boards)
add_subdirectory(port)

# 链接所有启用的组件到主程序
foreach(COMPONENT IN LISTS COMPONENT_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COMPONENT})
endforeach()
