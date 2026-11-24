#pragma once

#include "container.h"
#include "../core/signal.h"

namespace Ui {

/// Shows a single child scaled to fit the view, with pinch-zoom and pan on top.
///
/// The child gets laid out at a "content size" that is at least its minimum size and has the view's aspect ratio,
/// then gets drawn scaled by fit * zoom. fit is chosen so the whole content is visible at zoom 1.
/// This lets fixed-size desktop tracker layouts work on a small phone screen, and lets layouts with grow
/// (maps) use all available space.
class ZoomView : public Container {
public:
    ZoomView(int x, int y, int w, int h);
    ~ZoomView() override;

    /// Set the content widget. Takes ownership; deletes the previous child.
    void setChild(Widget* child);
    Widget* getChild() const { return _child; }

    void setSize(Size size) override;
    void render(Renderer renderer, int offX, int offY) override;

    bool isHit(int x, int y) const override { return Widget::isHit(x, y); }
    const Widget* getHit(int x, int y) const override;

    bool touchPan(int x, int y, int dx, int dy) override;
    bool touchZoom(int x, int y, float factor) override;

    /// Recalculate fit scale and content size, e.g. after the child's min size changed.
    void relayout();

    /// Go back to zoom 1 (everything visible)
    void resetZoom();
    float getZoom() const { return _zoom; }
    bool isZoomed() const { return _zoom > 1.001f; }

    /// Upper bound for scaling up small layouts on big screens
    void setMaxFit(float maxFit) { _maxFit = maxFit; relayout(); }

    /// Convert local (view) coordinates to child coordinates
    void toContent(int x, int y, int& cx, int& cy) const;

    Signal<float> onZoomChanged;

protected:
    Widget* _child = nullptr;
    float _fit = 1.0f;
    float _maxFit = 1.5f;
    float _zoom = 1.0f;
    float _panX = 0.0f; ///< in view pixels
    float _panY = 0.0f;
    float _panRemX = 0.0f; ///< fractional content-delta not yet forwarded to the child
    float _panRemY = 0.0f;
    Size _contentSize = {0, 0};
    bool _inRelayout = false;

    float scale() const { return _fit * _zoom; }
    void clampPan();
    void connectChildEvents();
};

} // namespace Ui
