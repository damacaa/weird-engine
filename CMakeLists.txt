cmake_minimum_required(VERSION 3.10)

project(WeirdEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "WeirdEngine path: ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag")
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

add_library(glad STATIC third-party/glad/glad.c)

target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(glad 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third-party>
        $<INSTALL_INTERFACE:third-party>
)



# SDL3 setup
# Add dependencies
set(SDL_WINDOWS_GAMEINPUT OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third-party/SDL)
# add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third-party/SDL_image)
# add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third-party/SDL_mixer)
# add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third-party/SDL_net)
# add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third-party/SDL_ttf)
       
# This is where we fix things.
# Replace all your old target_include_directories calls with this block.

target_include_directories(${PROJECT_NAME}
    PUBLIC
        # Anyone using WeirdEngine needs access to its public headers.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # The line below is for when you create an installable package later.
        $<INSTALL_INTERFACE:include>

    PRIVATE
        # Only WeirdEngine's own .cpp files need the SDL headers directly.
        # The consumer (WeirdSamples) will get them via the public headers above.
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL/include
)

target_include_directories(glad
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party>
        $<INSTALL_INTERFACE:third-party>
)

# This command links glad and SDL3 to your engine.
# Because you are linking to the TARGETS (glad and SDL3::SDL3), CMake automatically
# handles their include paths and other properties for you.
target_link_libraries(${PROJECT_NAME} PUBLIC glad)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
    

# Shaders
target_compile_definitions(${PROJECT_NAME} PUBLIC
    ENGINE_PATH="${CMAKE_CURRENT_SOURCE_DIR}"
    SHADERS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/weird-renderer/shaders/"
)

#
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)



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

assign_source_groups(${PROJECT_NAME} ${SOURCES} ${HEADERS})
