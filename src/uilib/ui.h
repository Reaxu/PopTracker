#ifndef _UILIB_UI_H
#define _UILIB_UI_H


#include "window.h"
#include <map>
#include <string>
#include <mutex>
#include <list>
#include <vector>
#include <stdint.h>


#define DEFAULT_FPS_LIMIT 120
#define DEFAULT_SOFTWARE_FPS_LIMIT 60


namespace Ui {

class Ui {
public:
    struct Hotkey {
        int id;
        int key;
        int mod;
    };

protected:
    std::map<Window::ID,Window*> _windows;
    std::string _name;
    unsigned _lastRenderDuration = 0;
    uint64_t _lastFrameMicroTimestamp = 0;
    unsigned _globalMouseButtons = 0; ///< bit mask of pressed mouse buttons during last capture
    Uint32 _lastEventType = 0;
    bool _fallbackRenderer = false;
    unsigned _fpsLimit = 0;
    unsigned _hardwareFpsLimit = DEFAULT_FPS_LIMIT;
    unsigned _softwareFpsLimit = DEFAULT_SOFTWARE_FPS_LIMIT;

    std::mutex _eventMutex;
    static int eventFilter(void *userdata, SDL_Event *event);

    std::list<Hotkey> _hotkeys;

    // touch gesture state, see touch.cpp
    enum class TouchState {
        Idle,        ///< no finger down
        Pending,     ///< one finger down, could become tap, long press or drag
        Dragging,    ///< one finger moving: pan/scroll
        Pinching,    ///< two fingers: zoom + pan
        LongPressed, ///< long press fired (right click), wait for release
        Ignoring,    ///< ignore until all fingers are up
    };
    struct Finger {
        SDL_FingerID id;
        float x, y; ///< current position in logical window coordinates
    };
    std::vector<Finger> _fingers;
    TouchState _touchState = TouchState::Idle;
    Window::ID _touchWindowId = 0;
    uint32_t _touchStartTicks = 0;
    float _touchStartX = 0, _touchStartY = 0;
    float _touchSentX = 0, _touchSentY = 0; ///< position up to which pan deltas were delivered
    float _pinchDist = 0;
    bool _backgrounded = false;

    Window* touchWindow();
    void handleFingerEvent(const SDL_Event& ev);
    void updateTouch();
    void touchClick(Window* win, int x, int y, int button);
    void handleBack(Window* win);

public:
    Ui(const char *name, bool fallbackRenderer, bool vsync);
    virtual ~Ui();
    template <class T>
    T *createWindow(const char *title, SDL_Surface* icon=nullptr, const Position& pos={-1,-1}, const Size& size={0,0}, const WindowConfig& config={})
    {
        T *win = new T(title,icon,pos,size,config);
        this->_windows[win->getID()] = win;
        return win;
    }
    void destroyWindow(Window *win);
    bool render();

    void setFPSLimit(unsigned hw_fps, unsigned sw_fps)
    {
        _hardwareFpsLimit = hw_fps;
        _softwareFpsLimit = sw_fps;
    }

    void addHotkey(const Hotkey&);
    void addHotkey(Hotkey&&);

    /// Change pos so that window is visible and can be moved (title bar on Windows or alt+drag on Linux)
    void makeWindowVisible(Position& pos, const Size& size) const;

    Signal<Window*> onWindowDestroyed;
    Signal<const Hotkey&> onHotkey;
    /// Back button (Android) / Escape (touch mode) that no window handled
    Signal<> onBack;
    /// App moved to background (Android) - save state now, the process may get killed
    Signal<> onAppBackground;
};

} // namespace Ui

#endif // _UILIB_UI_H
