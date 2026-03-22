add_executable(battleship_client
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(battleship_client
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Widgets
)

target_compile_definitions(battleship_client
    PRIVATE
        QT_DEPRECATED_WARNINGS
)
