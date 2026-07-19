cmake_minimum_required(VERSION 3.10)
project(MoleculeEditorTool)

# Create folders if they don't exist
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_LOCAL_WEIRD_ENGINE "Use local version of WeirdEngine" ON)
set(WEIRD_ENGINE_LOCAL_PATH "../../")
option(DEPLOY_STANDALONE "Configure paths and copy assets for a standalone distribution" OFF)

if(DEPLOY_STANDALONE)
    message(STATUS ">> DEPLOY MODE: Assets will be copied to output. Paths set to relative.")
    set(WEIRD_ENGINE_USE_RUNTIME_ASSETS ON CACHE BOOL "" FORCE)
else()
    message(STATUS ">> DEV MODE: Assets read directly from source folder.")
endif()

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

file(GLOB_RECURSE WEIRDGAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE WEIRDGAME_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
file(GLOB_RECURSE WEIRDGAME_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.*")

set_source_files_properties(${WEIRDGAME_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
add_executable(${PROJECT_NAME} ${WEIRDGAME_SOURCES} ${WEIRDGAME_HEADERS} ${WEIRDGAME_ASSETS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
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

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/fonts" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${WEIRD_ENGINE_REAL_SOURCE}/src/weird-renderer/shaders" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMENT "Copying Assets, Fonts, and Shaders"
)
