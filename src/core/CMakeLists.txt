qtcode_add_module_library(qtcode_core STATIC
    ApplicationController.cpp
    CliCapabilityService.cpp
    ContextManager.cpp
    FileOperationService.cpp
    McpServerService.cpp
    ProjectManager.cpp
    SettingsService.cpp
    StatusService.cpp
    WorkspaceInstaller.cpp
    ../settings/SettingsModels.cpp
    ../settings/McpServerModels.cpp
)

target_compile_definitions(qtcode_core
    PRIVATE
        QTcode_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(qtcode_core
    PUBLIC
        qtcode_storage
    PRIVATE
        Qt6::Core
        Qt6::Concurrent
        KF6::I18n
        qtcode_agents
        qtcode_terminal
        qtcode_git
        qtcode_github
        qtcode_memory
)
