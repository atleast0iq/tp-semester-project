add_executable(battleship_client
    apiclient.cpp
    apiclient.h
    commandlineclient.cpp
    commandlineclient.h
    main.cpp
)

target_link_libraries(battleship_client
    PRIVATE
        battleship_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(battleship_client
    PRIVATE
        QT_DEPRECATED_WARNINGS
)
