##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

include(${CMAKE_CURRENT_LIST_DIR}/coreMQTT/mqttFilePaths.cmake)

# LIB_SRCS
# aux_source_directory(${MODULE_PATH}/coreMQTT/source LIB_SRCS)
set(LIB_SRCS ${MQTT_SOURCES} ${MQTT_SERIALIZER_SOURCES})

# list(APPEND LIB_SRCS ${MODULE_PATH}/src/mqtt_client_wrapper.c)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include ${MODULE_PATH}/coreMQTT/source/include ${MODULE_PATH}/coreMQTT/source/include/interface)

set(LIB_PUBLIC_DEFINES "-DHTTP_DO_NOT_USE_CUSTOM_CONFIG")

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    PUBLIC
        ${LIB_PUBLIC_INC}
    )


target_compile_definitions(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVAT_DEFINES}
    PUBLIC
        ${LIB_PUBLIC_DEFINES}
)

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
