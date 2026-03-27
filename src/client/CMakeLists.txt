add_executable(battleship_client
    apiclient.cpp
    apiclient.h
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
)

target_link_libraries(battleship_client
    PRIVATE
        battleship_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Widgets
)

target_compile_definitions(battleship_client
    PRIVATE
        QT_DEPRECATED_WARNINGS
)