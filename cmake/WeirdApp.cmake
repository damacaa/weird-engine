# Configures a WeirdEngine application executable target.
#
# On Windows, hides the console window in Release and MinSizeRel builds while
# keeping standard `int main(int argc, char* argv[])` as the entry point
# (Debug and RelWithDebInfo keep the console for logs and assertions).
function(weird_configure_app TARGET)
    if(WIN32 AND NOT EMSCRIPTEN AND NOT BUILD_WEB)
        set_target_properties(${TARGET} PROPERTIES
            WIN32_EXECUTABLE "$<CONFIG:Release,MinSizeRel>"
        )
        if(MSVC)
            target_link_options(${TARGET} PRIVATE
                "$<$<CONFIG:Release,MinSizeRel>:/ENTRY:mainCRTStartup>"
            )
        endif()
    endif()
endfunction()
