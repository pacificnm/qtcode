qtcode_add_module_library(qtcode_commands STATIC
    CommandLibraryService.cpp
    QtCommandModels.cpp
    QtCommandYamlParser.cpp
)

target_link_libraries(qtcode_commands
    PUBLIC
        qtcode_shared
        Qt6::Core
)
