cmake_minimum_required(VERSION 3.10)

# Project Name
project(WeirdEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define ENGINE_PATH to the local path
add_definitions(-DENGINE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")

# Ensure we use the Multi-threaded DLL runtime library
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# Collect source files
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Create a static library
add_library(${PROJECT_NAME} STATIC ${SOURCES})

# Add glad library as static
add_library(glad STATIC src/weird-renderer/glad.c)

# Public interface include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third-party/include>
        $<INSTALL_INTERFACE:third-party/include>
)

# For glad, expose its include directory similarly
target_include_directories(glad 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third-party/include>
        $<INSTALL_INTERFACE:third-party/include>
)

# Link glad into WeirdEngine
target_link_libraries(${PROJECT_NAME} PUBLIC glad)

# Manually link the precompiled GLFW library (adjust the filename if needed)
set(GLFW_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/third-party/lib")
target_link_libraries(${PROJECT_NAME} PUBLIC "${GLFW_LIB_DIR}/glfw3.lib")

# Optional: Specify the output directory for the static library
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Export both targets: WeirdEngine and glad
export(TARGETS ${PROJECT_NAME} glad FILE ${CMAKE_BINARY_DIR}/WeirdEngineConfig.cmake)

# Install targets
install(TARGETS ${PROJECT_NAME} glad EXPORT WeirdEngineConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install public header directories
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY third-party/include/ DESTINATION third-party/include)

# Export the CMake configuration for consumers
install(EXPORT WeirdEngineConfig
    FILE WeirdEngineConfig.cmake
    NAMESPACE WeirdEngine::
    DESTINATION lib/cmake/WeirdEngine
)

message(STATUS "WeirdEngine include dirs: ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "WeirdEngine third-party dirs: ${CMAKE_CURRENT_LIST_DIR}/third-party/include")
