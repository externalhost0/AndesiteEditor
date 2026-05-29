#import <Cocoa/Cocoa.h>
#include <SDL3/SDL.h>
#include "MacOSGestures.h"

namespace Andesite {

static id s_magnifyMonitor = nil;
static id s_scrollMonitor  = nil;

void RegisterMacOSGestures(SDL_Window* window, GestureCallbacks callbacks)
{
    NSWindow* nsWindow = (__bridge NSWindow*)
        SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                               SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (!nsWindow) return;

    s_magnifyMonitor =
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMagnify
                                              handler:^NSEvent*(NSEvent* event) {
            // Y-flip required as NSWindow origin is bottom-left but imgui uses top-left coords
            const CGFloat h = nsWindow.contentView.bounds.size.height;
            callbacks.onMagnify((float)event.magnification,
                                (float)event.locationInWindow.x,
                                (float)(h - event.locationInWindow.y));
            return nil; // consume, prevent SDL and imgui from seeing it
        }];

    s_scrollMonitor =
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                              handler:^NSEvent*(NSEvent* event) {
            // if hasPreciseScrollingDeltas == YES: trackpad or magic mouse
            //if  hasPreciseScrollingDeltas == NO: discrete mouse wheel and thus we let SDL handle zoom
            if (!event.hasPreciseScrollingDeltas)
                return event;
            const bool consumed = callbacks.onPan((float)event.scrollingDeltaX,
                                                   (float)event.scrollingDeltaY);
            return consumed ? nil : event;
        }];
}

void UnregisterMacOSGestures()
{
    if (s_magnifyMonitor) { [NSEvent removeMonitor:s_magnifyMonitor]; s_magnifyMonitor = nil; }
    if (s_scrollMonitor)  { [NSEvent removeMonitor:s_scrollMonitor];  s_scrollMonitor  = nil; }
}

} // namespace Andesite
