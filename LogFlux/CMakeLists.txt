cmake_minimum_required(VERSION 3.16)
project(LogFlux LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# These are needed to build resources, otherwise images are shown only in the Qt Designer
# but not at in the application at runtime.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Network
)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    LogFlux.ui
    LogFlux.h
    LogFlux.cpp
    FileSource.h
    FileSource.cpp
    ServerSource.h
    ServerSource.cpp
    DataSource.h
    Settings.h
    Settings.cpp
    Settings.ui
    resources.qrc
    TagWidgets.h
    LineNumberArea.h
    LineNumberArea.cpp
    LogTextEdit.h
    LogTextEdit.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(${PROJECT_NAME}
        PROPERTIES WIN32_EXECUTABLE TRUE
    )
endif()

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
)

