cmake_minimum_required(VERSION 3.10)

project(WeirdEngine)

execute_process(COMMAND git submodule update --init --recursive)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensure strictly standard C++
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WEIRD_ENGINE_ENABLE_ASAN "Enable AddressSanitizer for memory leak detection" OFF)
if(WEIRD_ENGINE_ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

option(WEIRD_USE_FBDEV_EGL "Use fbdev EGL wrapper (for Mali)" OFF)
if(WEIRD_USE_FBDEV_EGL)
    add_compile_definitions(WEIRD_USE_FBDEV_EGL)
endif()

message(STATUS "WeirdEngine path: ${CMAKE_CURRENT_SOURCE_DIR}")



file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.glsl")
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

add_library(glad STATIC third-party/glad/glad.c)

option(WEIRD_DISABLE_IMGUI "Disable ImGui" OFF)

if(NOT WEIRD_DISABLE_IMGUI)
# ImGui setup
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
target_include_directories(imgui
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL/include
)
endif()

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
# FORCE SDL3 to build as a STATIC library.
# This prevents the creation of libSDL3.so and embeds the code
# directly into your engine/game.
set(SDL_SHARED ON CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Build SDL static library" FORCE)
set(SDL_DEPS_SHARED ON CACHE BOOL "Load dependencies dynamically" FORCE)
set(SDL_KMSDRM_SHARED ON CACHE BOOL "Dynamically load KMS DRM support" FORCE)
# Add dependencies
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third-party/SDL)

# Force SDL3 to dynamically load KMSDRM libraries via dlopen instead of hard-linking.
# This is necessary because cmake_dependent_option overrides our FORCE cache entries.
if(TARGET SDL3-shared)
  target_compile_definitions(SDL3-shared PRIVATE
    "SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC=\"libdrm.so.2\""
    "SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC_GBM=\"libgbm.so.1\""
  )
  # Keep the include directories for compilation but remove link dependencies
  target_include_directories(SDL3-shared PRIVATE
    /usr/include/libdrm
    /usr/include
  )
  # Remove the hard link dependencies that SDL added
  get_target_property(_libs SDL3-shared LINK_LIBRARIES)
  if(_libs)
    list(FILTER _libs EXCLUDE REGEX "PC_LIBDRM|PC_GBM")
    set_target_properties(SDL3-shared PROPERTIES LINK_LIBRARIES "${_libs}")
  endif()
  get_target_property(_iface_libs SDL3-shared INTERFACE_LINK_LIBRARIES)
  if(_iface_libs)
    list(FILTER _iface_libs EXCLUDE REGEX "PC_LIBDRM|PC_GBM")
    set_target_properties(SDL3-shared PROPERTIES INTERFACE_LINK_LIBRARIES "${_iface_libs}")
  endif()
endif()
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
        # SDL headers are needed since weird-engine.h includes them
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party
)

target_include_directories(glad
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party>
        $<INSTALL_INTERFACE:third-party>
)

# This command links glad and SDL3 to your engine.
# Because you are linking to the TARGETS (glad and SDL3::SDL3), CMake automatically
# handles their include paths and other properties for you.
    target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
    target_link_libraries(${PROJECT_NAME} 
        PUBLIC 
            SDL3::SDL3
        PRIVATE 
            GLESv2
            EGL
            dl
            glad
    )
    if(NOT WEIRD_DISABLE_IMGUI)
        target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
    else()
        target_compile_definitions(${PROJECT_NAME} PUBLIC WEIRD_DISABLE_IMGUI)
    endif()

if(WEIRD_USE_FBDEV_EGL)
    target_link_libraries(${PROJECT_NAME} PUBLIC EGL GLESv2 dl)
endif()
    

# Shaders
option(WEIRD_ENGINE_USE_RUNTIME_ASSETS
    "Load shaders/assets from runtime folder instead of source tree"
    OFF
)

if (WEIRD_ENGINE_USE_RUNTIME_ASSETS)
    set(WEIRD_ENGINE_FONTS_PATH "./fonts/")
    set(WEIRD_ENGINE_SHADERS_PATH "./shaders/")
else()
    set(WEIRD_ENGINE_FONTS_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/src/weird-renderer/fonts/"
    )
    set(WEIRD_ENGINE_SHADERS_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/src/weird-renderer/shaders/"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    FONTS_PATH="${WEIRD_ENGINE_FONTS_PATH}"
    SHADERS_PATH="${WEIRD_ENGINE_SHADERS_PATH}"
)

# target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

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

# Examples
option(WEIRD_ENGINE_BUILD_EXAMPLES "Build example projects" OFF)
if(WEIRD_ENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples/3d-experiments)
    add_subdirectory(examples/opengl-experiments)
    add_subdirectory(examples/sample-scenes)
endif()
