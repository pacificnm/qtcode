qtcode_add_module_library(qtcode_storage STATIC
    StorageService.cpp
)

target_link_libraries(qtcode_storage
    PUBLIC
        qtcode_shared
    PRIVATE
        SQLite::SQLite3
)
