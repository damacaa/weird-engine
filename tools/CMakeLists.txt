cmake_minimum_required(VERSION 3.10)

# ==============================================================================
# Project Configuration
# ==============================================================================
project(WeirdEditor)

set(WEIRD_ENGINE_LOCAL_PATH "../")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build option: compile only the Shape Editor (no Main Menu, Scene Editor, or Cluster Editor)
option(WEIRD_TOOLS_SHAPE_EDITOR_ONLY
    "Compile only the Shape Editor (excludes Main Menu, Scene Editor, and Cluster Editor)"
    OFF
)

# Standalone / web / engine source options (used when built standalone via cmake -S tools)
option(USE_LOCAL_WEIRD_ENGINE "Use local version of WeirdEngine" ON)
option(BUILD_WEB "Build for web using Emscripten" OFF)
option(DEPLOY_STANDALONE "Configure paths and copy assets for a standalone distribution" OFF)

# Output tool binaries into ${CMAKE_BINARY_DIR}/tools/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/bin")

if(EMSCRIPTEN OR BUILD_WEB)
    set(BUILD_WEB ON CACHE BOOL "" FORCE)
    set(BUILD_WEB ON)
    set(DEPLOY_STANDALONE ON CACHE BOOL "" FORCE)
    set(DEPLOY_STANDALONE ON)
endif()

if(DEPLOY_STANDALONE)
    message(STATUS ">> DEPLOY MODE: Assets will be copied to output. Paths set to relative.")
    set(WEIRD_ENGINE_USE_RUNTIME_ASSETS ON CACHE BOOL "" FORCE)
else()
    message(STATUS ">> DEV MODE: Assets read directly from source folder.")
endif()

# -------------------------------------------------------------------------
# INCLUDE ENGINE & CAPTURE SOURCE PATH
# -------------------------------------------------------------------------
if(NOT TARGET WeirdEngine)
    if(USE_LOCAL_WEIRD_ENGINE)
        message(STATUS "Using local version of WeirdEngine at ${WEIRD_ENGINE_LOCAL_PATH}")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${WEIRD_ENGINE_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/weird-engine")
        get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
    else()
        message(STATUS "Using online version of WeirdEngine")
        set(WEIRD_ENGINE_GIT_REF "main")

        include(FetchContent)
        FetchContent_Declare(
            WeirdEngine
            GIT_REPOSITORY https://github.com/damacaa/weird-engine
            GIT_TAG ${WEIRD_ENGINE_GIT_REF}
        )
        FetchContent_MakeAvailable(WeirdEngine)
        set(WEIRD_ENGINE_REAL_SOURCE "${weirdengine_SOURCE_DIR}")
    endif()
else()
    get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
endif()

# -------------------------------------------------------------------------
# SOURCES & TARGET SELECTION
# -------------------------------------------------------------------------
file(GLOB_RECURSE WEIRD_TOOLS_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.*")

if(WEIRD_TOOLS_SHAPE_EDITOR_ONLY)
    set(WEIRD_TOOLS_TARGET WeirdShapeEditor)
    set(WEIRD_WEB_TITLE "WeirdShapeEditor")

    file(GLOB_RECURSE WEIRD_TOOLS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shape-editor/*.cpp"
    )
    list(APPEND WEIRD_TOOLS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imnodes/imnodes.cpp"
    )

    file(GLOB_RECURSE WEIRD_TOOLS_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/shape-editor/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/shape-editor/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imnodes/*.h"
    )
    message(STATUS "WeirdEditor: Configuring Shape Editor ONLY mode (target: ${WEIRD_TOOLS_TARGET})")
else()
    set(WEIRD_TOOLS_TARGET WeirdEditor)
    set(WEIRD_WEB_TITLE "WeirdEditor")

    file(GLOB_RECURSE WEIRD_TOOLS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    )
    list(APPEND WEIRD_TOOLS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/imnodes/imnodes.cpp"
    )

    file(GLOB_RECURSE WEIRD_TOOLS_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imnodes/*.h"
    )
    message(STATUS "WeirdEditor: Configuring unified editor suite (target: ${WEIRD_TOOLS_TARGET})")
endif()

set_source_files_properties(${WEIRD_TOOLS_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

add_executable(${WEIRD_TOOLS_TARGET}
    ${WEIRD_TOOLS_SOURCES}
    ${WEIRD_TOOLS_HEADERS}
    ${WEIRD_TOOLS_ASSETS}
)

if(WEIRD_TOOLS_SHAPE_EDITOR_ONLY)
    target_compile_definitions(${WEIRD_TOOLS_TARGET} PRIVATE WEIRD_TOOLS_SHAPE_EDITOR_ONLY=1)
    add_custom_target(WeirdEditor DEPENDS ${WEIRD_TOOLS_TARGET})
endif()

if(BUILD_WEB)
    set_target_properties(${WEIRD_TOOLS_TARGET} PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
endif()

target_include_directories(${WEIRD_TOOLS_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/imnodes"
)

target_link_libraries(${WEIRD_TOOLS_TARGET} PRIVATE WeirdEngine)

if(WIN32 AND NOT WEIRD_TOOLS_SHAPE_EDITOR_ONLY)
    target_link_libraries(${WEIRD_TOOLS_TARGET} PRIVATE ole32 shell32 uuid)
endif()

# Asset Path Macro
if(DEPLOY_STANDALONE)
    set(ASSETS_PATH "./assets/")
else()
    set(ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
endif()

message(STATUS "WeirdEditor assets path: ${ASSETS_PATH}")
target_compile_definitions(${WEIRD_TOOLS_TARGET} PUBLIC ASSETS_PATH="${ASSETS_PATH}")

if(MINGW)
    target_link_options(${WEIRD_TOOLS_TARGET} PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# -------------------------------------------------------------------------
# POST BUILD COMMANDS (Assets + Fonts + Shaders)
# -------------------------------------------------------------------------
if(DEPLOY_STANDALONE)
    add_custom_command(TARGET ${WEIRD_TOOLS_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${WEIRD_TOOLS_TARGET}>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/fonts"
            "$<TARGET_FILE_DIR:${WEIRD_TOOLS_TARGET}>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/shaders"
            "$<TARGET_FILE_DIR:${WEIRD_TOOLS_TARGET}>/shaders"
        COMMENT "Copying Tools Assets, Engine Fonts, and Engine Shaders"
    )

    if(BUILD_WEB)
        configure_file(
            "${WEIRD_ENGINE_REAL_SOURCE}/cmake/web/index.html"
            "${CMAKE_CURRENT_BINARY_DIR}/web/index.html"
            @ONLY
        )
        add_custom_command(TARGET ${WEIRD_TOOLS_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/web/index.html"
                "$<TARGET_FILE_DIR:${WEIRD_TOOLS_TARGET}>/index.html"
            COMMENT "Copying index.html for web build"
        )
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

assign_source_groups(${WEIRD_TOOLS_TARGET} ${WEIRD_TOOLS_SOURCES} ${WEIRD_TOOLS_HEADERS} ${WEIRD_TOOLS_ASSETS})
