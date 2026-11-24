#include "zoomview.h"
#include "cliphelper.h"
#include "touch.h"
#include <algorithm>
#include <cmath>

namespace Ui {

ZoomView::ZoomView(int x, int y, int w, int h)
    : Container(x, y, w, h)
{
    // Replace Container's event forwarding with versions that transform coordinates into content space.
    onMouseDown -= this;
    onClick -= this;
    onMouseMove -= this;
    onMouseEnter -= this;
    onMouseLeave -= this;
    onMouseCancel -= this;
    onScroll -= this;
    connectChildEvents();
}

ZoomView::~ZoomView()
{
    _child = nullptr; // deleted by Container
}

void ZoomView::connectChildEvents()
{
    onMouseDown += {this, [this](void*, int x, int y, int button) {
        if (!_child || !_child->getVisible())
            return;
        int cx, cy;
        toContent(x, y, cx, cy);
        if (_child->isHit(cx, cy)) {
            _pressedChild = _child;
            _child->onMouseDown.emit(_child, cx - _child->getLeft(), cy - _child->getTop(), button);
        }
    }};
    onClick += {this, [this](void*, int x, int y, int button) {
        if (!_child)
            return;
        int cx, cy;
        toContent(x, y, cx, cy);
        if (_child->getVisible() && _child->isHit(cx, cy) && _pressedChild == _child) {
            _pressedChild = nullptr;
            _child->onClick.emit(_child, cx - _child->getLeft(), cy - _child->getTop(), button);
            return;
        }
        if (const auto pressed = _pressedChild) {
            _pressedChild = nullptr;
            pressed->onMouseCancel.emit(pressed);
        }
    }};
    onMouseMove += {this, [this](void*, int x, int y, unsigned buttons) {
        if (!_child)
            return;
        int cx, cy;
        toContent(x, y, cx, cy);
        const int lx = cx - _child->getLeft();
        const int ly = cy - _child->getTop();
        if (_child->getVisible() && _child->isHit(cx, cy)) {
            if (_hoverChild != _child) {
                _hoverChild = _child;
                _child->onMouseEnter.emit(_child, lx, ly, buttons);
            }
            if (_hoverChild == _child) // enter could have changed things
                _child->onMouseMove.emit(_child, lx, ly, buttons);
        } else if (_hoverChild) {
            const auto old = _hoverChild;
            _hoverChild = nullptr;
            old->onMouseLeave.emit(old);
        }
    }};
    onMouseLeave += {this, [this](void*) {
        if (const auto old = _hoverChild) {
            _hoverChild = nullptr;
            old->onMouseLeave.emit(old);
        }
    }};
    onMouseCancel += {this, [this](void*) {
        if (const auto pressed = _pressedChild) {
            _pressedChild = nullptr;
            pressed->onMouseCancel.emit(pressed);
        }
    }};
    onScroll += {this, [this](void*, int x, int y, unsigned mod) {
        if (_hoverChild)
            _hoverChild->onScroll.emit(_hoverChild, x, y, mod);
    }};
}

void ZoomView::setChild(Widget* child)
{
    clearChildren();
    _child = child;
    if (_child) {
        _child->setPosition({0, 0});
        Container::addChild(_child);
    }
    _zoom = 1.0f;
    _panX = _panY = 0.0f;
    relayout();
}

void ZoomView::setSize(Size size)
{
    Container::setSize(size);
    relayout();
}

void ZoomView::relayout()
{
    if (!_child || _inRelayout)
        return;
    if (_size.width < 1 || _size.height < 1)
        return;
    _inRelayout = true;

    const auto& minSize = _child->getMinSize();
    const float minW = (float)std::max(1, minSize.width);
    const float minH = (float)std::max(1, minSize.height);
    float fit = std::min((float)_size.width / minW, (float)_size.height / minH);
    fit = std::max(0.05f, std::min(_maxFit, fit));

    Size content = {
        std::max((int)minW, (int)std::ceil((float)_size.width / fit)),
        std::max((int)minH, (int)std::ceil((float)_size.height / fit)),
    };
    const auto& maxSize = _child->getMaxSize();
    if (maxSize.width > 0 && content.width > maxSize.width)
        content.width = std::max((int)minW, maxSize.width);
    if (maxSize.height > 0 && content.height > maxSize.height)
        content.height = std::max((int)minH, maxSize.height);

    const float oldScale = scale();
    _fit = fit;
    if (content != _child->getSize())
        _child->setSize(content);
    _contentSize = _child->getSize();
    if (oldScale != scale())
        onZoomChanged.emit(this, _zoom);
    clampPan();
    _inRelayout = false;
}

void ZoomView::clampPan()
{
    const float s = scale();
    const float cw = (float)_contentSize.width * s;
    const float ch = (float)_contentSize.height * s;
    const float w = (float)_size.width;
    const float h = (float)_size.height;
    // center content that is smaller than the view, otherwise keep the view inside the content
    if (cw <= w)
        _panX = -(w - cw) / 2.0f;
    else
        _panX = std::max(0.0f, std::min(cw - w, _panX));
    if (ch <= h)
        _panY = -(h - ch) / 2.0f;
    else
        _panY = std::max(0.0f, std::min(ch - h, _panY));
}

void ZoomView::toContent(int x, int y, int& cx, int& cy) const
{
    const float s = scale();
    cx = (int)std::floor(((float)x + _panX) / s);
    cy = (int)std::floor(((float)y + _panY) / s);
}

const Widget* ZoomView::getHit(int x, int y) const
{
    return Widget::isHit(x + _pos.left, y + _pos.top) ? this : nullptr;
}

void ZoomView::render(Renderer renderer, int offX, int offY)
{
    const int vx = offX + _pos.left;
    const int vy = offY + _pos.top;

    if (_backgroundColor.a > 0) {
        const auto& c = _backgroundColor;
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        SDL_Rect r = {vx, vy, _size.width, _size.height};
        SDL_RenderFillRect(renderer, &r);
    }
    if (!_child || !_child->getVisible())
        return;

    float sx0 = 1.0f, sy0 = 1.0f;
    SDL_RenderGetScale(renderer, &sx0, &sy0);
    const float s = scale();

    // Clip to the view in the outer coordinate system. SDL stores the clip rect in output pixels,
    // so it stays put when we change the scale below. The guard restores the previous clip.
    ClipGuard outerClip(renderer, {vx, vy, _size.width, _size.height});

    // switch to content coordinates: 1 content pixel = s outer pixels
    SDL_RenderSetScale(renderer, sx0 * s, sy0 * s);
    const int cox = (int)std::lround(((float)vx - _panX) / s);
    const int coy = (int)std::lround(((float)vy - _panY) / s);
    _child->render(renderer, cox, coy);

    // back to outer coordinates before outerClip restores the clip rect
    SDL_RenderSetScale(renderer, sx0, sy0);
}

bool ZoomView::touchPan(int x, int y, int dx, int dy)
{
    if (!_visible)
        return false;
    const int lx = x - _pos.left;
    const int ly = y - _pos.top;
    if (_child && _child->getVisible()) {
        const float s = scale();
        const float fdx = (float)dx / s + _panRemX;
        const float fdy = (float)dy / s + _panRemY;
        const int cdx = (int)fdx;
        const int cdy = (int)fdy;
        _panRemX = fdx - (float)cdx;
        _panRemY = fdy - (float)cdy;
        int cx, cy;
        toContent(lx, ly, cx, cy);
        if (_child->isHit(cx, cy) && _child->touchPan(cx, cy, cdx, cdy))
            return true;
    }
    _panRemX = _panRemY = 0.0f;
    const float s = scale();
    if ((float)_contentSize.width * s <= (float)_size.width + 0.5f &&
            (float)_contentSize.height * s <= (float)_size.height + 0.5f)
        return false; // everything visible, nothing to pan
    _panX -= (float)dx;
    _panY -= (float)dy;
    clampPan();
    return true;
}

bool ZoomView::touchZoom(int x, int y, float factor)
{
    if (!_visible || factor <= 0.0f)
        return false;
    const float lx = (float)(x - _pos.left);
    const float ly = (float)(y - _pos.top);
    float newZoom = std::max(1.0f, std::min(Touch::MAX_ZOOM, _zoom * factor));
    if (newZoom < 1.01f)
        newZoom = 1.0f;
    if (newZoom == _zoom)
        return true;
    const float oldScale = scale();
    // keep the content point under the fingers in place
    const float cx = (lx + _panX) / oldScale;
    const float cy = (ly + _panY) / oldScale;
    _zoom = newZoom;
    const float newScale = scale();
    _panX = cx * newScale - lx;
    _panY = cy * newScale - ly;
    clampPan();
    onZoomChanged.emit(this, _zoom);
    return true;
}

void ZoomView::resetZoom()
{
    if (_zoom == 1.0f)
        return;
    _zoom = 1.0f;
    clampPan();
    onZoomChanged.emit(this, _zoom);
}

} // namespace Ui
