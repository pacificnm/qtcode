qtcode_add_module_library(qtcode_agents STATIC
    AgentAdapter.cpp
    AgentManager.cpp
    AgentModels.cpp
    AgentOptions.cpp
    AgentSession.cpp
    DiffApplier.cpp
    adapters/CodexAgentAdapter.cpp
    adapters/CursorAgentAdapter.cpp
)

target_link_libraries(qtcode_agents
    PUBLIC
        qtcode_shared
        Qt6::Core
    PRIVATE
        qtcode_storage
)
