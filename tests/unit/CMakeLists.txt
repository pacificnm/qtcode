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

qtcode_add_executable(qtcode_cursor_agent_adapter_test
    agents/CursorAgentAdapterTest.cpp
)

target_link_libraries(qtcode_cursor_agent_adapter_test
    PRIVATE
        qtcode_agents
        qtcode_memory
        Qt6::Test
)

add_test(
    NAME cursor_agent_adapter
    COMMAND qtcode_cursor_agent_adapter_test
)

set_tests_properties(cursor_agent_adapter PROPERTIES
    LABELS "unit;agents"
    TIMEOUT 30
)

qtcode_add_executable(qtcode_agent_manager_conversation_test
    agents/AgentManagerConversationTest.cpp
)

target_link_libraries(qtcode_agent_manager_conversation_test
    PRIVATE
        qtcode_agents
        qtcode_memory
        qtcode_storage
        Qt6::Test
)

add_test(
    NAME agent_manager_conversation
    COMMAND qtcode_agent_manager_conversation_test
)

set_tests_properties(agent_manager_conversation PROPERTIES
    LABELS "unit;agents"
    TIMEOUT 30
)

qtcode_add_executable(qtcode_agent_manager_active_session_test
    agents/AgentManagerActiveSessionTest.cpp
)

target_link_libraries(qtcode_agent_manager_active_session_test
    PRIVATE
        qtcode_agents
        qtcode_memory
        qtcode_storage
        Qt6::Test
)

add_test(
    NAME agent_manager_active_session
    COMMAND qtcode_agent_manager_active_session_test
)

set_tests_properties(agent_manager_active_session PROPERTIES
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

qtcode_add_executable(qtcode_file_operation_service_test
    core/FileOperationServiceTest.cpp
)

target_link_libraries(qtcode_file_operation_service_test
    PRIVATE
        qtcode_core
        Qt6::Test
)

add_test(
    NAME file_operation_service
    COMMAND qtcode_file_operation_service_test
)

set_tests_properties(file_operation_service PROPERTIES
    LABELS "unit;core"
    TIMEOUT 30
)

qtcode_add_executable(qtcode_workspace_tabs_test
    ui/WorkspaceTabsTest.cpp
)

target_link_libraries(qtcode_workspace_tabs_test
    PRIVATE
        qtcode_ui
        Qt6::Test
        Qt6::Widgets
        KF6::CoreAddons
        KF6::I18n
        KF6::TextEditor
)

add_test(
    NAME workspace_tabs
    COMMAND qtcode_workspace_tabs_test
)

set_tests_properties(workspace_tabs PROPERTIES
    LABELS "unit;ui;editor"
    TIMEOUT 60
)

qtcode_add_executable(qtcode_terminal_manager_working_directory_test
    terminal/TerminalManagerWorkingDirectoryTest.cpp
)

target_link_libraries(qtcode_terminal_manager_working_directory_test
    PRIVATE
        qtcode_terminal
        qtcode_storage
        Qt6::Test
)

add_test(
    NAME terminal_manager_working_directory
    COMMAND qtcode_terminal_manager_working_directory_test
)

set_tests_properties(terminal_manager_working_directory PROPERTIES
    LABELS "unit;terminal"
    TIMEOUT 30
)
