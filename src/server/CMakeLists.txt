add_executable(battleship_server
    main.cpp
    serverapplication.cpp
    serverapplication.h
)

target_link_libraries(battleship_server
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(battleship_server
    PRIVATE
        QT_DEPRECATED_WARNINGS
)
