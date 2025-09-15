##
# @file CMakeLists.txt
# @brief 
#/

if (CONFIG_ENABLE_ARM2D STREQUAL "y")
# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
# GLOB_RECURSE
file(GLOB LIB_SRCS 
    "${MODULE_PATH}/platform/*.c"
    "${MODULE_PATH}/platform/math/*.c"
    "${MODULE_PATH}/Library/Source/*.c"
    "${MODULE_PATH}/Helper/Source/*.c"
    "${MODULE_PATH}/Helper/asset/*.c"
    "${MODULE_PATH}/examples/common/asset/*.c"
    "${MODULE_PATH}/examples/common/benchmark/*.c"
    "${MODULE_PATH}/examples/common/controls/*.c"
    "${MODULE_PATH}/examples/common/controls/qrcode_box/*.c"
    "${MODULE_PATH}/examples/common/opcodes/*.c"
    "${MODULE_PATH}/examples/common/scenes/fitness/*.c"
    "${MODULE_PATH}/examples/common/scenes/meter/*.c"
    "${MODULE_PATH}/examples/common/scenes/watch/*.c"
    "${MODULE_PATH}/examples/common/scenes/alarm_clock/*.c"
    "${MODULE_PATH}/examples/common/scenes/histogram/*.c"
    "${MODULE_PATH}/examples/common/scenes/fan/*.c"
    "${MODULE_PATH}/examples/common/scenes/bubble_charging/*.c"
    "${MODULE_PATH}/examples/common/scenes/animate_background/*.c"
    "${MODULE_PATH}/examples/common/scenes/knob/*.c"
    "${MODULE_PATH}/examples/common/loader/tjpgd_loader/*.c"
    "${MODULE_PATH}/examples/common/loader/tjpgd_loader/tjpgd/*.c"
    "${MODULE_PATH}/examples/common/loader/zjpgd_loader/*.c"
    "${MODULE_PATH}/examples/common/loader/zjpgd_loader/zjpgd/*.c"
    "${MODULE_PATH}/examples/demos/*.c"
    "${MODULE_PATH}/examples/demos/monochrome/*.c"
)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
        ${MODULE_PATH}/platform
        ${MODULE_PATH}/platform/math
        ${MODULE_PATH}/platform/dsp
        ${MODULE_PATH}/Library/Include
        ${MODULE_PATH}/Helper/Include
        ${MODULE_PATH}/Helper/built_in
        ${MODULE_PATH}/examples/common/benchmark
        ${MODULE_PATH}/examples/common/controls
        ${MODULE_PATH}/examples/common/opcodes
        ${MODULE_PATH}/examples/common/loader
        ${MODULE_PATH}/examples/common/loader/tjpgd_loader
        ${MODULE_PATH}/examples/common/loader/zjpgd_loader
        ${MODULE_PATH}/examples/common/scenes/fitness
        ${MODULE_PATH}/examples/common/scenes/meter
        ${MODULE_PATH}/examples/common/scenes/watch
        ${MODULE_PATH}/examples/common/scenes/alarm_clock
        ${MODULE_PATH}/examples/common/scenes/histogram
        ${MODULE_PATH}/examples/common/scenes/fan
        ${MODULE_PATH}/examples/common/scenes/bubble_charging
        ${MODULE_PATH}/examples/common/scenes/animate_background
        ${MODULE_PATH}/examples/common/scenes/knob
        ${MODULE_PATH}/examples/demos
        ${MODULE_PATH}/examples/demos/monochrome
)

add_compile_definitions(
    "-D_RTE_"
    "-DVT_SCALING_RATIO=1"
)

add_compile_options(-MMD -g -ffunction-sections -fdata-sections -fno-ms-extensions -Wno-macro-redefined -O3 -w)

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

target_compile_options(${MODULE_NAME}
    PRIVATE
        ${LIB_OPTIONS}
    )
########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()