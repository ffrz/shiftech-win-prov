function(set_shiftech_warnings target)
    set(WARNINGS
        -Wall
        -Wextra
        -Wpedantic
    )
    if(SHIFTECH_WARNINGS_AS_ERRORS)
        list(APPEND WARNINGS -Werror)
    endif()
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${WARNINGS}>)
endfunction()
