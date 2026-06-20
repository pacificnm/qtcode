qtcode_add_module_library(qtcode_memory STATIC
    MemoryService.cpp
)

target_link_libraries(qtcode_memory
    PUBLIC
        qtcode_shared
)
