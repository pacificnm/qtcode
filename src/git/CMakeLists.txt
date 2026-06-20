qtcode_add_module_library(qtcode_git STATIC
    GitService.cpp
)

target_link_libraries(qtcode_git
    PUBLIC
        qtcode_shared
        Qt6::Core
    PRIVATE
        PkgConfig::LibGit2
)
