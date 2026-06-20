find_package(Qt6 REQUIRED COMPONENTS Test)

qtcode_add_executable(qtcode_migration_runner_test
    storage/MigrationRunnerTest.cpp
)

target_link_libraries(qtcode_migration_runner_test
    PRIVATE
        qtcode_storage
        Qt6::Test
)

add_test(
    NAME migration_runner
    COMMAND qtcode_migration_runner_test
)

set_tests_properties(migration_runner PROPERTIES
    LABELS "unit;storage"
    TIMEOUT 30
)

qtcode_add_executable(qtcode_codex_agent_adapter_test
    agents/CodexAgentAdapterTest.cpp
)

target_link_libraries(qtcode_codex_agent_adapter_test
    PRIVATE
        qtcode_agents
        qtcode_memory
        Qt6::Test
)

add_test(
    NAME codex_agent_adapter
    COMMAND qtcode_codex_agent_adapter_test
)

set_tests_properties(codex_agent_adapter PROPERTIES
    LABELS "unit;agents"
    TIMEOUT 30
)

qtcode_add_executable(qtcode_settings_persistence_test
    storage/SettingsPersistenceTest.cpp
)

target_link_libraries(qtcode_settings_persistence_test
    PRIVATE
        qtcode_storage
        Qt6::Test
)

add_test(
    NAME settings_persistence
    COMMAND qtcode_settings_persistence_test
)

set_tests_properties(settings_persistence PROPERTIES
    LABELS "unit;storage;settings"
    TIMEOUT 30
)
