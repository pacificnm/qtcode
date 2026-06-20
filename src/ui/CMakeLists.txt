qtcode_add_module_library(qtcode_ui STATIC
    MainWindow.cpp
    models/RepositoryListModel.cpp
    panels/RepositoryPanel.cpp
    panels/AgentPanel.cpp
    panels/TerminalPanel.cpp
)

target_link_libraries(qtcode_ui
    PUBLIC
        qtcode_core
    PRIVATE
        qtcode_shared
        qtcode_git
        Qt6::Widgets
        Qt6::Concurrent
        KF6::CoreAddons
        KF6::I18n
        PkgConfig::QTermWidget
)
