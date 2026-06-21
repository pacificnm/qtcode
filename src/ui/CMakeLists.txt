qtcode_add_module_library(qtcode_ui STATIC
    MainWindow.cpp
    models/RepositoryListModel.cpp
    panels/RepositoryPanel.cpp
    panels/AgentPanel.cpp
    panels/McpServerPanel.cpp
    panels/TerminalPanel.cpp
    views/ConversationView.cpp
    views/DiffReviewView.cpp
    views/ContextResultsView.cpp
    views/GitHubDetailView.cpp
)

target_link_libraries(qtcode_ui
    PUBLIC
        qtcode_core
    PRIVATE
        qtcode_shared
        qtcode_memory
        qtcode_git
        qtcode_github
        qtcode_terminal
        Qt6::Widgets
        Qt6::Concurrent
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
)
