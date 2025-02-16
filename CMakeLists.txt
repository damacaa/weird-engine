cmake_minimum_required(VERSION 3.10)

# Project Name
project(WeirdEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure we use the Multi-threaded DLL runtime library
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Create a static library
add_library(${PROJECT_NAME} STATIC ${SOURCES})

# Add glad library as static
add_library(glad STATIC src/weird-renderer/glad.c)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third-party/include
)

target_include_directories(glad PRIVATE ${CMAKE_SOURCE_DIR}/third-party/include)

# Link dependencies (since this is a static library, linking is usually handled by the final executable)
target_link_libraries(${PROJECT_NAME} PUBLIC glad)

# Optional: Specify the output directory for the static library
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)
