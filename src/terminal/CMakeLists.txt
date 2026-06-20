qtcode_add_module_library(qtcode_terminal STATIC
    TerminalManager.cpp
    TerminalProfile.cpp
)

target_link_libraries(qtcode_terminal
    PUBLIC
        qtcode_shared
        qtcode_storage
    PRIVATE
        Qt6::Widgets
        PkgConfig::QTermWidget
)
