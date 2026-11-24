#include "mobilemenu.h"
#include "../uilib/touch.h"
#include <algorithm>

namespace Ui {

MobileMenu::MobileMenu(int x, int y, int w, int h, FONT font)
    : Container(x, y, w, h), _font(font)
{
    setBackground(BACKDROP);
    _panel = new ScrollVBox(0, 0, 0, 0);
    _panel->setPadding(0);
    _panel->setSpacing(1);
    _panel->setBackground(PANEL_BG);
    addChild(_panel);

    // tap on the backdrop closes the menu
    onClick += {this, [this](void*, int x, int y, int) {
        if (!_panel->isHit(x, y))
            setVisible(false);
    }};
}

void MobileMenu::setEntries(const std::vector<Entry>& entries)
{
    _hover = nullptr;
    _panel->clearChildren();
    const int rowH = std::max(Touch::rowHeight(), 32);
    for (const auto& entry : entries) {
        auto lbl = new Label(0, 0, 0, rowH, _font, "  " + entry.text);
        lbl->setTextAlignment(Label::HAlign::LEFT, Label::VAlign::MIDDLE);
        lbl->setMinSize({lbl->getAutoWidth() + 16, rowH});
        lbl->setSize({lbl->getAutoWidth() + 16, rowH});
        if (entry.header) {
            lbl->setBackground(HEADER_BG);
            lbl->setTextColor({160, 160, 160});
        } else {
            lbl->setBackground(ROW_BG);
            const std::string id = entry.id;
            const int index = entry.index;
            lbl->onMouseEnter += {this, [this](void* s, int, int, unsigned) {
                if (_hover && _hover != s)
                    _hover->setBackground(ROW_BG);
                _hover = (Label*)s;
                _hover->setBackground(ROW_BG_HOVER);
            }};
            lbl->onMouseLeave += {this, [this](void* s) {
                ((Label*)s)->setBackground(ROW_BG);
                if (_hover == s)
                    _hover = nullptr;
            }};
            lbl->onClick += {this, [this, id, index](void*, int, int, int button) {
                if (button != BUTTON_LEFT)
                    return;
                setVisible(false);
                onSelected.emit(this, id, index); // may modify the menu, so this has to be last
            }};
        }
        _panel->addChild(lbl);
    }
    relayout();
}

void MobileMenu::setSize(Size size)
{
    Container::setSize(size);
    relayout();
}

void MobileMenu::relayout()
{
    // panel on the right edge, as wide as the widest entry (but at least 240 and at most the window)
    int w = 240;
    int contentH = 0;
    for (const auto child : _panel->getChildren()) {
        w = std::max(w, child->getMinWidth());
        contentH += child->getHeight() + 1;
    }
    w = std::min(w, std::max(1, _size.width - 8));
    const int maxH = std::max(1, _size.height - _panelTop - 8);
    const int h = std::min(contentH, maxH);
    _panel->setPosition({_size.width - w, _panelTop});
    _panel->setSize({w, h});
    _panel->relayout();
}

} // namespace Ui
