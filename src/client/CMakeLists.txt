add_executable(battleship_client
    apiclient.cpp
    apiclient.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    gamesqare.h gamesqare.cpp
    boardwidget.h boardwidget.cpp
    gamewindow.h gamewindow.cpp gamewindow.ui
    boardwidget.cpp
    boardwidget.h


)

target_link_libraries(battleship_client
    PRIVATE
        battleship_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VESION_MAJOR}::Multimedia
)

target_compile_definitions(battleship_client
    PRIVATE
        QT_DEPRECATED_WARNINGS
)