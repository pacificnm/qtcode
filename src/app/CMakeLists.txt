qtcode_add_executable(qtcode
    main.cpp
    QtCodeApplication.cpp
)

target_compile_definitions(qtcode
    PRIVATE
        QTcode_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(qtcode
    PRIVATE
        qtcode_ui
        qtcode_core
        qtcode_shared
        Qt6::Widgets
        KF6::CoreAddons
        KF6::I18n
)

install(TARGETS qtcode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
