qtcode_add_module_library(qtcode_core STATIC
    ApplicationController.cpp
    CliCapabilityService.cpp
    ContextManager.cpp
    FileOperationService.cpp
    McpServerService.cpp
    ProjectManager.cpp
    SettingsService.cpp
    StatusService.cpp
    ../settings/SettingsModels.cpp
    ../settings/McpServerModels.cpp
)

target_link_libraries(qtcode_core
    PUBLIC
        qtcode_storage
    PRIVATE
        Qt6::Core
        Qt6::Concurrent
        qtcode_agents
        qtcode_terminal
        qtcode_git
        qtcode_github
        qtcode_memory
)
