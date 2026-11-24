// Touch gesture recognition for Ui::Ui. See touch.h for an overview.
//
//   tap                  -> left click (mouse move + down + up at the finger position)
//   long press           -> right click
//   one finger drag      -> Widget::touchPan (scroll lists, pan a zoomed view)
//   two finger pinch     -> Widget::touchZoom + touchPan around the center of the fingers
//
// Coordinates are converted to logical window coordinates (see Window::getUiScale).

#include "ui.h"
#include "touch.h"
#include <algorithm>
#include <cmath>

namespace Ui {

Window* Ui::touchWindow()
{
    auto it = _windows.find(_touchWindowId);
    if (it != _windows.end())
        return it->second;
    return nullptr;
}

void Ui::touchClick(Window* win, int x, int y, int button)
{
    // hover first, so widgets that show information on hover (map locations, tool bar text) update
    win->onMouseMove.emit(win, x, y, 0);
    win->onMouseDown.emit(win, x, y, button);
    win->onClick.emit(win, x, y, button);
}

void Ui::handleBack(Window* win)
{
    if (win && win->handleBack())
        return;
    onBack.emit(this);
}

static float fingerDistance(float x1, float y1, float x2, float y2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return std::sqrt(dx*dx + dy*dy);
}

void Ui::handleFingerEvent(const SDL_Event& ev)
{
    const auto& tf = ev.tfinger;

    // find the window; SDL >= 2.0.12 sets windowID for touch events
    Window* win = nullptr;
    {
        auto it = _windows.find(tf.windowID);
        if (it != _windows.end())
            win = it->second;
        else if (!_windows.empty() && _touchState == TouchState::Idle)
            win = _windows.begin()->second; // single window (Android)
        else
            win = touchWindow();
    }
    if (!win)
        return;
    if (_touchState == TouchState::Idle)
        _touchWindowId = win->getID();
    else if (win->getID() != _touchWindowId)
        return; // gesture belongs to another window

    // normalized -> logical window coordinates
    const float x = tf.x * (float)win->getWidth();
    const float y = tf.y * (float)win->getHeight();

    auto fingerIt = std::find_if(_fingers.begin(), _fingers.end(), [&](const Finger& f) {
        return f.id == tf.fingerId;
    });

    switch (ev.type) {
        case SDL_FINGERDOWN: {
            if (fingerIt == _fingers.end())
                _fingers.push_back({tf.fingerId, x, y});
            else
                *fingerIt = {tf.fingerId, x, y};

            if (_fingers.size() == 1 && _touchState == TouchState::Idle) {
                _touchState = TouchState::Pending;
                _touchStartTicks = SDL_GetTicks();
                _touchStartX = _touchSentX = x;
                _touchStartY = _touchSentY = y;
            } else if (_fingers.size() == 2 &&
                    (_touchState == TouchState::Pending || _touchState == TouchState::Dragging)) {
                // second finger: start pinch / two finger pan
                _touchState = TouchState::Pinching;
                const auto& a = _fingers[0];
                const auto& b = _fingers[1];
                _pinchDist = fingerDistance(a.x, a.y, b.x, b.y);
                _touchSentX = (a.x + b.x) / 2;
                _touchSentY = (a.y + b.y) / 2;
            } else if (_fingers.size() > 2) {
                _touchState = TouchState::Ignoring;
            }
            break;
        }

        case SDL_FINGERMOTION: {
            if (fingerIt == _fingers.end())
                break;
            fingerIt->x = x;
            fingerIt->y = y;

            if (_touchState == TouchState::Pending) {
                const float slop = Touch::SLOP_DP;
                if (fingerDistance(_touchStartX, _touchStartY, x, y) > slop)
                    _touchState = TouchState::Dragging;
            }
            if (_touchState == TouchState::Dragging) {
                const int dx = (int)std::lround(x - _touchSentX);
                const int dy = (int)std::lround(y - _touchSentY);
                if (dx || dy) {
                    win->touchPan((int)_touchStartX, (int)_touchStartY, dx, dy);
                    _touchSentX += (float)dx;
                    _touchSentY += (float)dy;
                }
            } else if (_touchState == TouchState::Pinching && _fingers.size() >= 2) {
                const auto& a = _fingers[0];
                const auto& b = _fingers[1];
                const float cx = (a.x + b.x) / 2;
                const float cy = (a.y + b.y) / 2;
                const float dist = fingerDistance(a.x, a.y, b.x, b.y);
                if (_pinchDist > 1.0f && dist > 1.0f) {
                    const float factor = dist / _pinchDist;
                    if (std::fabs(factor - 1.0f) > 0.002f) {
                        win->touchZoom((int)cx, (int)cy, factor);
                        _pinchDist = dist;
                    }
                }
                const int dx = (int)std::lround(cx - _touchSentX);
                const int dy = (int)std::lround(cy - _touchSentY);
                if (dx || dy) {
                    win->touchPan((int)cx, (int)cy, dx, dy);
                    _touchSentX += (float)dx;
                    _touchSentY += (float)dy;
                }
            }
            break;
        }

        case SDL_FINGERUP: {
            if (fingerIt != _fingers.end())
                _fingers.erase(fingerIt);

            if (_touchState == TouchState::Pending && _fingers.empty()) {
                // tap
                touchClick(win, (int)_touchStartX, (int)_touchStartY, BUTTON_LEFT);
            } else if (_touchState == TouchState::Pinching && !_fingers.empty()) {
                // lifting one finger of a pinch should not turn into a drag jump
                _touchState = TouchState::Ignoring;
            }
            if (_fingers.empty())
                _touchState = TouchState::Idle;
            break;
        }

        default:
            break;
    }
}

void Ui::updateTouch()
{
    if (_touchState != TouchState::Pending)
        return;
    if (SDL_GetTicks() - _touchStartTicks < Touch::LONG_PRESS_MS)
        return;
    _touchState = TouchState::LongPressed;
    if (Window* win = touchWindow())
        touchClick(win, (int)_touchStartX, (int)_touchStartY, BUTTON_RIGHT);
}

} // namespace Ui
