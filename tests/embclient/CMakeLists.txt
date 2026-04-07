cmake_minimum_required(VERSION 3.5)

project(embclient)

set(CMAKE_AUTOMOC ON)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui DBus Widgets)

set(Emb_XML ${DBUS_XML_DIR}/org.deepin.ai.daemon.EmbeddingPlatform.xml)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt6_add_dbus_interface(EMB_DBUS_SRCS ${Emb_XML} EmbeddingPlatformInterface)
else()
    qt5_add_dbus_interface(EMB_DBUS_SRCS ${Emb_XML} EmbeddingPlatformInterface)
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    testwidget.cpp
    testwidget.h
    testmail.cpp
    testmail.h
    ${EMB_DBUS_SRCS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)