cmake_minimum_required(VERSION 3.16)

# Set target name to match directory name
set(TARGET_NAME modelconfigtool)

# Find Qt components
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# Enable Qt MOC, RCC, UIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/modelconfigdialog.cpp
    src/configmanager.cpp
    src/modelitem.cpp
    src/logcategory.cpp
)

# Header files
set(HEADERS
    src/mainwindow.h
    src/modelconfigdialog.h
    src/configmanager.h
    src/modelitem.h
    src/logcategory.h
)

# Create executable
add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(${TARGET_NAME}
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Set target properties
set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Install target
install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION bin
)
