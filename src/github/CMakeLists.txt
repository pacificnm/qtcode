qtcode_add_module_library(qtcode_github STATIC
    GitHubService.cpp
)

target_link_libraries(qtcode_github
    PUBLIC
        qtcode_shared
)
