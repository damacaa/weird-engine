
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
4. Run CMake to generate the build files.  
5. Open the generated solution in the `/build` folder.  
6. Build and run your project.  

### Linux
#### Ubuntu
```
sudo apt install build-essential libx11-dev libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxrandr-dev libxxf86vm-devs libxinerama-dev
```
#### Fedora 
```
sudo dnf install libXext-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```
