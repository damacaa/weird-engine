cmake_minimum_required(VERSION 3.10)

# Project Name
project(WeirdEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define ENGINE_PATH to the local path
# add_definitions(-DENGINE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")

message(STATUS "WeirdEngine path: ${CMAKE_CURRENT_SOURCE_DIR}")

# Ensure we use the Multi-threaded DLL runtime library
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# Collect source files
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag")

# Collect header files
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

# Create a static library (Explicitly add headers)
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

# Add glad library as static
add_library(glad STATIC third-party/glad/glad.c)

# Public interface include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third-party>
        $<INSTALL_INTERFACE:third-party>
)

target_include_directories(glad 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third-party>
        $<INSTALL_INTERFACE:third-party>
)

# Link glad into WeirdEngine
target_link_libraries(${PROJECT_NAME} PUBLIC glad)

# Fetch and build GLFW
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8  # Change to the latest stable version if needed
)
FetchContent_MakeAvailable(glfw)
set_target_properties(glfw PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link GLFW dynamically into WeirdEngine
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

# Define ENGINE_PATH
target_compile_definitions(${PROJECT_NAME} PUBLIC ENGINE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")

# Define SHADERS_PATH
target_compile_definitions(${PROJECT_NAME} PUBLIC SHADERS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/weird-renderer/shaders/")

# Optional: Specify the output directory for the static library
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Export both targets: WeirdEngine, glad, and glfw
export(TARGETS ${PROJECT_NAME} glad glfw FILE ${CMAKE_BINARY_DIR}/WeirdEngineConfig.cmake)


# Install targets
install(TARGETS ${PROJECT_NAME} glad EXPORT WeirdEngineConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install public header directories
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY third-party DESTINATION third-party)

# Export the CMake configuration for consumers
install(EXPORT WeirdEngineConfig
    FILE WeirdEngineConfig.cmake
    NAMESPACE WeirdEngine::
    DESTINATION lib/cmake/WeirdEngine
)

# ----------------------------------------------------------
# Helper function to assign source groups based on folder structure.
function(assign_source_groups TARGET)
    foreach(source_file IN LISTS ARGN)
        # Get the full path to the file's directory.
        get_filename_component(FILE_PATH "${source_file}" PATH)
        # Compute the path relative to the project's root.
        file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_PATH}")
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
assign_source_groups(${PROJECT_NAME} ${SOURCES} ${HEADERS})