#include "packbrowser.h"
#include "../uilib/touch.h"
#include "../uilib/window.h"
#include <algorithm>
#include <cctype>
#include <vector>

using nlohmann::json;

namespace Ui {

static std::string lower(std::string s)
{
    for (auto& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

PackBrowser::PackBrowser(int x, int y, int w, int h, FONT font, FONT smallFont, Window* window)
    : Container(x, y, w, h), _font(font), _smallFont(smallFont ? smallFont : font), _window(window)
{
    setBackground(BG);
    const int rowH = std::max(Touch::rowHeight(), 32);

    _btnClose = new Button(0, 0, 0, 0, _font, "  Close  ");
    _btnClose->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::MIDDLE);
    addChild(_btnClose);
    _btnClose->onClick += {this, [this](void*, int, int, int button) {
        if (button != BUTTON_LEFT)
            return;
        releaseFocus();
        onClose.emit(this);
    }};

    _title = new Label(0, 0, 0, rowH, _font, "Get packs");
    _title->setTextAlignment(Label::HAlign::LEFT, Label::VAlign::MIDDLE);
    addChild(_title);

    _filter = new TextField(0, 0, 0, rowH - 8, _font, _window);
    _filter->setPlaceholder("Search packs...");
    _filter->setBackground({40, 40, 40});
    addChild(_filter);
    _filter->onTextChanged += {this, [this](void*, const std::string&) {
        refresh();
    }};

    _status = new Label(0, 0, 0, 0, _smallFont, "");
    _status->setTextAlignment(Label::HAlign::LEFT, Label::VAlign::MIDDLE);
    _status->setTextColor({180, 180, 180});
    addChild(_status);

    _list = new ScrollVBox(0, 0, 0, 0);
    _list->setPadding(0);
    _list->setSpacing(1);
    addChild(_list);

    relayout();
}

void PackBrowser::releaseFocus()
{
    if (_filter)
        _filter->releaseFocus();
}

void PackBrowser::setLoading()
{
    _packs = json::object();
    _list->clearChildren();
    _hover = nullptr;
    _status->setText("Loading pack list ...");
    relayout();
}

void PackBrowser::setError(const std::string& message)
{
    _list->clearChildren();
    _hover = nullptr;
    _status->setText(message);
    relayout();
}

void PackBrowser::setPacks(const json& packs, const std::set<std::string>& installedUids)
{
    _packs = packs.is_object() ? packs : json::object();
    _installed = installedUids;
    refresh();
}

void PackBrowser::refresh()
{
    _list->clearChildren();
    _hover = nullptr;

    struct Row {
        std::string uid;
        std::string name;
        std::string game;
    };
    std::vector<Row> rows;
    const std::string filter = lower(_filter ? _filter->getText() : "");
    for (const auto& pair : _packs.items()) {
        const auto& v = pair.value();
        Row row;
        row.uid = pair.key();
        row.name = v.is_object() && v["name"].is_string() ? v["name"].get<std::string>() : row.uid;
        if (v.is_object() && v["game_name"].is_string())
            row.game = v["game_name"].get<std::string>();
        if (!filter.empty() && lower(row.name).find(filter) == std::string::npos &&
                lower(row.uid).find(filter) == std::string::npos && lower(row.game).find(filter) == std::string::npos)
            continue;
        rows.push_back(std::move(row));
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return lower(a.name) < lower(b.name);
    });

    if (_packs.empty() && _status->getText().empty())
        _status->setText("No packs available.");
    else if (!_packs.empty())
        _status->setText(std::to_string(rows.size()) + " packs - tap one to download and install it");

    const int rowH = std::max(Touch::rowHeight() + 8, 40);
    for (const auto& row : rows) {
        const bool installed = _installed.count(row.uid) > 0;
        std::string text = "  " + row.name;
        if (installed)
            text += "  (installed)";
        text += "\n  " + row.uid;
        auto lbl = new Label(0, 0, 0, 0, _font, text);
        lbl->setTextAlignment(Label::HAlign::LEFT, Label::VAlign::MIDDLE);
        const int h = std::max(rowH, lbl->getAutoHeight() + 8);
        lbl->setMinSize({0, h});
        lbl->setSize({_list->getWidth(), h});
        const Color bg = installed ? ROW_BG_INSTALLED : ROW_BG;
        lbl->setBackground(bg);
        lbl->onMouseEnter += {this, [this](void* s, int, int, unsigned) {
            _hover = (Label*)s;
            _hover->setBackground(ROW_BG_HOVER);
        }};
        lbl->onMouseLeave += {this, [this, bg](void* s) {
            ((Label*)s)->setBackground(bg);
            if (_hover == s)
                _hover = nullptr;
        }};
        const std::string uid = row.uid;
        const std::string name = row.name;
        lbl->onClick += {this, [this, uid, name](void*, int, int, int button) {
            if (button != BUTTON_LEFT)
                return;
            releaseFocus();
            onInstall.emit(this, uid, name);
        }};
        _list->addChild(lbl);
    }
    relayout();
}

void PackBrowser::setSize(Size size)
{
    Container::setSize(size);
    relayout();
}

void PackBrowser::relayout()
{
    const int pad = 4;
    const int rowH = std::max(Touch::rowHeight(), 32);
    const int w = _size.width;

    _btnClose->setPosition({pad, pad});
    _btnClose->setSize({std::max(_btnClose->getAutoWidth(), 72), rowH - 2 * pad});
    _title->setPosition({_btnClose->getLeft() + _btnClose->getWidth() + 8, 0});
    _title->setSize({std::max(0, w - _title->getLeft()), rowH});

    int y = rowH;
    _filter->setPosition({pad, y});
    _filter->setSize({std::max(1, w - 2 * pad), rowH - 8});
    y += rowH - 8 + pad;

    _status->setPosition({pad, y});
    _status->setSize({std::max(1, w - 2 * pad), std::max(_status->getAutoHeight(), 16) + 4});
    y += _status->getHeight() + pad;

    _list->setPosition({0, y});
    _list->setSize({w, std::max(1, _size.height - y)});
    _list->relayout();
}

} // namespace Ui
