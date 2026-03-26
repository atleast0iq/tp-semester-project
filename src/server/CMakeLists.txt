file(GLOB BATTLESHIP_SERVER_MIGRATIONS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/migrations/*.sql"
)

add_executable(battleship_server
    databasemanager.cpp
    databasemanager.h
    gameboard.cpp
    gameboard.h
    gamemanager.cpp
    gamemanager.h
    main.cpp
    serverapplication.cpp
    serverapplication.h
    ${BATTLESHIP_SERVER_MIGRATIONS}
)

target_link_libraries(battleship_server
    PRIVATE
        battleship_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Sql
)

target_compile_definitions(battleship_server
    PRIVATE
        QT_DEPRECATED_WARNINGS
)

set_source_files_properties(${BATTLESHIP_SERVER_MIGRATIONS}
    PROPERTIES
        HEADER_FILE_ONLY ON
)

add_custom_command(
    TARGET battleship_server
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        "$<TARGET_FILE_DIR:battleship_server>/migrations"
    COMMENT
        "Copying SQL migrations next to battleship_server"
)
