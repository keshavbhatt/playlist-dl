# Adds the project-wide warning set to a target.
# Usage: pldl_set_warnings(<target>)
#
# The code base predates the CMake port and is not clean under the stricter
# conversion warnings yet, so those stay off until the sources are tidied.
function(pldl_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(PLDL_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wnon-virtual-dtor
            -Woverloaded-virtual
        )
        if(PLDL_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
