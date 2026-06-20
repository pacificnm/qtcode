qtcode_add_module_library(qtcode_github STATIC
    GitHubRemoteResolver.cpp
    GitHubService.cpp
)

target_link_libraries(qtcode_github
    PUBLIC
        qtcode_shared
)
