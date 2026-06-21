qtcode_add_module_library(qtcode_shared STATIC
    Logging.cpp
    PerformanceTimer.cpp
    ProcessRunner.cpp
    SecretStore.cpp
    ../core/RepoConfigLoader.cpp
)

target_link_libraries(qtcode_shared
    PUBLIC
        Qt6::Core
        KF6::Wallet
)
