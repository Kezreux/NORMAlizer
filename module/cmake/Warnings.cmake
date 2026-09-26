# Shared warning configuration for the project's own targets.
#
# The fetched dependencies (Asio, Catch2, pybind11) are deliberately left alone:
# putting -Werror on the command line would apply it to their sources too, and a
# warning in someone else's code is not something this build can fix.

# Turns on the warning set this project builds with, and -Werror/WX when
# NORMA_WERROR is ON.
function(norma_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(NORMA_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(NORMA_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
