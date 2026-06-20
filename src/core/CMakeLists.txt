qtcode_add_module_library(qtcode_core STATIC
    ApplicationController.cpp
    ../settings/SettingsModels.cpp
)

target_link_libraries(qtcode_core
    PUBLIC
        qtcode_storage
    PRIVATE
        qtcode_agents
        qtcode_terminal
        qtcode_git
        qtcode_github
        qtcode_memory
)
