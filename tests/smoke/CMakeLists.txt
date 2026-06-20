add_test(
    NAME smoke_startup
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/startup_smoke_test.sh"
            "$<TARGET_FILE:qtcode>"
)

set_tests_properties(smoke_startup PROPERTIES
    LABELS "smoke"
    TIMEOUT 60
)

add_test(
    NAME smoke_repository_add_open
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/repository_smoke_test.sh"
            "$<TARGET_FILE:qtcode>"
            "${CMAKE_SOURCE_DIR}"
            "${SQLITE3_EXECUTABLE}"
)

set_tests_properties(smoke_repository_add_open PROPERTIES
    LABELS "smoke"
    TIMEOUT 120
)
