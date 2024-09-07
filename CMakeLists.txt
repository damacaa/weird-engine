cmake_minimum_required(VERSION 3.10)

# Project Name
project(WeirdEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable
set(SOURCES
    src/main.cpp
    src/Utils.h
    src/WeirdEngine.h

    # weird-engine files
    src/weird-engine/Input.cpp
    src/weird-engine/Input.h
    src/weird-engine/ResourceManager.cpp
    src/weird-engine/ResourceManager.h
    src/weird-engine/Scene.cpp
    src/weird-engine/Scene.h
    src/weird-engine/SceneManager.cpp
    src/weird-engine/SceneManager.h

    # weird-engine/ecs files
    src/weird-engine/ecs/Component.h
    src/weird-engine/ecs/ComponentManager.h
    src/weird-engine/ecs/ECS.h
    src/weird-engine/ecs/Entity.h

    # weird-engine/ecs/Components files
    src/weird-engine/ecs/Components/Camera.h
    src/weird-engine/ecs/Components/FlyMovement.h
    src/weird-engine/ecs/Components/FlyMovement2D.h
    src/weird-engine/ecs/Components/InstancedMeshRenderer.h
    src/weird-engine/ecs/Components/LogicComponent.h
    src/weird-engine/ecs/Components/MeshRenderer.h
    src/weird-engine/ecs/Components/RigidBody.h
    src/weird-engine/ecs/Components/SDFRenderer.h
    src/weird-engine/ecs/Components/Transform.h

    # weird-engine/ecs/Systems files
    src/weird-engine/ecs/Systems/CameraSystem.h
    src/weird-engine/ecs/Systems/InstancedRenderSystem.h
    src/weird-engine/ecs/Systems/PhysicsInteractionSystem.h
    src/weird-engine/ecs/Systems/PhysicsSystem2D.h
    src/weird-engine/ecs/Systems/PlayerMovementSystem.h
    src/weird-engine/ecs/Systems/RenderSystem.h
    src/weird-engine/ecs/Systems/SDFRenderSystem.h
    src/weird-engine/ecs/Systems/SDFRenderSystem2D.h

    # weird-physics files
    src/weird-physics/Simulation.cpp
    src/weird-physics/Simulation.h
    src/weird-physics/Simulation2D.cpp
    src/weird-physics/Simulation2D.h

    # weird-physics/CollisionDetection files
    src/weird-physics/CollisionDetection/Octree.h
    src/weird-physics/CollisionDetection/SpatialHash.h
    src/weird-physics/CollisionDetection/UniformGrid2D.h

    # weird-renderer files
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
    src/weird-renderer/RenderPlane.cpp
    src/weird-renderer/RenderPlane.h
    src/weird-renderer/Shader.cpp
    src/weird-renderer/Shader.h
    src/weird-renderer/Shape.cpp
    src/weird-renderer/Shape.h
    src/weird-renderer/Shape2D.h
    src/weird-renderer/stb.cpp
    src/weird-renderer/Texture.cpp
    src/weird-renderer/Texture.h
    src/weird-renderer/VAO.cpp
    src/weird-renderer/VAO.h
    src/weird-renderer/VBO.cpp
    src/weird-renderer/VBO.h

    # shaders
    src/weird-renderer/shaders/default.frag
    src/weird-renderer/shaders/default.vert
    src/weird-renderer/shaders/default_instancing.vert
    src/weird-renderer/shaders/hg_sdf.glsl
    src/weird-renderer/shaders/output.frag
    src/weird-renderer/shaders/raymarching.frag
    src/weird-renderer/shaders/raymarching.vert
    src/weird-renderer/shaders/raymarching2d.frag

    # .vscode settings
    src/weird-renderer/shaders/.vscode/settings.json
)



# Glad needs to be compiled as well
# Add glad library
add_library(glad STATIC src/weird-renderer/glad.c)

# Include directory for glad
target_include_directories(glad PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/include)

# Ensure glad is built before WeirdEngine
add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} glad)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/weird-engine
)

# Link GLFW library
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/lib/glfw3.lib
    opengl32.lib
    glad
)

# Optional: Specify the output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)


