cmake_minimum_required(VERSION 3.10)

project(deepin-ai-daemon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 内存泄漏检测
# set(CMAKE_CXX_FLAGS "-fsanitize=undefined,address,leak -fno-omit-frame-pointer")
# set(CMAKE_C_FLAGS "-fsanitize=undefined,address,leak -fno-omit-frame-pointer")
# set(CMAKE_Ｌ_FLAGS "-fsanitize=undefined,address,leak -fno-omit-frame-pointer")

# 执行 git submodule update --init 命令
#execute_process(
#    COMMAND git submodule update --init --recursive
#    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#    RESULT_VARIABLE SUBMODULE_UPDATE_RESULT
#)

## 检查子模块更新是否成功
#if(NOT SUBMODULE_UPDATE_RESULT EQUAL "0")
#    message(FATAL_ERROR "git submodule update --init failed: ${SUBMODULE_UPDATE_RESULT}")
#endif()

find_package(PkgConfig REQUIRED)

# 获取系统版本
if (NOT DEFINED BUILD_OS_VERSION OR BUILD_OS_VERSION STREQUAL "")
    # 读取文件并提取Version值
    file(STRINGS "/etc/deepin-version" FILE_CONTENTS REGEX "^Version=")

    # 遍历文件内容，查找Version行
    foreach(LINE ${FILE_CONTENTS})
        # 检查是否是Version行
        if(LINE MATCHES "Version=([0-9]+)")
            # 提取版本号
            set(BUILD_OS_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    message("Get BUILD_OS_VERSION from deepin-version ${BUILD_OS_VERSION}")
else()
    message("catch BUILD_OS_VERSION ${BUILD_OS_VERSION}")
endif()

if (${BUILD_OS_VERSION} MATCHES "25")
    find_package(QT NAMES Qt6)
    add_definitions(-DCOMPILE_ON_QT6)
    set(DTK_VERSION_MAJOR 6)
else()
    find_package(QT NAMES Qt5)
    set(DTK_VERSION_MAJOR "")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif()

# dbus xml dir
set(DBUS_XML_DIR ${CMAKE_SOURCE_DIR}/assets/dbus)

add_subdirectory(3rdparty)
add_subdirectory(src)
add_subdirectory(tests)


