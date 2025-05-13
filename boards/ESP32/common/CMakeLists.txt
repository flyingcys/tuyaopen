set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

set(BOARD_SRC "")
set(BOARD_INC "")

message(STATUS "===========CONFIG_ENABLE_DISPLAY: ${CONFIG_ENABLE_DISPLAY}")
message(STATUS "=========== CONFIG_ENABLE_AUDIO: ${CONFIG_ENABLE_AUDIO}")


if (CONFIG_ENABLE_DISPLAY)
    set(FONT_SRC 
        "${COMMON_PATH}/display/font/font_awesome_14_1.c"
        "${COMMON_PATH}/display/font/font_awesome_30_1.c"
        "${COMMON_PATH}/display/font/font_puhui_14_1.c"
    )

    list(APPEND BOARD_SRC
        "${FONT_SRC}"
        "${COMMON_PATH}/display/oled_display.c")

    list(APPEND BOARD_INC
        "${COMMON_PATH}/display"
        "${COMMON_PATH}/display/font"
        )
endif()

if (CONFIG_ENABLE_AUDIO)
    list(APPEND BOARD_SRC
        "${COMMON_PATH}/audio/tdd_audio_no_codec.c"
        "${COMMON_PATH}/audio/tdd_audio_8311_codec.c"
        "${COMMON_PATH}/audio/tdd_audio_atk_no_codec.c"
        "${COMMON_PATH}/audio/tdd_audio_codec_bus.c"
        "${COMMON_PATH}/audio/tdd_audio_es8388_codec.c"
        "${COMMON_PATH}/audio/tdd_xl9555_io.c"
        )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/audio")
endif()

