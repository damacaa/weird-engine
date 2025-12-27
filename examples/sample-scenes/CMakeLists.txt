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

# Option to build for standalone deployment
option(DEPLOY_STANDALONE "Configure paths and copy assets for a standalone distribution" ON)

if(DEPLOY_STANDALONE)
    message(STATUS ">> DEPLOY MODE: Assets will be copied to output. Paths set to relative.")
    # Force the engine to expect runtime assets
    set(WEIRD_ENGINE_USE_RUNTIME_ASSETS ON CACHE BOOL "" FORCE)
else()
    message(STATUS ">> DEV MODE: Assets read directly from source folder.")
endif()

# -------------------------------------------------------------------------
# INCLUDE ENGINE & CAPTURE SOURCE PATH
# -------------------------------------------------------------------------
if(USE_LOCAL_WEIRD_ENGINE)
    # Use the local version of WeirdEngine
    message(STATUS "Using local version of WeirdEngine")
    add_subdirectory(${WEIRD_ENGINE_LOCAL_PATH} ${CMAKE_BINARY_DIR}/weird-engine)
    
    # Resolve the local path to an absolute path for copying files later
    get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
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
    
    # FetchContent defines <name>_SOURCE_DIR (usually lowercase)
    set(WEIRD_ENGINE_REAL_SOURCE "${weirdengine_SOURCE_DIR}")
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

if(DEPLOY_STANDALONE)
    set(ASSETS_PATH "./assets/")
else()
    set(ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
endif()

message(STATUS "Assets path: ${ASSETS_PATH}")
target_compile_definitions(${PROJECT_NAME} PUBLIC ASSETS_PATH="${ASSETS_PATH}")

if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# -------------------------------------------------------------------------
# POST BUILD COMMANDS (DLLs + Assets + Fonts + Shaders)
# -------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # 1. Copy DLLs (SDL3)
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #    $<TARGET_FILE:SDL3::SDL3>
    #    $<TARGET_FILE_DIR:${PROJECT_NAME}>

    # 2. Copy Project ASSETS folder -> Build/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"

    # 3. Copy WeirdEngine FONTS -> Build/fonts
    # We look into the captured WEIRD_ENGINE_REAL_SOURCE/src/weird-renderer/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/fonts"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts"

    # 4. Copy WeirdEngine SHADERS -> Build/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"

    COMMENT "Copying Runtime DLLs, Game Assets, Engine Fonts, and Engine Shaders"
)

# -------------------------------------------------------------------------
# SOURCE GROUPS
# -------------------------------------------------------------------------
function(assign_source_groups TARGET)
    foreach(source_file IN LISTS ARGN)
        get_filename_component(FILE_PATH "${source_file}" PATH)
        file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}" "${FILE_PATH}")
        string(REPLACE "/" "\\" FILTER_PATH "${REL_PATH}")
        if(FILTER_PATH STREQUAL "")
            set(FILTER_PATH "Root")
        endif()
        source_group("${FILTER_PATH}" FILES "${source_file}")
    endforeach()
endfunction()

assign_source_groups(${PROJECT_NAME} ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS} ${WEIRDGAME_ASSETS})