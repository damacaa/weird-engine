
# Weird Engine  

## Overview  

Weird Engine is a simple yet unique game engine featuring:  

- **Custom OpenGL Renderer**: Uses ray marching to render 2D Signed Distance Fields (SDFs).  
- **Physics Engine**: Implements Position-Based Dynamics (PBD) with custom SDF-based collision detection.  
- **ECS Architecture**: A fully custom-built Entity Component System (ECS).  

## Getting Started  

### Creating Your First Project  

1. Navigate to `/CMake/create-project`.  
2. Place your header files (`.h`) in the `/include` directory.  
3. Place your source files (`.cpp`) in the `/src` directory.  
   - You can create subfolders for better organization.  
4. Build witch CMake and run your project.  

### Linux
You'll need to install SDL3 dependencies:
[SDL3 Linux README](https://wiki.libsdl.org/SDL3/README-linux)

### Issues downloading SDL submodule
```
git rm --cached third-party/SDL
rm -rf .git/modules/third-party/SDL
rm -rf third-party/SDL

git submodule add https://github.com/libsdl-org/SDL.git third-party/SDL
git submodule update --init --recursive
```
