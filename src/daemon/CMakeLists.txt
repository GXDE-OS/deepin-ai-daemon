cmake_minimum_required(VERSION 3.5)

project(deepin-ai-daemon)

set(CMAKE_AUTOMOC ON)
# qdbuscpp2xml dbusserver/embplatformdbus.h -o ../../assets/dbus/org.deepin.ai.daemon.EmbeddingPlatform.xml

#add_definitions(-DAIPC_AI_DAEMON)

pkg_search_module(NlGenl REQUIRED libnl-genl-3.0 IMPORTED_TARGET)
pkg_check_modules(Lucene REQUIRED IMPORTED_TARGET liblucene++ liblucene++-contrib)
pkg_check_modules(Taglib REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-i18n)
pkg_check_modules(Docparser REQUIRED IMPORTED_TARGET docparser)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui DBus Sql Network Concurrent WebSockets)
find_package(Dtk${DTK_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Boost REQUIRED COMPONENTS system)

pkg_check_modules(dtkocr REQUIRED dtk${DTK_VERSION_MAJOR}ocr)

FILE(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

FILE(GLOB_RECURSE ANALYZER_SRC
    ${CMAKE_SOURCE_DIR}/3rdparty/analyzer/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/analyzer/*.h
)

add_executable(${PROJECT_NAME} ${ANALYZER_SRC} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/dbusserver
        ${CMAKE_CURRENT_SOURCE_DIR}/vectordb
        ${dtkocr_INCLUDE_DIRS}
        ${faiss_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3rdparty
)

target_link_libraries(${PROJECT_NAME}
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::WebSockets
    Dtk${DTK_VERSION_MAJOR}::Core
    ${Boost_LIBRARIES}
    ${dtkocr_LIBRARIES}
    PkgConfig::NlGenl
    PkgConfig::Lucene
    PkgConfig::Taglib
    PkgConfig::ICU
    PkgConfig::Docparser
    pthread
    faiss
)

# bin
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# services
install(FILES
    ${CMAKE_SOURCE_DIR}/assets/autostart/org.deepin.ai.daemon.service
    ${CMAKE_SOURCE_DIR}/assets/autostart/org.deepin.ai.daemon.QueryLang.service
    ${CMAKE_SOURCE_DIR}/assets/autostart/org.deepin.ai.daemon.VectorIndex.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/services)

# systemd service
install(FILES ${CMAKE_SOURCE_DIR}/assets/autostart/deepin-ai-daemon.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user)

# xdg-autostart
install(FILES ${CMAKE_SOURCE_DIR}/assets/autostart/deepin-ai-daemon.desktop
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart)

# install Manual App icon
install(FILES ${CMAKE_SOURCE_DIR}/assets/icon/deepin-ai-daemon.svg
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)

include(${CMAKE_SOURCE_DIR}/install_dconfig.cmake)
INSTALL_DCONFIG("deepin-ai-daemon.blacklist.json")
INSTALL_DCONFIG("deepin-ai-daemon.enableembeddingfileslist.json")
INSTALL_DCONFIG("deepin-ai-daemon.autoindex.json")
INSTALL_DCONFIG("deepin-ai-daemon.semanticanalysis.json")
