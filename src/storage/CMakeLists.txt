qtcode_add_module_library(qtcode_storage STATIC
    StorageService.cpp
    StorageTransaction.cpp
    MigrationRunner.cpp
    repositories/SettingsRepository.cpp
    repositories/ProjectRepository.cpp
    repositories/TerminalSessionRepository.cpp
    repositories/TerminalProfileStore.cpp
)

target_link_libraries(qtcode_storage
    PUBLIC
        qtcode_shared
        Qt6::Sql
)
