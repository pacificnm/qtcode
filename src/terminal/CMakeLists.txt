qtcode_add_module_library(qtcode_terminal STATIC
    TerminalManager.cpp
)

target_link_libraries(qtcode_terminal
    PUBLIC
        qtcode_shared
    PRIVATE
        Qt6::Widgets
        PkgConfig::QTermWidget
)
