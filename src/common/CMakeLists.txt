add_library(battleship_common STATIC
    apiprotocol.cpp
    apiprotocol.h
    fleetutils.cpp
    fleetutils.h
)

target_link_libraries(battleship_common
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(battleship_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(battleship_common
    PRIVATE
        QT_DEPRECATED_WARNINGS
)
