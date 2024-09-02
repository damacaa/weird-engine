cmake_minimum_required(VERSION 3.10)

# Project Name
project(WeirdEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Utils.h
    src/WeirdEngine.h
    src/weird-engine/Input.cpp
    src/weird-engine/Input.h
    src/weird-engine/ResourceManager.cpp
    src/weird-engine/ResourceManager.h
    src/weird-engine/Scene.cpp
    src/weird-engine/Scene.h
    src/weird-engine/SceneManager.cpp
    src/weird-engine/SceneManager.h
    src/weird-engine/ecs/Component.h
    src/weird-engine/ecs/ComponentManager.h
    src/weird-engine/ecs/ECS.h
    src/weird-engine/ecs/Entity.h
    src/weird-engine/ecs/Components/InstancedMeshRenderer.h
    src/weird-engine/ecs/Components/LogicComponent.h
    src/weird-engine/ecs/Components/MeshRenderer.h
    src/weird-engine/ecs/Components/RigidBody.h
    src/weird-engine/ecs/Components/SDFRenderer.h
    src/weird-engine/ecs/Components/Transform.h
    src/weird-engine/ecs/Systems/InstancedRenderSystem.h
    src/weird-engine/ecs/Systems/PhysicsInteractionSystem.h
    src/weird-engine/ecs/Systems/PhysicsSystem2D.h
    src/weird-engine/ecs/Systems/RenderSystem.h
    src/weird-engine/ecs/Systems/SDFRenderSystem.h
    src/weird-engine/ecs/Systems/SDFRenderSystem2D.h
    src/weird-physics/Simulation.cpp
    src/weird-physics/Simulation.h
    src/weird-physics/Simulation2D.cpp
    src/weird-physics/Simulation2D.h
    src/weird-physics/CollisionDetection/Octree.h
    src/weird-physics/CollisionDetection/SpatialHash.h
    src/weird-physics/CollisionDetection/UniformGrid2D.h
    src/weird-renderer/Camera.cpp
    src/weird-renderer/Camera.h
    src/weird-renderer/EBO.cpp
    src/weird-renderer/EBO.h
    src/weird-renderer/glad.c
    src/weird-renderer/Light.h
    src/weird-renderer/Mesh.cpp
    src/weird-renderer/Mesh.h
    src/weird-renderer/Renderer.cpp
    src/weird-renderer/Renderer.h
    # Add other source files here
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/include
    ${CMAKE_SOURCE_DIR}/src
)

# Link GLFW library
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/lib/glfw3.lib
    opengl32.lib
)

# Optional: Specify the output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# Glad needs to be compiled as well
add_library(glad STATIC src/weird-renderer/glad.c)
target_link_libraries(${PROJECT_NAME} PRIVATE glad)
