qtcode_add_module_library(qtcode_git STATIC
    GitService.cpp
)

target_link_libraries(qtcode_git
    PUBLIC
        qtcode_shared
    PRIVATE
        PkgConfig::LibGit2
)
