cmake_minimum_required(VERSION 3.10)

# ==============================================================================
# 1. Project Configuration
# ==============================================================================
project(WeirdSdfNodeEditor)

set(WEIRD_ENGINE_LOCAL_PATH "../../")

# Create folders if they don't exist
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_LOCAL_WEIRD_ENGINE "Use local version of WeirdEngine" ON)
option(BUILD_WEB "Build for web using Emscripten" OFF)
option(DEPLOY_STANDALONE "Configure paths and copy assets for a standalone distribution" OFF)

if(EMSCRIPTEN OR BUILD_WEB)
    set(BUILD_WEB ON CACHE BOOL "" FORCE)
    set(BUILD_WEB ON)
    set(DEPLOY_STANDALONE ON CACHE BOOL "" FORCE)
    set(DEPLOY_STANDALONE ON)
endif()

if(DEPLOY_STANDALONE)
    set(WEIRD_ENGINE_USE_RUNTIME_ASSETS ON CACHE BOOL "" FORCE)
endif()

# -------------------------------------------------------------------------
# INCLUDE ENGINE & CAPTURE SOURCE PATH
# -------------------------------------------------------------------------
if(NOT TARGET WeirdEngine)
    if(USE_LOCAL_WEIRD_ENGINE)
        add_subdirectory(${WEIRD_ENGINE_LOCAL_PATH} ${CMAKE_BINARY_DIR}/weird-engine)
        get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
    else()
        include(FetchContent)
        FetchContent_Declare(
            WeirdEngine
            GIT_REPOSITORY https://github.com/damacaa/weird-engine
            GIT_TAG main
        )
        FetchContent_MakeAvailable(WeirdEngine)
        set(WEIRD_ENGINE_REAL_SOURCE "${weirdengine_SOURCE_DIR}")
    endif()
else()
    get_filename_component(WEIRD_ENGINE_REAL_SOURCE "${WEIRD_ENGINE_LOCAL_PATH}" ABSOLUTE)
endif()

file(GLOB_RECURSE TOOL_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imnodes/*.cpp"
)
file(GLOB_RECURSE TOOL_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imnodes/*.h"
)

set_source_files_properties(${TOOL_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

add_executable(${PROJECT_NAME} ${TOOL_SOURCES} ${TOOL_HEADERS})

if(BUILD_WEB)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/imnodes"
)

target_link_libraries(${PROJECT_NAME} PRIVATE WeirdEngine)

if(DEPLOY_STANDALONE)
    set(ASSETS_PATH "./assets/")
else()
    set(ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC ASSETS_PATH="${ASSETS_PATH}")

if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(DEPLOY_STANDALONE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/fonts"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/shaders"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        COMMENT "Copying Tool Assets, Engine Fonts, and Engine Shaders"
    )

    if(BUILD_WEB)
        # For web builds, generate the HTML shell from the shared template
        set(WEIRD_WEB_TITLE "SdfNodeEditor")
        configure_file(
            "${WEIRD_ENGINE_REAL_SOURCE}/cmake/web/index.html"
            "${CMAKE_CURRENT_BINARY_DIR}/web/index.html"
            @ONLY
        )
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/web/index.html"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/index.html"
            COMMENT "Copying index.html for web build"
        )
    endif()
endif()
