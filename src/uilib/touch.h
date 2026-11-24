#pragma once

// Touch / mobile UI support.
//
// When touch mode is enabled (always on Android, opt-in on desktop via config "touch_ui" or the environment variable
// POPTRACKER_TOUCH_UI=1), the UI
//   * renders every window at a UI scale (density-independent pixels on Android),
//   * turns finger input into gestures: tap = left click, long press = right click, drag = scroll/pan,
//     pinch = zoom (see Ui::Ui::handleFingerEvent),
//   * wraps tracker layouts in a ZoomView that fits them to the screen,
//   * uses larger tool bars and a menu instead of hotkeys and extra windows.

namespace Ui {

class Touch final {
public:
    Touch() = delete;

    /// Touch-friendly UI enabled
    static bool enabled() { return _enabled; }
    static void setEnabled(bool enabled) { _enabled = enabled; }

    /// Only a single SDL window is supported (Android). Extra windows (broadcast, settings) become overlays.
    static bool singleWindow() { return _singleWindow; }
    static void setSingleWindow(bool singleWindow) { _singleWindow = singleWindow; }

    /// UI scale for new windows (1 = 1 logical pixel per screen pixel). 0 = auto (display density).
    static float requestedScale() { return _requestedScale; }
    static void setRequestedScale(float scale) { _requestedScale = scale; }

    /// Text oversampling: fonts are rasterized at this factor and drawn scaled down, so text stays sharp when the
    /// renderer scales the UI up. Must be set before the first window is created.
    static int textScale() { return _textScale; }
    static void setTextScale(int scale) { _textScale = scale < 1 ? 1 : scale; }

    /// Convert a size in rasterized text pixels to logical pixels
    static int fromTextPixels(int v) { return (v + _textScale - 1) / _textScale; }

    /// Height of tool bars and minimum height of touch targets in logical pixels
    static int barHeight() { return _enabled ? 44 : 32; }
    static int rowHeight() { return _enabled ? 40 : 0; }

    /// Gesture tuning
    static constexpr unsigned LONG_PRESS_MS = 450;
    static constexpr float SLOP_DP = 10.0f; ///< finger movement before a tap becomes a drag (in logical px)
    static constexpr float MAX_ZOOM = 6.0f;

private:
    static inline bool _enabled = false;
    static inline bool _singleWindow = false;
    static inline float _requestedScale = 0.0f;
    static inline int _textScale = 1;
};

} // namespace Ui
