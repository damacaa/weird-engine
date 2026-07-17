#pragma once

namespace WeirdEngine {
namespace WeirdRenderer {
    // Returns true if the fbdev EGL backend initialized successfully and is
    // the active video path. Always returns false when the engine is built
    // without WEIRD_USE_FBDEV_EGL support.
    bool IsFBDevEGLActive();

    // Initializes EGL directly against the framebuffer device (ARM libMali
    // fbdev winsys). Fills width/height with the native display resolution.
    bool InitFBDevEGL(int& width, int& height);

    void ShutdownFBDevEGL();

    // Presents the current frame. No-op if fbdev EGL is not active.
    void SwapFBDevBuffers();

    // GL function loader suitable for glad (uses eglGetProcAddress with a
    // dlopen fallback on libGLESv2).
    void* GetEGLProcAddress(const char* name);
}
}
