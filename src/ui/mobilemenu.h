#pragma once

#include <string>
#include <vector>
#include "../uilib/container.h"
#include "../uilib/scrollvbox.h"
#include "../uilib/label.h"
#include "../core/signal.h"

namespace Ui {

/// Full-window overlay with a list of large, touch friendly menu entries.
/// Used in touch mode instead of hotkeys and small icon buttons.
class MobileMenu : public Container {
public:
    using FONT = Label::FONT;

    struct Entry {
        std::string id;      ///< menu id passed to onSelected
        int index = 0;       ///< extra argument, e.g. auto tracker index
        std::string text;
        bool header = false; ///< non-clickable section title
    };

    MobileMenu(int x, int y, int w, int h, FONT font);

    void setEntries(const std::vector<Entry>& entries);
    /// Position of the panel's top edge (below the tool bar)
    void setPanelTop(int top) { _panelTop = top; relayout(); }
    void setSize(Size size) override;

    Signal<const std::string&, int> onSelected;

protected:
    FONT _font;
    ScrollVBox* _panel = nullptr;
    int _panelTop = 0;
    Label* _hover = nullptr;

    void relayout();

    static constexpr Color BACKDROP = {0, 0, 0, 160};
    static constexpr Color PANEL_BG = {40, 40, 40, 255};
    static constexpr Color ROW_BG = {48, 48, 48, 255};
    static constexpr Color ROW_BG_HOVER = {80, 80, 80, 255};
    static constexpr Color HEADER_BG = {24, 24, 24, 255};
};

} // namespace Ui
