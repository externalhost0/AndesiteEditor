#pragma once
#ifdef __APPLE__
#include <functional>
struct SDL_Window;
namespace Andesite {
    struct GestureCallbacks {
        std::function<void(float magnification, float cx, float cy)> onMagnify;
        std::function<bool(float dx, float dy)> onPan; // return true to consume, false to pass through
    };
    void RegisterMacOSGestures(SDL_Window* window, GestureCallbacks callbacks);
    void UnregisterMacOSGestures();
}
#endif
