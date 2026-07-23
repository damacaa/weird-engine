cmake_minimum_required(VERSION 3.10)

# ==============================================================================
# 1. Project Configuration
# ==============================================================================
project(OpenGLExperiments)

# Path to the WeirdEngine directory. Change this depending on where your project is located.
# Examples inside weird-engine use "../../"
# External projects usually use "../weird-engine/" or similar.
set(WEIRD_ENGINE_LOCAL_PATH "../../")
# ==============================================================================

# Create folders if they don't exist
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensure strictly standard C++

# Option to switch between local and online version of WeirdEngine
option(USE_LOCAL_WEIRD_ENGINE "Use local version of WeirdEngine" ON)

# Option to build for web (Emscripten)
option(BUILD_WEB "Build for web using Emscripten" OFF)

# Option to build for standalone deployment
option(DEPLOY_STANDALONE "Configure paths and copy assets for a standalone distribution" OFF)

# Auto-detect Emscripten toolchain or explicit BUILD_WEB flag
if(EMSCRIPTEN OR BUILD_WEB)
    set(BUILD_WEB ON CACHE BOOL "" FORCE)
    set(BUILD_WEB ON)
    set(DEPLOY_STANDALONE ON CACHE BOOL "" FORCE)
    set(DEPLOY_STANDALONE ON)
endif()

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
if(NOT TARGET WeirdEngine)
    if(USE_LOCAL_WEIRD_ENGINE)
        # Use the local version of WeirdEngine
        message(STATUS "Using local version of WeirdEngine at ${WEIRD_ENGINE_LOCAL_PATH}")
        add_subdirectory(${WEIRD_ENGINE_LOCAL_PATH} ${CMAKE_BINARY_DIR}/weird-engine)
        
        # Resolve the local path to an absolute path for copying files later
        get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
    else()
        # Fetch WeirdEngine from GitHub
        message(STATUS "Using online version of WeirdEngine")
        set(WEIRD_ENGINE_GIT_REF "main")
        
        include(FetchContent)
        FetchContent_Declare(
            WeirdEngine
            GIT_REPOSITORY https://github.com/damacaa/weird-engine
            GIT_TAG ${WEIRD_ENGINE_GIT_REF}
        )
        FetchContent_MakeAvailable(WeirdEngine)
        
        # FetchContent defines <name>_SOURCE_DIR (usually lowercase)
        set(WEIRD_ENGINE_REAL_SOURCE "${weirdengine_SOURCE_DIR}")
    endif()
else()
    # WeirdEngine already defined (built from root); resolve its source path
    get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
endif()

# Glob source files and header files from the proper directories.
file(GLOB_RECURSE WEIRDGAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE WEIRDGAME_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)
file(GLOB_RECURSE WEIRDGAME_ASSETS 
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.*"
)

# Mark headers as header-only so that Visual Studio treats them appropriately.
set_source_files_properties(${WEIRDGAME_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

# Add executable including both sources and headers.
add_executable(${PROJECT_NAME} ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS} ${WEIRDGAME_ASSETS})

if(BUILD_WEB)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
endif()

# Set include directories.
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

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
if(DEPLOY_STANDALONE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # 1. Copy Project ASSETS folder -> Build/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"

        # 2. Copy WeirdEngine FONTS -> Build/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/fonts"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts"

        # 3. Copy WeirdEngine SHADERS -> Build/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/shaders"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"

        COMMENT "Copying Game Assets, Engine Fonts, and Engine Shaders"
    )

    if(BUILD_WEB)
        # For web builds, copy index.html if it exists
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/index.html")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/index.html"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/index.html"
                COMMENT "Copying index.html for web build"
            )
        endif()
    endif()
endif()

# -------------------------------------------------------------------------
# SOURCE GROUPS
# -------------------------------------------------------------------------
function(assign_source_groups TARGET)
    foreach(source_file IN LISTS ARGN)
        get_filename_component(FILE_PATH "${source_file}" PATH)
        file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_PATH}")
        string(REPLACE "/" "\\" FILTER_PATH "${REL_PATH}")
        if(FILTER_PATH STREQUAL "")
            set(FILTER_PATH "Root")
        endif()
        source_group("${FILTER_PATH}" FILES "${source_file}")
    endforeach()
endfunction()

assign_source_groups(${PROJECT_NAME} ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS} ${WEIRDGAME_ASSETS})
