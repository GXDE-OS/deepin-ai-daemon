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
 set(BUILD_OS_VERSION "25")

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


