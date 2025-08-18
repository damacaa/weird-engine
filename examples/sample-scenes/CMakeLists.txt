cmake_minimum_required(VERSION 3.10)
project(WeirdSamples)

# Create folders if they don't exist
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/assets")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/src")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to switch between local and online version of WeirdEngine
option(USE_LOCAL_WEIRD_ENGINE "Use local version of WeirdEngine" ON)

# Set paths for the local WeirdEngine
set(WEIRD_ENGINE_LOCAL_PATH "../../")

if(USE_LOCAL_WEIRD_ENGINE)
    # Use the local version of WeirdEngine
    message(STATUS "Using local version of WeirdEngine")
    add_subdirectory(${WEIRD_ENGINE_LOCAL_PATH} ${CMAKE_BINARY_DIR}/weird-engine)
else()
    # Fetch WeirdEngine from GitHub
    message(STATUS "Using online version of WeirdEngine")
    include(FetchContent)
    FetchContent_Declare(
        WeirdEngine
        GIT_REPOSITORY https://github.com/damacaa/weird-engine
        GIT_TAG main
    )
    FetchContent_MakeAvailable(WeirdEngine)
endif()

# Glob source files and header files from the proper directories.
file(GLOB_RECURSE WEIRDGAME_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE WEIRDGAME_HEADERS 
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)
file(GLOB_RECURSE WEIRDGAME_ASSETS 
    "${CMAKE_SOURCE_DIR}/assets/*.*"
)

# Mark headers as header-only so that Visual Studio treats them appropriately.
set_source_files_properties(${WEIRDGAME_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

# Add executable including both sources and headers.
add_executable(${PROJECT_NAME} ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS} ${WEIRDGAME_ASSETS})

# Set include directories.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link to WeirdEngine.
target_link_libraries(${PROJECT_NAME} PRIVATE WeirdEngine)

# Set Asset Path Macro
# Define the assets path variable
set(ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
# set(ASSETS_PATH "./assets") # Set the asset path macro in release mode to a relative path that assumes the assets folder is in the same directory as the game executable

# Print the assets path for debugging or confirmation
message(STATUS "Assets path: ${ASSETS_PATH}")

# Use the variable in target_compile_definitions
target_compile_definitions(${PROJECT_NAME} PUBLIC ASSETS_PATH="${ASSETS_PATH}")

# Example for the future with more libraries
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # Command for SDL3
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>

    # Command for SDL3_image
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     $<TARGET_FILE:SDL3_image::SDL3_image>  #<-- Note the target name might be different
    #     $<TARGET_FILE_DIR:${PROJECT_NAME}>

    COMMENT "Copying runtime DLLs to executable directory"
)

# ----------------------------------------------------------
# Helper function to assign source groups based on folder structure.
function(assign_source_groups TARGET)
    foreach(source_file IN LISTS ARGN)
        # Get the full path to the file's directory.
        get_filename_component(FILE_PATH "${source_file}" PATH)
        # Compute the path relative to the project's root.
        file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}" "${FILE_PATH}")
        # Replace forward slashes with backslashes for Visual Studio filter naming.
        string(REPLACE "/" "\\" FILTER_PATH "${REL_PATH}")
        if(FILTER_PATH STREQUAL "")
            set(FILTER_PATH "Root")
        endif()
        # Assign the file to the computed filter.
        source_group("${FILTER_PATH}" FILES "${source_file}")
    endforeach()
endfunction()

# Assign source groups for all your files.
assign_source_groups(${PROJECT_NAME} ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS} ${WEIRDGAME_ASSETS})
