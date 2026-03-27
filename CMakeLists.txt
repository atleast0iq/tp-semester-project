cmake_minimum_required(VERSION 3.21)

project(
    Battleship
    VERSION 0.0.2
    DESCRIPTION "Qt client-server battleship project"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${config}" config_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/${config}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/${config}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/${config}")
    endforeach()
endif()

option(BATTLESHIP_BUILD_CLIENT "Build the Qt TCP test client" ON)
option(BATTLESHIP_BUILD_SERVER "Build the TCP battleship server" ON)

if(BATTLESHIP_BUILD_CLIENT OR BATTLESHIP_BUILD_SERVER)
    set(BATTLESHIP_QT_COMPONENTS Core Network Widgets)

    if(BATTLESHIP_BUILD_SERVER)
        list(APPEND BATTLESHIP_QT_COMPONENTS Sql)
    endif()

    list(REMOVE_DUPLICATES BATTLESHIP_QT_COMPONENTS)

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${BATTLESHIP_QT_COMPONENTS})
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${BATTLESHIP_QT_COMPONENTS})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/common)

if(BATTLESHIP_BUILD_CLIENT)
    add_subdirectory(src/client)
endif()

if(BATTLESHIP_BUILD_SERVER)
    add_subdirectory(src/server)
endif()
