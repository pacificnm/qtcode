qtcode_add_module_library(qtcode_agents STATIC
    AgentAdapter.cpp
)

target_link_libraries(qtcode_agents
    PUBLIC
        qtcode_shared
)
