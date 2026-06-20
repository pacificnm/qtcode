# Module dependency direction:
#   ui -> core -> integrations -> shared
#   core -> storage
#   integrations -> shared
#   storage -> shared

add_subdirectory(shared)
add_subdirectory(storage)
add_subdirectory(agents)
add_subdirectory(terminal)
add_subdirectory(git)
add_subdirectory(github)
add_subdirectory(memory)
add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(app)
