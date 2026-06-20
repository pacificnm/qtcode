function(qtcode_apply_target_defaults target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

function(qtcode_add_library name type)
    add_library(${name} ${type} ${ARGN})
    qtcode_apply_target_defaults(${name})
endfunction()

function(qtcode_add_executable name)
    add_executable(${name} ${ARGN})
    qtcode_apply_target_defaults(${name})
endfunction()
