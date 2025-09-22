##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC
file(GLOB APP_SRC 
    "${APP_PATH}/src/*.c"
    "${APP_PATH}/src/lingdong-gui/gui/*.c"
    "${APP_PATH}/src/lingdong-gui/misc/*.c"
)

# APP_INC
set(APP_INC 
    "${APP_PATH}/include"
    "${APP_PATH}/src/lingdong-gui/gui"
    "${APP_PATH}/src/lingdong-gui/misc"
    "${APP_PATH}/src/lingdong-gui/config"
)

# APP_OPTIONS
set(APP_OPTIONS "-W")
# list(APPEND APP_OPTIONS "-Wall" "-DLV_LVGL_H_INCLUDE_SIMPLE")
list(APPEND APP_OPTIONS "-Wall"
    "-D_RTE_"
    "-DVT_SCALING_RATIO=1"
    "-D__ARM_2D_USER_APP_CFG_H__=\"ldConfig.h\""
    "-D__va_list=va_list"
    "-DRTE_Acceleration_Arm_2D_Helper_Disp_Adapter0"
    "-DRTE_Acceleration_Arm_2D_Alpha_Blending"
)

add_compile_options(-MMD -g -ffunction-sections -fdata-sections -fno-ms-extensions -Wno-macro-redefined -O3 -w)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )
