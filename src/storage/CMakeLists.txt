qtcode_add_module_library(qtcode_storage STATIC
    StorageService.cpp
    StorageTransaction.cpp
)

target_link_libraries(qtcode_storage
    PUBLIC
        qtcode_shared
        Qt6::Sql
)
