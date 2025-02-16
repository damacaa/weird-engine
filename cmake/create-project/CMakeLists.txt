cmake_minimum_required(VERSION 3.10)
project(WeirdGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to switch between local and online version of WeirdEngine
option(USE_LOCAL_WEIRD_ENGINE "Use local version of WeirdEngine" OFF)

# Set paths for the local WeirdEngine
set(WEIRD_ENGINE_LOCAL_PATH "C:/Projects/weird-games/weird-engine")

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

# Mark headers as header-only so that Visual Studio treats them appropriately.
set_source_files_properties(${WEIRDGAME_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

# Add executable including both sources and headers.
add_executable(WeirdGame ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS})

# Set include directories.
target_include_directories(WeirdGame PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(WeirdGame PRIVATE
    ${CMAKE_BINARY_DIR}/weird-engine/include
    ${CMAKE_BINARY_DIR}/weird-engine/third-party/include
)

# Link to WeirdEngine.
target_link_libraries(WeirdGame PRIVATE WeirdEngine)

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
assign_source_groups(WeirdGame ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS})
