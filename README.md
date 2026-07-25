
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
```

## Anbernic muOS Deployment

Weird Engine includes generic scripts for building and deploying games directly to Anbernic handheld consoles running muOS over MTP. These scripts are located in `scripts/anbernic/`.

You can use these generic scripts to deploy *any* game built with Weird Engine without needing to copy the scripts to your game's folder. The scripts automatically detect your project's name, cross-compile it via Podman, package assets, generate launcher scripts, and push the files to the device.

### Prerequisites

- [Podman](https://podman.io/) installed on your machine (used to safely isolate the cross-compiler toolchain).
- The device must be connected to your PC via USB and mounted via MTP (e.g., `mtp:/RG35XX-H/SD2`).

### Deploying a Game

To build and deploy a game to the console:

```bash
# General Usage
/path/to/weird-engine/scripts/anbernic/deploy-muos.sh <path_to_game_project> <mtp_base_path>

# Example: Deploying a game from its own directory
cd my-awesome-game
../weird-engine/scripts/anbernic/deploy-muos.sh . mtp:/RG35XX-H/SD2
```

### Fetching Device Logs

If you need to retrieve `log.txt` or screenshots from the device after running your game:

```bash
../weird-engine/scripts/anbernic/fetch-logs.sh . mtp:/RG35XX-H/SD2
```
Logs will be saved to a timestamped folder inside your project's `device-logs/` directory.
