cmake_minimum_required(VERSION 3.10)
project(WeirdEngineTools)

# Output all tool executables into the same directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/bin")

add_subdirectory(cluster-editor)
add_subdirectory(scene-editor)
add_subdirectory(shape-editor)
