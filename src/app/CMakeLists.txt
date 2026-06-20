qtcode_add_executable(qtcode
    main.cpp
)

target_link_libraries(qtcode
    PRIVATE
        qtcode_ui
)

install(TARGETS qtcode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
