qtcode_add_module_library(qtcode_agents STATIC
    AgentAdapter.cpp
    AgentManager.cpp
    AgentModels.cpp
    AgentSession.cpp
    adapters/CodexAgentAdapter.cpp
)

target_link_libraries(qtcode_agents
    PUBLIC
        qtcode_shared
        Qt6::Core
)
