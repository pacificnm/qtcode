qtcode_add_module_library(qtcode_github STATIC
    GhCliClient.cpp
    GitHubRemoteResolver.cpp
    GitHubService.cpp
)

target_link_libraries(qtcode_github
    PUBLIC
        qtcode_shared
    PRIVATE
        qtcode_storage
)
