qtcode_add_executable(qtcode
    main.cpp
)

install(TARGETS qtcode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
