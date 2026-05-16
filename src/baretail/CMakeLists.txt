add_executable(baretail
    main.cpp
    app/application.h
    app/application.cpp
    app/mainwindow.h
    app/mainwindow.cpp
    app/taildocument.h
    app/taildocument.cpp

    # Cannibalised viewport cluster — copied verbatim from src/ui/ so we can
    # diverge from klogg's UX without coupling to klogg_ui internals.
    view/abstractlogview.h
    view/abstractlogview.cpp
    view/logmainview.h
    view/logmainview.cpp
    view/selection.h
    view/selection.cpp
    view/viewtools.h
    view/viewtools.cpp
    view/wrappedstring.h
    view/quickfindpattern.h
    view/quickfindpattern.cpp
    view/quickfind.h
    view/quickfind.cpp
    view/overview.h
    view/overview.cpp
    view/overviewwidget.h
    view/overviewwidget.cpp
    view/qfnotifications.h
    view/qfnotifications.cpp
    view/quickfindmux.h
    view/quickfindmux.cpp
    view/highlightedmatch.h
    view/viewinterface.h
    view/highlighterset.h
    view/highlighterset.cpp
    view/highlightersmenu.h
    view/highlightersmenu.cpp
    view/colorlabelsmanager.h
    view/colorlabelsmanager.cpp
    view/menu.h
    view/menu.cpp
    view/iconloader.h
    view/iconloader.cpp

    app/highlightingdialog.h
    app/highlightingdialog.cpp
)

set_target_properties(baretail PROPERTIES AUTOMOC ON)

target_include_directories(baretail PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/view
)

target_link_libraries(baretail PRIVATE
    klogg_logdata
    klogg_settings
    klogg_regex
    klogg_utils
    klogg_logging
    Qt${QT_VERSION_MAJOR}::Widgets
)
