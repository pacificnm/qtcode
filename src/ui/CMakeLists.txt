qtcode_add_module_library(qtcode_ui STATIC
    MainWindow.cpp
)

target_link_libraries(qtcode_ui
    PUBLIC
        qtcode_core
    PRIVATE
        Qt6::Widgets
        KF6::CoreAddons
        KF6::I18n
)
