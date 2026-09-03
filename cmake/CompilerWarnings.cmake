function(set_shiftech_warnings target)
    set(WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        # We use partial brace-init of small POD result structs (all trailing members
        # have sane default member initializers); this warning fights that idiom.
        -Wno-missing-field-initializers
    )
    if(SHIFTECH_WARNINGS_AS_ERRORS)
        list(APPEND WARNINGS -Werror)
    endif()
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${WARNINGS}>)
endfunction()
