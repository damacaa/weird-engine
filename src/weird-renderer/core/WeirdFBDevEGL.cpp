#include "weird-renderer/core/WeirdFBDevEGL.h"

#ifdef WEIRD_USE_FBDEV_EGL
#include <EGL/egl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace WeirdEngine {
namespace WeirdRenderer {

namespace {
    // ARM libMali fbdev winsys expects a pointer to this struct as the
    // EGLNativeWindowType.
    struct fbdev_window {
        unsigned short width;
        unsigned short height;
    };

    EGLDisplay s_display = EGL_NO_DISPLAY;
    EGLSurface s_surface = EGL_NO_SURFACE;
    EGLContext s_context = EGL_NO_CONTEXT;
    fbdev_window s_nativeWindow{0, 0};
    bool s_active = false;

    const char* eglErrorString(EGLint error) {
        switch (error) {
        case EGL_SUCCESS: return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
        default: return "UNKNOWN";
        }
    }

    void logEGLError(const char* where) {
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {
            std::cerr << "[FBDevEGL] " << where << " failed: 0x" << std::hex << error << std::dec
                      << " (" << eglErrorString(error) << ")" << std::endl;
        }
    }
} // namespace

bool IsFBDevEGLActive() {
    return s_active;
}

bool InitFBDevEGL(int& width, int& height) {
    std::cout << "[FBDevEGL] Initializing framebuffer EGL backend..." << std::endl;

    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        std::cerr << "[FBDevEGL] Failed to open /dev/fb0 (errno=" << errno << ")" << std::endl;
        return false;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        std::cerr << "[FBDevEGL] Failed to get fb var screeninfo" << std::endl;
        close(fbfd);
        return false;
    }
    close(fbfd);

    width = vinfo.xres;
    height = vinfo.yres;
    std::cout << "[FBDevEGL] fb0: " << width << "x" << height
              << " (virtual " << vinfo.xres_virtual << "x" << vinfo.yres_virtual
              << ", " << vinfo.bits_per_pixel << " bpp)" << std::endl;

    // Log client extensions (EGL 1.5 only, harmless if it fails)
    const char* clientExts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    std::cout << "[FBDevEGL] Client extensions: " << (clientExts ? clientExts : "<unavailable>") << std::endl;

    std::cout << "[FBDevEGL] eglGetDisplay..." << std::endl;
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_display == EGL_NO_DISPLAY) {
        logEGLError("eglGetDisplay");
        return false;
    }

    EGLint eglMajor = 0, eglMinor = 0;
    if (!eglInitialize(s_display, &eglMajor, &eglMinor)) {
        logEGLError("eglInitialize");
        s_display = EGL_NO_DISPLAY;
        return false;
    }

    std::cout << "[FBDevEGL] EGL " << eglMajor << "." << eglMinor << std::endl;
    std::cout << "[FBDevEGL] Vendor: " << eglQueryString(s_display, EGL_VENDOR) << std::endl;
    std::cout << "[FBDevEGL] Version: " << eglQueryString(s_display, EGL_VERSION) << std::endl;
    std::cout << "[FBDevEGL] Client APIs: " << eglQueryString(s_display, EGL_CLIENT_APIS) << std::endl;
    std::cout << "[FBDevEGL] Extensions: " << eglQueryString(s_display, EGL_EXTENSIONS) << std::endl;

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        logEGLError("eglBindAPI(EGL_OPENGL_ES_API)");
        return false;
    }

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    eglChooseConfig(s_display, attribs, &config, 1, &numConfigs);
    if (numConfigs == 0 || !config) {
        logEGLError("eglChooseConfig");
        std::cerr << "[FBDevEGL] No suitable EGL config found" << std::endl;
        return false;
    }
    std::cout << "[FBDevEGL] Config chosen (" << numConfigs << " matched)" << std::endl;

    s_nativeWindow = fbdev_window{(unsigned short)width, (unsigned short)height};

    std::cout << "[FBDevEGL] eglCreateWindowSurface (" << width << "x" << height << ")..." << std::endl;
    s_surface = eglCreateWindowSurface(s_display, config, (EGLNativeWindowType)&s_nativeWindow, nullptr);
    if (s_surface == EGL_NO_SURFACE) {
        logEGLError("eglCreateWindowSurface");
        return false;
    }

    EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    std::cout << "[FBDevEGL] eglCreateContext (ES3)..." << std::endl;
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (s_context == EGL_NO_CONTEXT) {
        logEGLError("eglCreateContext");
        return false;
    }

    if (!eglMakeCurrent(s_display, s_surface, s_surface, s_context)) {
        logEGLError("eglMakeCurrent");
        return false;
    }

    eglSwapInterval(s_display, 1);
    logEGLError("eglSwapInterval"); // informational only

    s_active = true;
    std::cout << "[FBDevEGL] Initialization successful!" << std::endl;
    return true;
}

void ShutdownFBDevEGL() {
    if (s_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_context != EGL_NO_CONTEXT) {
            eglDestroyContext(s_display, s_context);
            s_context = EGL_NO_CONTEXT;
        }
        if (s_surface != EGL_NO_SURFACE) {
            eglDestroySurface(s_display, s_surface);
            s_surface = EGL_NO_SURFACE;
        }
        eglTerminate(s_display);
        s_display = EGL_NO_DISPLAY;
    }
    s_active = false;
}

void SwapFBDevBuffers() {
    if (s_active) {
        eglSwapBuffers(s_display, s_surface);
    }
}

void* GetEGLProcAddress(const char* name) {
    void* p = (void*)eglGetProcAddress(name);
    if (!p) {
        static void* glesLib = nullptr;
        static bool triedToLoad = false;
        if (!triedToLoad) {
            glesLib = dlopen("libGLESv2.so.2", RTLD_LAZY);
            if (!glesLib) {
                std::cout << "[FBDevEGL] dlopen libGLESv2.so.2 failed: " << dlerror() << std::endl;
                glesLib = dlopen("libGLESv2.so", RTLD_LAZY);
                if (!glesLib) {
                    std::cout << "[FBDevEGL] dlopen libGLESv2.so failed: " << dlerror() << std::endl;
                }
            }
            triedToLoad = true;
        }
        if (glesLib) {
            p = dlsym(glesLib, name);
        }
    }
    return p;
}

} // namespace WeirdRenderer
} // namespace WeirdEngine

#else // !WEIRD_USE_FBDEV_EGL

namespace WeirdEngine {
namespace WeirdRenderer {

bool IsFBDevEGLActive() { return false; }
bool InitFBDevEGL(int&, int&) { return false; }
void ShutdownFBDevEGL() {}
void SwapFBDevBuffers() {}
void* GetEGLProcAddress(const char*) { return nullptr; }

} // namespace WeirdRenderer
} // namespace WeirdEngine
#endif
