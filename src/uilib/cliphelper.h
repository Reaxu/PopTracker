#pragma once

#include <SDL2/SDL.h>

namespace Ui {

/// Sets a clip rect for the lifetime of the object, intersected with the clip rect that is already active,
/// and restores the previous clip state on destruction. This allows nesting clipped widgets
/// (e.g. a scroll list inside a zoomed view).
class ClipGuard final {
public:
    ClipGuard(SDL_Renderer* renderer, const SDL_Rect& rect)
        : _renderer(renderer)
    {
        _hadClip = SDL_RenderIsClipEnabled(_renderer) == SDL_TRUE;
        SDL_Rect clip = rect;
        if (_hadClip) {
            SDL_RenderGetClipRect(_renderer, &_oldClip);
            if (!SDL_IntersectRect(&_oldClip, &rect, &clip))
                clip = {rect.x, rect.y, 0, 0}; // nothing visible
        }
        SDL_RenderSetClipRect(_renderer, &clip);
    }

    ~ClipGuard()
    {
        SDL_RenderSetClipRect(_renderer, _hadClip ? &_oldClip : nullptr);
    }

    ClipGuard(const ClipGuard&) = delete;
    ClipGuard& operator=(const ClipGuard&) = delete;

private:
    SDL_Renderer* _renderer;
    SDL_Rect _oldClip = {0, 0, 0, 0};
    bool _hadClip = false;
};

} // namespace Ui
