qtcode_add_module_library(qtcode_shared STATIC
    Logging.cpp
    ProcessRunner.cpp
)

target_link_libraries(qtcode_shared
    PUBLIC
        Qt6::Core
)
