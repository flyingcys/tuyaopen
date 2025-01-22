##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
# if (CONFIG_ENABLE_LIBLWIP STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

include(${CMAKE_CURRENT_LIST_DIR}/srtp/srtpFilePaths.cmake)

# LIB_SRCS
file(GLOB LIB_SRCS
    "${MODULE_PATH}/libpeer/src/*.c"
)

list(APPEND LIB_SRCS ${SRTP2_SRCS})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/libpeer/src
)

set(LIB_PRIVATE_INC
    ${SRTP2_INCLUDE_DIRS}
)

set(LIB_OPTIONS "-w")
# "-DDISABLE_PEER_SIGNALING=0"
set(LIB_PUBLIC_OPTIONS "-DHAVE_CONFIG_H" "-DCONFIG_USE_USRSCTP=0" )

# if (CONFIG_PLATFORM_T5 STREQUAL "y")
    list(APPEND LIB_PUBLIC_OPTIONS "-DCONFIG_USE_LWIP" "-D__BYTE_ORDER=__LITTLE_ENDIAN" "-DHTTP_DO_NOT_USE_CUSTOM_CONFIG" "-DMQTT_DO_NOT_USE_CUSTOM_CONFIG")

    list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/srtp/port/lwip)
# endif()

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_link_libraries(${MODULE_NAME} PUBLIC libtls)

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

target_compile_options(${MODULE_NAME}
    PRIVATE
        ${LIB_OPTIONS}
    PUBLIC
        ${LIB_PUBLIC_OPTIONS}
    )
########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

# endif()