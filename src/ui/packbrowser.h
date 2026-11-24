#pragma once

#include <set>
#include <string>
#include <nlohmann/json.hpp>
#include "../uilib/container.h"
#include "../uilib/scrollvbox.h"
#include "../uilib/label.h"
#include "../uilib/button.h"
#include "../uilib/textfield.h"
#include "../core/signal.h"

namespace Ui {

class Window;

/// Full-window list of packs from the community pack repository, to download and install packs
/// without a file manager. Fed by PackManager::getAvailablePacks().
class PackBrowser : public Container {
public:
    using FONT = Label::FONT;

    PackBrowser(int x, int y, int w, int h, FONT font, FONT smallFont, Window* window);

    /// Show "loading" until setPacks is called
    void setLoading();
    /// packs: { uid: { "name": ..., ... }, ... } as returned by PackManager::getAvailablePacks
    void setPacks(const nlohmann::json& packs, const std::set<std::string>& installedUids);
    void setError(const std::string& message);
    void setSize(Size size) override;
    void releaseFocus();

    Signal<const std::string&, const std::string&> onInstall; ///< uid, name
    Signal<> onClose;

protected:
    FONT _font;
    FONT _smallFont;
    Window* _window;
    Label* _title = nullptr;
    Button* _btnClose = nullptr;
    TextField* _filter = nullptr;
    Label* _status = nullptr;
    ScrollVBox* _list = nullptr;
    nlohmann::json _packs;
    std::set<std::string> _installed;
    Label* _hover = nullptr;

    void refresh();
    void relayout();

    static constexpr Color BG = {16, 16, 16, 255};
    static constexpr Color ROW_BG = {40, 40, 40, 255};
    static constexpr Color ROW_BG_HOVER = {72, 72, 72, 255};
    static constexpr Color ROW_BG_INSTALLED = {32, 64, 32, 255};
};

} // namespace Ui
