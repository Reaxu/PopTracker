#ifndef _UI_DEFAULTTRACKERWINDOW_H
#define _UI_DEFAULTTRACKERWINDOW_H

#include "trackerwindow.h"
#include "loadpackwidget.h"
#include "mobilemenu.h"
#include "packbrowser.h"
#include "../uilib/progressbar.h"
#include "../uilib/button.h"
#include "../uilib/zoomview.h"
#include <set>
#include <vector>

namespace Ui {

class DefaultTrackerWindow : public TrackerWindow {
public:
    using FONT = Window::FONT;
    
    DefaultTrackerWindow(const char *title, SDL_Surface* icon=nullptr, const Position& pos=WINDOW_DEFAULT_POSITION, const Size& size={0,0}, const WindowConfig& config={});
    virtual ~DefaultTrackerWindow();

    virtual void render(Renderer renderer, int offX, int offY) override;
    virtual void setTracker(Tracker* tracker) override;
    virtual void setAutoTrackerState(int index, AutoTracker::State state, const std::string& name, const std::string& subname) override;
    virtual void setSize(Size size) override;
    void setMinSize(Size size) override;
    virtual void showOpen();
    virtual void hideOpen();
    virtual void showProgress(const std::string& title, int progress, int max);
    virtual void hideProgress();
    virtual void setAlwaysOnTop(bool alwaysOnTop) override;

    // touch mode
    bool handleBack() override;
    void showMenu();
    void showPackBrowser();
    void setPackBrowserPacks(const nlohmann::json& packs, const std::set<std::string>& installedUids);
    void setPackBrowserError(const std::string& message);
    void hidePackBrowser();
    /// show the pack's settings_popup layout inside the main window (no extra windows on Android)
    void showPackSettings(Tracker* tracker);
    /// deferred=true when called from inside the overlay's own event handlers
    void hidePackSettings(bool deferred = false);
    bool isPackSettingsVisible() const { return _settingsOverlay && _settingsOverlay->getVisible(); }
    /// state for the menu's "hide cleared locations" entry
    void setHideLocationsMenuState(bool hidden) { _menuLocationsHidden = hidden; }

    Signal<const fs::path&, const std::string&> onPackSelected;
    Signal<const std::string&, const std::string&> onInstallPack; ///< uid, name; from the pack browser
    
protected:
    ImageButton *_btnLoad = nullptr;
    ImageButton *_btnReload = nullptr;
    ImageButton *_btnImport = nullptr;
    ImageButton *_btnExport = nullptr;
    ImageButton *_btnBroadcast = nullptr;
    ImageButton *_btnAlwaysOnTop = nullptr;
    ImageButton *_btnPackSettings = nullptr;
    HBox *_hboxAutoTrackers = nullptr;
    std::vector<Label*> _lblsAutoTrackers;
    Label *_lblTooltip = nullptr;
    Label *_lblMessage = nullptr;
    LoadPackWidget *_loadPackWidget = nullptr;
    VBox *_vboxProgress = nullptr;
    HBox *_hboxProgressTexts = nullptr;
    Label *_lblProgressTitle = nullptr;
    Label *_lblProgressPercent = nullptr;
    Label *_lblProgressValues = nullptr;
    ProgressBar *_pgbProgress = nullptr;
    std::vector<AutoTracker::State> _autoTrackerStates;
    std::vector<std::string> _autoTrackerNames;
    std::vector<std::string> _autoTrackerSubNames;
    float _aspectRatio = 1;
    std::string _lastHoverItem;

    // touch mode
    ImageButton *_btnMenu = nullptr;
    Button *_btnFit = nullptr;
    MobileMenu *_mobileMenu = nullptr;
    PackBrowser *_packBrowser = nullptr;
    Container *_settingsOverlay = nullptr;
    ZoomView *_settingsZoom = nullptr;
    TrackerView *_settingsView = nullptr;
    bool _menuLocationsHidden = false;
    std::vector<Widget*> _pendingDelete;
    Tracker *_tracker = nullptr;

    void layoutOverlays(Size size);
    void updateFitButton();

    virtual void setTracker(Tracker *tracker, const std::string& layout) override;

private:
    void showMessage(const std::string& message, bool error=false);
    void hideMessage();
};

} // namespace Ui

#endif /* _UI_DEFAULTTRACKERWINDOW_H */

