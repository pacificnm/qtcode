qtcode_add_module_library(qtcode_memory STATIC
    MemoryService.cpp
    McpClient.cpp
    McpHealthResult.cpp
)

target_link_libraries(qtcode_memory
    PUBLIC
        qtcode_shared
    PRIVATE
        Qt6::Core
        Qt6::Concurrent
)
