#include "window.h"
#include "../core/assets.h"
#include "../ui/defaults.h" // DEFAULT_FONT_*
#include "tooltip.h"
#include "texturemanager.h"
#include "touch.h"
#include <cmath>
#include <SDL2/SDL_syswm.h>
#include <algorithm>
#include <string>


namespace Ui {

//#define DEFAULT_SCALE_QUALITY_HINT "1"

Window::Window(const char *title, SDL_Surface* icon, const Position& pos, const Size& size, const WindowConfig& config)
    : Container(0,0,600,400)
{
    _title = title;
    if (size.width>0 && size.height>0) _size = size;
    (void)config;
    
    printf("Creating renderer ...");
    // NOTE: on Android the window always fills the activity, size and position are ignored
    _win = SDL_CreateWindow(title,
            pos.left, pos.top,
            _size.width, _size.height, SDL_WINDOW_RESIZABLE);
    if (!_win) {
        printf(" error\n");
        fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
        return;
    }
    
    if (icon) setIcon(icon);
    
#ifdef DEFAULT_SCALE_QUALITY_HINT
    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, DEFAULT_SCALE_QUALITY_HINT, SDL_HINT_DEFAULT);
#endif

    constexpr Uint32 flags = 0; // default / from hints
    _ren = SDL_CreateRenderer(_win, -1, flags);
    if (!_ren) {
        printf(" error\n");
        fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
        return;
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(_ren, &info) == 0) {
        _rendererFlags = info.flags;
        const bool vsync = !!(_rendererFlags & SDL_RENDERER_PRESENTVSYNC);
        printf(" %s%s\n", info.name, vsync ? " with vsync" : "");
    } else {
        printf(" unknown\n");
    }
    
    SDL_SetRenderDrawBlendMode(_ren, SDL_BLENDMODE_BLEND);

    if (Touch::enabled()) {
        float scale = Touch::requestedScale();
        if (scale <= 0.0f) {
            // auto: Android-style density independent pixels (160 dpi = 1.0)
            float ddpi = 0;
            int display = SDL_GetWindowDisplayIndex(_win);
            if (SDL_GetDisplayDPI(display < 0 ? 0 : display, &ddpi, nullptr, nullptr) == 0 && ddpi > 0)
                scale = ddpi / 160.0f;
            else
                scale = 1.0f;
        }
        _uiScale = std::max(0.5f, std::min(8.0f, scale));
        SDL_RenderSetScale(_ren, _uiScale, _uiScale);
        Size px = getPixelSize();
        Container::setSize({(int)(px.width / _uiScale), (int)(px.height / _uiScale)});
        printf("Ui scale: %.2f, logical size %dx%d\n", _uiScale, _size.width, _size.height);
    }
    
    printf("Loading font ...\n");
    _fontStore = new FontStore();
    _font = _fontStore->getFont(DEFAULT_FONT_NAME, DEFAULT_FONT_SIZE);

    onMouseMove += {this, [this](void*, int x, int y, unsigned) {
        _lastMousePos = {x, y};
    }};
}

Window::~Window()
{
    printf("Destroying window...\n");
    // NOTE: we have to destroy children before destroying the renderer
    clearChildren();
    TextureManager::remove(_ren);
    if (_fontStore) delete _fontStore;
    if (_ren) SDL_DestroyRenderer(_ren);
    if (_win) SDL_DestroyWindow(_win);
    _font = nullptr;
    _fontStore = nullptr;
    _ren = nullptr;
    _win = nullptr;
}

void Window::clear()
{
    auto& c = _backgroundColor;
    if (SDL_SetRenderDrawColor(_ren, c.r, c.g, c.b, 255) != 0) {
        fprintf(stderr, "Window: error setting draw color: %s\n", SDL_GetError());
    }
    if (SDL_RenderClear(_ren) != 0) {
        fprintf(stderr, "Window: error clearing: %s\n", SDL_GetError());
    }
}
void Window::present()
{
    SDL_RenderPresent(_ren);
}

void Window::render(Renderer renderer, int offX, int offY)
{
    Container::render(renderer, offX, offY);
}

void Window::render()
{
    clear();
    render(_ren, 0, 0);
    present();
}

void Window::showTooltip(Tooltip* tooltip, Position pos)
{
    if (tooltip) {
        if (pos != Position::UNDEFINED)
            tooltip->setPosition(pos);
        else {
            auto autoPos = _lastMousePos + Size(Tooltip::OFFSET, Tooltip::OFFSET);
            auto endPos = autoPos + tooltip->getSize();
            if (endPos.left > _size.width)
                autoPos.left -= endPos.left - _size.width + Tooltip::OFFSET;
            if (autoPos.left < 0)
                autoPos.left = 0;
            if (endPos.top > _size.height)
                autoPos.top -= endPos.top - _size.height + Tooltip::OFFSET;
            if (autoPos.top < 0)
                autoPos.top = 0;
            tooltip->setPosition(autoPos);
        }
    }

    if (tooltip == _tooltip)
        return;

    if (_tooltip) {
        removeChild(_tooltip);
        delete _tooltip;
    }

    _tooltip = tooltip;
    if (_tooltip)
        addChild(_tooltip);
}

void Window::showTooltip(const std::string& text, Position pos)
{
    Tooltip* tooltip = new Tooltip(_font, text, _size);
    showTooltip(tooltip, pos);
}

Window::ID Window::getID()
{
    return SDL_GetWindowID(_win);
}

void Window::setIcon(SDL_Surface *icon)
{
    SDL_SetWindowIcon(_win,icon);
}

void Window::setTitle(const std::string& title)
{
    SDL_SetWindowTitle(_win,title.c_str());
}

void Window::resize(Size size)
{
    if (Touch::enabled()) {
        // window size is controlled by the OS/user; content adapts instead (see ZoomView)
        return;
    }
    SDL_SetWindowSize(_win, size.width, size.height);
    setSize(size);
}

Size Window::getPixelSize() const
{
    int w = 0, h = 0;
    if (_ren && SDL_GetRendererOutputSize(_ren, &w, &h) == 0 && w > 0 && h > 0)
        return {w, h};
    SDL_GetWindowSize(_win, &w, &h);
    return {w, h};
}

void Window::onPixelSizeChanged(Size pixelSize)
{
    if (_uiScale == 1.0f) {
        setSize(pixelSize);
    } else {
        setSize({(int)std::floor(pixelSize.width / _uiScale), (int)std::floor(pixelSize.height / _uiScale)});
    }
}

void Window::setUiScale(float scale)
{
    scale = std::max(0.5f, std::min(8.0f, scale));
    if (scale == _uiScale)
        return;
    _uiScale = scale;
    if (_ren)
        SDL_RenderSetScale(_ren, _uiScale, _uiScale);
    onPixelSizeChanged(getPixelSize());
}

void Window::Raise()
{
    SDL_RaiseWindow(_win);
}

static Position tmp; // FIXME: this is not a good solution
const Position& Window::getPosition() const
{
    SDL_GetWindowPosition(_win, &tmp.left, &tmp.top);
    return tmp;
}
Position Window::getPlacementPosition() const
{
    // this returns window coordinates to use in create_window to restore the window
    // NOTE: window position differs between creation and after on X11 by
    //       ~title bar + border. This works around it.
    
    Position pos;
    int offX=0, offY=0;
    SDL_GetWindowPosition(_win, &pos.left, &pos.top);
    
    SDL_SysWMinfo wminfo = {};
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWindowWMInfo(_win, &wminfo) && wminfo.subsystem == SDL_SYSWM_X11) {
        SDL_GetWindowBordersSize(_win, &offY, &offX, nullptr, nullptr);
    }
    return { pos.left - offX, pos.top - offY };
}
void Window::setPosition(const Position& pos)
{
    // _pos has to stay {0,0} for the render() to work
    SDL_SetWindowPosition(_win, pos.left, pos.top);
}

void Window::setMinSize(Size size)
{
    Container::setMinSize(size);
    if (Touch::enabled())
        return; // see resize()
    // limit min window width/height to of 5/6 of the screen min. dimension
    int w = 720;
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
        w = std::min(dm.w, dm.h) * 5 / 6;
    size = size && Size{w,w};
    SDL_SetWindowMinimumSize(_win, size.width, size.height);
}

int Window::getDisplay() const
{
    return SDL_GetWindowDisplayIndex(_win);
}

std::string Window::getDisplayName() const
{
    const char *name = SDL_GetDisplayName(getDisplay());
    return name ? name : "";
}


Position Window::getPositionOnDisplay() const
{
    SDL_Rect rect = {};
    SDL_GetDisplayBounds(getDisplay(), &rect);
    auto pos = getPosition();
    return { pos.left - rect.x, pos.top - rect.y };
}

void Window::grabFocus()
{
    SDL_SetWindowInputFocus(_win);
}

void Window::setKeyboardFocus(Widget* w)
{
    if (_keyboardFocus == w) return;
    if (_keyboardFocus) {
        SDL_StopTextInput();
        _keyboardFocus->onDestroy -= this;
    }
    _keyboardFocus = w;
    if (_keyboardFocus) {
        SDL_StartTextInput();
        _keyboardFocus->onDestroy += {this, [this](void* destroyed) {
            if (_keyboardFocus == destroyed) _keyboardFocus = nullptr;
        }};
    }
}

bool Window::getAlwaysOnTop() const
{
    return _isAlwaysOnTop;
}

void Window::setAlwaysOnTop(bool alwaysOnTop)
{
    _isAlwaysOnTop = alwaysOnTop;
    SDL_SetWindowAlwaysOnTop(_win, _isAlwaysOnTop ? SDL_TRUE : SDL_FALSE);
}

} // namsepace
