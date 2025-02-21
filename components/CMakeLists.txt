# 获取顶层传递的选项
set(COMPONENTS_TO_BUILD "")

# 自动遍历 components 目录下的所有子目录
file(GLOB COMPONENT_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*/CMakeLists.txt")

message(STATUS "Components to build: ${COMPONENT_DIRS}")

# 提取组件名称（去掉路径和文件名）
foreach(COMPONENT_DIR IN LISTS COMPONENT_DIRS)

    get_filename_component(COMPONENT_NAME "${COMPONENT_DIR}" DIRECTORY)
    message(STATUS "Component name: ${COMPONENT_NAME}")
    string(TOUPPER ${COMPONENT_NAME} COMPONENT_NAME_UPPER)
    if(CONFIG_ENABLE_${COMPONENT_NAME_UPPER})
        list(APPEND COMPONENTS_TO_BUILD ${COMPONENT_NAME})
    endif()

endforeach()

# 确保组件名称是唯一的
list(REMOVE_DUPLICATES COMPONENTS_TO_BUILD)
message(STATUS "Components to build: ${COMPONENTS_TO_BUILD}")

# 添加并记录每个组件
foreach(COMPONENT IN LISTS COMPONENTS_TO_BUILD)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${COMPONENT}/CMakeLists.txt")
        # 添加组件的子目录
        add_subdirectory(${COMPONENT})

        # 记录构建的库
        list(APPEND COMPONENT_LIBS ${COMPONENT})
    endif()
endforeach()

# 将构建的库列表传递回顶层
set(COMPONENT_LIBS ${COMPONENT_LIBS} PARENT_SCOPE)