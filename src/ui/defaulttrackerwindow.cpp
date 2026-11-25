#include "defaulttrackerwindow.h"
#include "../uilib/dlg.h"
#include "../uilib/hbox.h"
#include "../core/assets.h"
#include "../uilib/touch.h"
#include "defaults.h"

namespace Ui {

namespace {
/// plain container for overlays
class OverlayContainer final : public Container {
public:
    OverlayContainer(int x, int y, int w, int h) : Container(x, y, w, h) {}
};
}

DefaultTrackerWindow::DefaultTrackerWindow(const char* title, SDL_Surface* icon, const Position& pos, const Size& size, const WindowConfig& config)
    : TrackerWindow(title, icon, pos, size, config)
{
    const bool touch = Touch::enabled();
    const int bar = Touch::barHeight(); // 32 on desktop, larger touch targets in touch mode
    const int btn = bar - 4;

    auto hbox = new HBox(0,0,_size.width,bar);
    hbox->setBackground({0,0,0,255});
    hbox->setPadding(2);
    hbox->setSpacing(touch ? 4 : 2);
    hbox->setGrow(1,1);
    _menu = hbox;
    addChild(_menu);
    
    _btnLoad = new ImageButton(0,0,btn,btn, asset("load.png"));
    _btnLoad->setDarkenGreyscale(false);
    hbox->addChild(_btnLoad);
    _btnLoad->onClick += { this, [this](void*, int, int, int) {
        onMenuPressed.emit(this, MENU_LOAD, 0);
    }};

    // in touch mode, reload/import/export live in the menu to save space
    if (!touch) {
        _btnReload = new ImageButton(0,0,btn,btn, asset("reload.png"));
        _btnReload->setVisible(false);
        hbox->addChild(_btnReload);
        _btnReload->onClick += { this, [this](void*, int, int, int) {
            onMenuPressed.emit(this, MENU_RELOAD, 0);
        }};

        _btnImport = new ImageButton(0,0,btn,btn, asset("import.png"));
        _btnImport->setVisible(false);
        hbox->addChild(_btnImport);
        _btnImport->onClick += { this, [this](void*, int, int, int) {
            onMenuPressed.emit(this, MENU_LOAD_STATE, 0);
        }};

        _btnExport = new ImageButton(0,0,btn,btn, asset("export.png"));
        _btnExport->setVisible(false);
        hbox->addChild(_btnExport);
        _btnExport->onClick += { this, [this](void*, int, int, int) {
            onMenuPressed.emit(this, MENU_SAVE_STATE, 0);
        }};
    }

#ifndef __EMSCRIPTEN__ // no multi-window support (yet)
    if (!Touch::singleWindow()) {
        _btnBroadcast = new ImageButton(0,0,btn,btn, asset("broadcast.png"));
        _btnBroadcast->setVisible(false);
        hbox->addChild(_btnBroadcast);
        _btnBroadcast->onClick += { this, [this](void*, int, int, int) {
            onMenuPressed.emit(this, MENU_BROADCAST, 0);
        }};
    }
#endif

    if (config.showAlwaysOnTopButton() && !Touch::singleWindow()) {
        _btnAlwaysOnTop = new ImageButton(0,0,btn,btn, asset("always_on_top.png"));
        hbox->addChild(_btnAlwaysOnTop);
        _btnAlwaysOnTop->onClick += { this, [this](void *s, int, int, int) {
            ImageButton* btn = (ImageButton*)s;
            btn->setEnabled(!btn->getEnabled()); // toggle greyscale
            onMenuPressed.emit(this, MENU_ALWAYS_ON_TOP, 0);
        }};
    }
    
    _btnPackSettings = new ImageButton(btn,0,btn,btn, asset("settings.png"));
    _btnPackSettings->setVisible(false);
    hbox->addChild(_btnPackSettings);
    _btnPackSettings->onClick += { this, [this](void*, int, int, int) {
        onMenuPressed.emit(this, MENU_PACK_SETTINGS, 0);
    }};

    _hboxAutoTrackers = new HBox(0,0,0,btn);
    _hboxAutoTrackers->setPadding(4);
    _hboxAutoTrackers->setSpacing(touch ? 10 : 6);
    hbox->addChild(_hboxAutoTrackers);

    _lblTooltip = new Label(0,0,0,0,_font,"");
    _lblTooltip->setHeight(btn);
    _lblTooltip->setGrow(1,1);
    _lblTooltip->setTextAlignment(Label::HAlign::RIGHT, Label::VAlign::MIDDLE);
    hbox->addChild(_lblTooltip);

    if (touch) {
        // "Items" / "Maps" page switch, filled in by updateSplitButtons()
        _splitBox = new HBox(0,0,0,btn);
        _splitBox->setPadding(0);
        _splitBox->setSpacing(2);
        _splitBox->setGrow(0,0);
        hbox->addChild(_splitBox);

        // floating "Fit" button, shows up while the layout is zoomed in (see layoutOverlays)
        _btnFit = new Button(0,0,0,btn, _font, "  Fit to screen  ");
        _btnFit->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::MIDDLE);
        _btnFit->setBackground({0x30, 0x30, 0x30, 0xe0});
        _btnFit->setSize({_btnFit->getAutoWidth() + 8, btn});
        _btnFit->setVisible(false);
        addChild(_btnFit);
        _btnFit->onClick += { this, [this](void*, int, int, int) {
            resetZoom();
        }};

        _btnMenu = new ImageButton(0,0,btn,btn, asset("menu.png"));
        _btnMenu->setDarkenGreyscale(false);
        hbox->addChild(_btnMenu);
        _btnMenu->onClick += { this, [this](void*, int, int, int) {
            showMenu();
        }};
    }

    _lblMessage = new Label(0,bar+4,_size.width,0, _font, "");
    _lblMessage->setVisible(false);
    _lblMessage->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::TOP);
    addChild(_lblMessage);

    if (!Dlg::hasGUI()) {
        showMessage("WARNING: Dialog helper not available. Certain things will be broken!\n"
                "Please install which and zenity, kdialog, matedialog, qarma, xdialog or python + tkinter.",
                true);
    }

    _loadPackWidget = new LoadPackWidget(0,0,0,0,_fontStore, this);
    _loadPackWidget->setPosition({0,0});
    _loadPackWidget->setSize(_size);
    _loadPackWidget->setBackground({0,0,0});
    _loadPackWidget->setVisible(false);
    addChild(_loadPackWidget);
    
    _loadPackWidget->onPackSelected += {this,[this](void *, const fs::path& pack, const std::string& variant) {
        onPackSelected.emit(this,pack,variant);
    }};
    _loadPackWidget->onGetPacks += {this, [this](void*) {
        onMenuPressed.emit(this, MENU_GET_PACKS, 0);
    }};
    _loadPackWidget->onImportPack += {this, [this](void*) {
        onMenuPressed.emit(this, MENU_IMPORT_PACK, 0);
    }};
    _loadPackWidget->onClose += {this, [this](void*) {
        hideOpen();
    }};

    if (touch) {
        auto menuFont = _fontStore->getFont(DEFAULT_FONT_NAME, DEFAULT_FONT_SIZE + 2);
        if (!menuFont) menuFont = _font;
        _mobileMenu = new MobileMenu(0, 0, _size.width, _size.height, menuFont);
        _mobileMenu->setPanelTop(bar);
        _mobileMenu->setVisible(false);
        addChild(_mobileMenu);
        _mobileMenu->onSelected += {this, [this](void*, const std::string& id, int index) {
            onMenuPressed.emit(this, id, index);
        }};

        _packBrowser = new PackBrowser(0, 0, _size.width, _size.height, menuFont, _font, this);
        _packBrowser->setVisible(false);
        addChild(_packBrowser);
        _packBrowser->onClose += {this, [this](void*) {
            hidePackBrowser();
        }};
        _packBrowser->onInstall += {this, [this](void*, const std::string& uid, const std::string& name) {
            onInstallPack.emit(this, uid, name);
        }};
    }
    
    // fixed size icons; HBox may shrink children to their min size when distributing space
    for (auto b : {_btnLoad, _btnReload, _btnImport, _btnExport, _btnBroadcast, _btnAlwaysOnTop, _btnPackSettings, _btnMenu}) {
        if (b) b->setMinSize({btn, btn});
    }
    hbox->relayout();

    for (const auto& pair : {
            std::pair{_btnLoad, "Load Pack"},
            std::pair{_btnReload, "Reload Pack"},
            std::pair{_btnImport, "Import State"},
            std::pair{_btnExport, "Export State"},
            std::pair{_btnBroadcast, "Open Broadcast View"},
            std::pair{_btnPackSettings, "Open Pack Settings"}
    }) {
        if (!pair.first) continue;
        pair.first->setDarkenGreyscale(false);
        const char* text = pair.second; // pointer to program memory
        if (touch) {
            // no hover on touch screens: always show in color
            pair.first->setEnabled(true);
            continue;
        }
        pair.first->setEnabled(false); // make greyscale
        pair.first->onMouseEnter += {this, [this,text](void *s, int, int, unsigned)
        {
            ImageButton* btn = (ImageButton*)s;
            btn->setEnabled(true); // disable greyscale
            _lblTooltip->setText(text);
        }};
        pair.first->onMouseLeave += {this, [this](void *s)
        {
            ImageButton* btn = (ImageButton*)s;
            btn->setEnabled(false); // enable greyscale
            _lblTooltip->setText("");
        }};
    }

    if (_btnAlwaysOnTop) {
        _btnAlwaysOnTop->setDarkenGreyscale(false);
        _btnAlwaysOnTop->onMouseEnter += {this, [this](void*, int, int, unsigned)
        {
            _lblTooltip->setText("Toggle Always On Top");
        }};
        _btnAlwaysOnTop->onMouseLeave += {this, [this](void*)
        {
            _lblTooltip->setText("");
        }};
    }
}

DefaultTrackerWindow::~DefaultTrackerWindow()
{
    // TODO: make sure widgets are deleted in container's destructor or delete here
    _btnLoad = nullptr;
    _btnReload = nullptr;
    _btnBroadcast = nullptr;
    _btnAlwaysOnTop = nullptr;
    _btnPackSettings = nullptr;
    _hboxAutoTrackers = nullptr;
    _lblsAutoTrackers.clear();
    _vboxProgress = nullptr;
    _lblProgressTitle = nullptr;
    _lblProgressPercent = nullptr;
    _lblProgressValues = nullptr;
    _pgbProgress = nullptr;
}

void DefaultTrackerWindow::setTracker(Tracker* tracker)
{
    std::string layout = _aspectRatio > 1 ? "tracker_horizontal" : _aspectRatio < 1 ? "tracker_vertical" : "tracker_default";
    if (tracker && !tracker->hasLayout(layout)) {
        if (tracker->hasLayout("tracker_default")) layout = "tracker_default";
        else if (tracker->hasLayout("tracker_horizontal")) layout = "tracker_horizontal";
        else if (tracker->hasLayout("tracker_vertical")) layout = "tracker_vertical";
    }
    setTracker(tracker, layout);
}

void DefaultTrackerWindow::setTracker(Tracker* tracker, const std::string& layout)
{
    if (_view && _view->getTracker() == tracker && _view->getLayoutRoot() == layout)
        return;

    if (_tracker != tracker) {
        // settings view references the old tracker: delete now
        hidePackSettings();
        for (auto w : _pendingDelete) {
            removeChild(w);
            delete w;
        }
        _pendingDelete.clear();
    }
    _tracker = tracker;
    TrackerWindow::setTracker(tracker, layout);
    if (_zoomView) {
        _zoomView->onZoomChanged += {this, [this](void*, float) {
            updateFitButton();
        }};
    }
    updateFitButton();
    if (_view && Touch::enabled()) {
        _view->onSplitChanged += {this, [this](void*) {
            updateSplitButtons();
        }};
        _view->setSplitMode(_splitMode, _splitPage);
    }
    updateSplitButtons();
    if (tracker) {
        hideMessage();
        tracker->onLayoutChanged -= this;
        tracker->onLayoutChanged += {this, [this,tracker](void*, const std::string&) {
            if (_btnBroadcast) _btnBroadcast->setVisible(tracker->hasLayout("tracker_broadcast"));
            if (_btnPackSettings) _btnPackSettings->setVisible(tracker->hasLayout("settings_popup"));
            setTracker(tracker); // reevaluate preferred and fall-back layout
        }};
        _view->onItemHover += {this, [this, tracker](void*, const std::string& itemid) {
            if (!_lastHoverItem.empty()) {
                auto& item = tracker->getItemById(itemid);
                item.onChange -= this;
            }
            if (itemid.empty()) _lblTooltip->setText("");
            else {
                auto& item = tracker->getItemById(itemid);
                const auto& text = (item.getBaseItem().empty() || item.getState()) ?
                    item.getCurrentName() : tracker->getItemByCode(item.getBaseItem()).getCurrentName();
                _lblTooltip->setText(text);
                item.onChange += {this, [this, tracker, itemid](void*) {
                    const auto& item = tracker->getItemById(itemid);
                    const auto& text = (item.getBaseItem().empty() || item.getState()) ?
                        item.getCurrentName() : tracker->getItemByCode(item.getBaseItem()).getCurrentName();
                    _lblTooltip->setText(text);
                }};
            }
            _lastHoverItem = itemid;
        }};
        if (_btnReload) _btnReload->setVisible(true);
        if (_btnImport) _btnImport->setVisible(true);
        if (_btnExport) _btnExport->setVisible(true);
    } else {
        if (_btnBroadcast) _btnBroadcast->setVisible(false);
        if (_btnPackSettings) _btnPackSettings->setVisible(false);
        if (_btnReload) _btnReload->setVisible(false);
        if (_btnImport) _btnImport->setVisible(false);
        if (_btnExport) _btnExport->setVisible(false);
    }
    raiseChild(_loadPackWidget);
    if (_settingsOverlay) raiseChild(_settingsOverlay);
    if (_packBrowser) raiseChild(_packBrowser);
    if (_mobileMenu) raiseChild(_mobileMenu);
}

void DefaultTrackerWindow::showMessage(const std::string& message, bool error)
{
    _lblMessage->setText(message);
    _lblMessage->setHeight(_lblMessage->getAutoHeight());
    if (error)
        _lblMessage->setTextColor({0xff, 0x7f, 0x7f, 0xff});
    else
        _lblMessage->setTextColor({0xff, 0xff, 0xff, 0xff});
    _lblMessage->setVisible(true);
}

void DefaultTrackerWindow::hideMessage()
{
    _lblMessage->setVisible(false);
}

void DefaultTrackerWindow::render(Renderer renderer, int offX, int offY)
{
    if (!_pendingDelete.empty()) {
        auto pending = std::move(_pendingDelete);
        _pendingDelete.clear();
        for (auto w : pending) {
            removeChild(w);
            delete w;
        }
    }
    if (_view) {
        float oldAspectRatio = _aspectRatio;
        _aspectRatio = (float)getWidth() / (float)getHeight();
        Tracker* tracker = _view->getTracker();
        if (tracker && ((_aspectRatio > 1 && oldAspectRatio <= 1) || (_aspectRatio < 1 && oldAspectRatio >= 1)))
            setTracker(tracker); // reevaluate preferred and fall-back layout
    }
    TrackerWindow::render(renderer, offX, offY);
}

void DefaultTrackerWindow::setAutoTrackerState(int index, AutoTracker::State state, const std::string& name, const std::string& subname)
{
    if (index<0) {
        // clear all labels
        for (size_t i=0; i<_lblsAutoTrackers.size(); i++) {
            _autoTrackerStates[i] = state;
            auto lbl = _lblsAutoTrackers[i];
            lbl->setText(name);
            lbl->setWidth(lbl->getAutoWidth());
        }
        if (!_lblsAutoTrackers.empty()) {
            _hboxAutoTrackers->relayout();
            auto menu = dynamic_cast<HBox*>(_menu);
            if (menu) menu->relayout();
        }
        return;
    }

    Label* lbl = nullptr;
    if (_lblsAutoTrackers.size() > (size_t)index) {
        lbl = _lblsAutoTrackers[index];
        auto oldWidth = lbl->getWidth();
        lbl->setText(name);
        lbl->setWidth(lbl->getAutoWidth());
        if (lbl->getWidth() != oldWidth) {
            _hboxAutoTrackers->relayout();
            auto menu = dynamic_cast<HBox*>(_menu);
            if (menu) menu->relayout();
        }
        _autoTrackerStates[index] = state;
        _autoTrackerNames[index] = name;
        _autoTrackerSubNames[index] = subname;
    } else {
        printf("adding label #%d \"%s\"\n", index, name.c_str());
        lbl = new Label(0,0,0,Touch::barHeight()-4,_font, name);
        lbl->setWidth(lbl->getAutoWidth());
        lbl->setTextColor({0,0,0});
        lbl->onClick += {this, [this,index](void*, int, int, int button) {
            if (button == BUTTON_RIGHT) {
                onMenuPressed.emit(this, MENU_CYCLE_AUTOTRACKER, index);
            } else {
                onMenuPressed.emit(this, MENU_TOGGLE_AUTOTRACKER, index);
            }
        }};
        lbl->onMouseEnter += {this, [this,index](void*, int, int, unsigned)
        {
            auto state = _autoTrackerStates[index];
            const char* text = state == AutoTracker::State::Disconnected ? "Offline" :
                               state == AutoTracker::State::BridgeConnected ? "No Game/Console" :
                               state == AutoTracker::State::ConsoleConnected ? "Online" :
                               state == AutoTracker::State::Disabled ? "Disabled" :
                               state == AutoTracker::State::Unavailable ? nullptr : "Unknown";
            std::string name = _autoTrackerNames[index];
            if (name.empty()) name = "Auto Tracker";
            if (state == AutoTracker::State::ConsoleConnected) {
                if (!_autoTrackerSubNames[index].empty())
                    name = _autoTrackerSubNames[index];
            }
            if (text) {
                _lblTooltip->setText(name + ": " + text);
            }
        }};
        lbl->onMouseLeave += {this, [this](void*)
        {
            _lblTooltip->setText("");
        }};
        _lblsAutoTrackers.push_back(lbl);
        _autoTrackerStates.push_back(state);
        _autoTrackerNames.push_back(name);
        _autoTrackerSubNames.push_back(subname);
        _hboxAutoTrackers->addChild(lbl);
        _hboxAutoTrackers->relayout();
        auto menu = dynamic_cast<HBox*>(_menu);
        if (menu) menu->relayout();
    }

    lbl->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::MIDDLE);
    if (state == AutoTracker::State::Disconnected) {
        lbl->setTextColor({255,0,0});
    } else if (state == AutoTracker::State::BridgeConnected) {
        lbl->setTextColor({255,255,0});
    } else if (state == AutoTracker::State::ConsoleConnected) {
        lbl->setTextColor({0,255,0});
    } else if (state == AutoTracker::State::Disabled) {
        lbl->setTextColor({128,128,128});
    } else if (state == AutoTracker::State::Unavailable) {
        lbl->setTextColor({0,0,0}); // hidden
    }

    if (isHover(lbl)) {
        // update tool tip if current tool tip is AT state
        lbl->onMouseEnter.emit(lbl, 0, 0, 0);
    }
}

void DefaultTrackerWindow::setSize(Size size)
{
    TrackerWindow::setSize(size);
    _lblMessage->setWidth(size.width);
    _loadPackWidget->setSize(size);
    layoutOverlays(size);
    if (_vboxProgress) {
        _vboxProgress->setPosition({
            (_size.width - _vboxProgress->getWidth())/2,
            (_size.height - _vboxProgress->getHeight())/2
        });
    }
}

void DefaultTrackerWindow::setMinSize(const Size size)
{
    TrackerWindow::setMinSize(size || Size{220, 0});
}

void DefaultTrackerWindow::showOpen()
{
    _loadPackWidget->update();
    _loadPackWidget->setVisible(true);
    raiseChild(_loadPackWidget);
    if (!Touch::enabled()) // don't pop up the on-screen keyboard
        _loadPackWidget->focusFilter();
}
void DefaultTrackerWindow::hideOpen()
{
    _loadPackWidget->releaseFilterFocus();
    _loadPackWidget->setVisible(false);
}

void DefaultTrackerWindow::showProgress(const std::string& text, int progress, int max)
{
    char percentText[12];
    if (max > 0)
        snprintf(percentText, 12, "%3u%%", upercent(progress, max)); // TODO: fmt
    else
        percentText[0] = 0;
    std::string valuesText = format_bytes(progress) + "B";
    if (max) valuesText += " / " + format_bytes(max) + "B";

    if (!_vboxProgress) {
        int w = 300;
        int h = 69; // FIXME: should be able to auto-size this
        int x = (_size.width-w)/2;
        _vboxProgress = new VBox(x, 0, w, h);
        _vboxProgress->setPadding(5);
        _vboxProgress->setSpacing(5);
        _vboxProgress->setBackground({64,64,64});
        _lblProgressTitle = new Label(0,0,0,0, _font, text);
        _lblProgressTitle->setSize(_lblProgressTitle->getAutoSize());
        _lblProgressTitle->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::TOP);
        _vboxProgress->addChild(_lblProgressTitle);
        _pgbProgress = new ProgressBar(1, 0, w-2, 6, max, progress);
        _vboxProgress->addChild(_pgbProgress);
        _hboxProgressTexts = new HBox(0,0,w,0);
        _vboxProgress->addChild(_hboxProgressTexts);
        _lblProgressPercent = new Label(0,0,w/2-5,0, _font, percentText);
        _lblProgressPercent->setTextAlignment(Label::HAlign::LEFT, Label::VAlign::TOP);
        _lblProgressPercent->setHeight(_lblProgressPercent->getAutoHeight());
        _hboxProgressTexts->addChild(_lblProgressPercent);
        _lblProgressValues = new Label(0,0,w/2-5,0, _font, valuesText);
        _lblProgressValues->setTextAlignment(Label::HAlign::RIGHT, Label::VAlign::TOP);
        _lblProgressValues->setHeight(_lblProgressValues->getAutoHeight());
        _hboxProgressTexts->setHeight(_lblProgressValues->getHeight());
        _hboxProgressTexts->addChild(_lblProgressValues);
        addChild(_vboxProgress);
        _vboxProgress->setHeight(h);
        _vboxProgress->setTop((_size.height-_vboxProgress->getHeight())/2);
    } else {
        _lblProgressTitle->setText(text);
        _pgbProgress->setProgress(progress);
        _pgbProgress->setMax(max);
        _lblProgressPercent->setText(percentText);
        _lblProgressValues->setText(valuesText);
        _vboxProgress->setVisible(true);
        raiseChild(_vboxProgress);
    }
}

void DefaultTrackerWindow::hideProgress()
{
    _vboxProgress->setVisible(false);
}

void DefaultTrackerWindow::setAlwaysOnTop(bool alwaysOnTop)
{
    // Color / greyscale represents "always on top" enabled / disabled
    if (_btnAlwaysOnTop) {
        _btnAlwaysOnTop->setEnabled(alwaysOnTop);
    }
    TrackerWindow::setAlwaysOnTop(alwaysOnTop);
}

void DefaultTrackerWindow::updateFitButton()
{
    if (!_btnFit)
        return;
    const bool zoomed = _zoomView && _zoomView->isZoomed();
    if (_btnFit->getVisible() != zoomed)
        _btnFit->setVisible(zoomed);
    if (zoomed) {
        raiseChild(_btnFit);
        layoutOverlays(_size);
    }
}

void DefaultTrackerWindow::layoutOverlays(Size size)
{
    const int bar = Touch::barHeight();
    if (_btnFit) {
        _btnFit->setPosition({size.width - _btnFit->getWidth() - 12, size.height - _btnFit->getHeight() - 12});
    }
    if (_mobileMenu) {
        _mobileMenu->setPosition({0, 0});
        _mobileMenu->setPanelTop(bar);
        _mobileMenu->setSize(size);
    }
    if (_packBrowser) {
        _packBrowser->setPosition({0, 0});
        _packBrowser->setSize(size);
    }
    if (_settingsOverlay) {
        _settingsOverlay->setPosition({0, 0});
        _settingsOverlay->setSize(size);
        // children: [0] header bar, [1] zoom view
        const auto& children = _settingsOverlay->getChildren();
        if (children.size() >= 2) {
            auto header = dynamic_cast<HBox*>(children[0]);
            if (header)
                header->setSize({size.width, bar});
            if (_settingsZoom) {
                _settingsZoom->setPosition({0, bar});
                _settingsZoom->setSize({size.width, std::max(1, size.height - bar)});
            }
        }
    }
}

void DefaultTrackerWindow::showMenu()
{
    if (!_mobileMenu)
        return;
    std::vector<MobileMenu::Entry> entries;
    entries.push_back({MENU_LOAD, 0, "Load pack"});
    entries.push_back({MENU_GET_PACKS, 0, "Get more packs"});
    entries.push_back({MENU_IMPORT_PACK, 0, "Import pack from file (.zip)"});
    if (_tracker) {
        entries.push_back({"", 0, "This pack", true});
        if (_tracker->hasLayout("settings_popup"))
            entries.push_back({MENU_PACK_SETTINGS, 0, "Pack settings"});
        entries.push_back({MENU_RELOAD, 0, "Reload pack"});
        if (!_splitMode || (_view && _view->getSplitPageCount() > 1))
            entries.push_back({MENU_TOGGLE_SPLIT, 0,
                    _splitMode ? "Show items and maps together" : "Show items and maps separately"});
        entries.push_back({MENU_TOGGLE_VISIBILITY, 0,
                _menuLocationsHidden ? "Show cleared / blocked locations" : "Hide cleared / blocked locations"});
        entries.push_back({MENU_SAVE_STATE, 0, "Export state..."});
        entries.push_back({MENU_LOAD_STATE, 0, "Import state..."});
        bool atHeader = false;
        for (size_t i = 0; i < _autoTrackerStates.size(); i++) {
            const auto state = _autoTrackerStates[i];
            if (state == AutoTracker::State::Unavailable)
                continue;
            if (!atHeader) {
                entries.push_back({"", 0, "Auto tracking", true});
                atHeader = true;
            }
            std::string name = _autoTrackerNames[i].empty() ? "Auto Tracker" : _autoTrackerNames[i];
            const char* action = state == AutoTracker::State::Disabled ? "Connect " : "Disconnect ";
            const char* status = state == AutoTracker::State::Disconnected ? " (offline)" :
                                 state == AutoTracker::State::BridgeConnected ? " (no game)" :
                                 state == AutoTracker::State::ConsoleConnected ? " (online)" : "";
            entries.push_back({MENU_TOGGLE_AUTOTRACKER, (int)i, action + name + status});
        }
    }
    entries.push_back({"", 0, "View", true});
    if (_zoomView && _zoomView->isZoomed())
        entries.push_back({"reset-zoom", 0, "Fit tracker to screen"});
    entries.push_back({MENU_UI_LARGER, 0, "Larger UI"});
    entries.push_back({MENU_UI_SMALLER, 0, "Smaller UI"});
    entries.push_back({MENU_HELP, 0, "Help"});
    _mobileMenu->onSelected -= this;
    _mobileMenu->onSelected += {this, [this](void*, const std::string& id, int index) {
        if (id == "reset-zoom") {
            resetZoom();
        } else if (id == MENU_TOGGLE_SPLIT) {
            setSplitMode(!_splitMode);
            onMenuPressed.emit(this, id, _splitMode ? 1 : 0); // to save the setting
        } else {
            onMenuPressed.emit(this, id, index);
        }
    }};
    _mobileMenu->setEntries(entries);
    _mobileMenu->setVisible(true);
    raiseChild(_mobileMenu);
}

void DefaultTrackerWindow::showPackBrowser()
{
    if (!_packBrowser)
        return;
    _packBrowser->setLoading();
    _packBrowser->setVisible(true);
    raiseChild(_packBrowser);
}

void DefaultTrackerWindow::setPackBrowserPacks(const nlohmann::json& packs, const std::set<std::string>& installedUids)
{
    if (_packBrowser)
        _packBrowser->setPacks(packs, installedUids);
}

void DefaultTrackerWindow::setPackBrowserError(const std::string& message)
{
    if (_packBrowser)
        _packBrowser->setError(message);
}

void DefaultTrackerWindow::hidePackBrowser()
{
    if (!_packBrowser)
        return;
    _packBrowser->releaseFocus();
    _packBrowser->setVisible(false);
}

void DefaultTrackerWindow::showPackSettings(Tracker* tracker)
{
    if (!tracker || !tracker->hasLayout("settings_popup"))
        return;
    hidePackSettings();
    const int bar = Touch::barHeight();

    _settingsOverlay = new OverlayContainer(0, 0, _size.width, _size.height);
    _settingsOverlay->setBackground({0x40, 0x40, 0x40});

    auto header = new HBox(0, 0, _size.width, bar);
    header->setBackground({0, 0, 0});
    header->setPadding(2);
    header->setSpacing(8);
    auto btnClose = new Button(0, 0, 0, bar - 4, _font, "  Close  ");
    btnClose->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::MIDDLE);
    btnClose->setMinSize({std::max(btnClose->getMinWidth(), 72), bar - 4});
    btnClose->setSize({std::max(btnClose->getMinWidth(), 72), bar - 4});
    header->addChild(btnClose);
    auto title = new Label(0, 0, 0, bar - 4, _font, "Pack settings");
    title->setTextAlignment(Label::HAlign::LEFT, Label::VAlign::MIDDLE);
    title->setGrow(1, 0);
    header->addChild(title);
    _settingsOverlay->addChild(header);
    btnClose->onClick += {this, [this](void*, int, int, int) {
        hidePackSettings(true); // we are inside the overlay's click handler, delete later
    }};

    _settingsView = new TrackerView(0, 0, 100, 100, tracker, "settings_popup", _fontStore);
    _settingsZoom = new ZoomView(0, bar, _size.width, std::max(1, _size.height - bar));
    _settingsZoom->setBackground({0x7f, 0x7f, 0x7f});
    _settingsZoom->setMaxFit(2.0f); // settings are usually small; make them easy to tap
    _settingsZoom->setChild(_settingsView);
    _settingsView->onMinSizeChanged += {this, [this](void*) {
        if (_settingsZoom)
            _settingsZoom->relayout();
    }};
    _settingsOverlay->addChild(_settingsZoom);

    addChild(_settingsOverlay);
    layoutOverlays(_size);
    if (_mobileMenu) raiseChild(_mobileMenu);
}

void DefaultTrackerWindow::hidePackSettings(bool deferred)
{
    if (!_settingsOverlay)
        return;
    if (deferred) {
        _settingsOverlay->setVisible(false);
        _pendingDelete.push_back(_settingsOverlay);
    } else {
        removeChild(_settingsOverlay);
        delete _settingsOverlay; // deletes zoom view and settings view
    }
    _settingsOverlay = nullptr;
    _settingsZoom = nullptr;
    _settingsView = nullptr;
}

bool DefaultTrackerWindow::handleBack()
{
    // close the top-most overlay
    if (_mobileMenu && _mobileMenu->getVisible()) {
        _mobileMenu->setVisible(false);
        return true;
    }
    if (_packBrowser && _packBrowser->getVisible()) {
        hidePackBrowser();
        return true;
    }
    if (_settingsOverlay) {
        hidePackSettings();
        return true;
    }
    if (_loadPackWidget && _loadPackWidget->getVisible() && _view) {
        hideOpen();
        return true;
    }
    if (_zoomView && _zoomView->isZoomed()) {
        resetZoom();
        return true;
    }
    return false;
}

static void styleSplitButton(Button* btn, bool active)
{
    // selected page: accent color, like a segmented control
    btn->setBackground(active ? Widget::Color{0x2a, 0x62, 0xc0} : Widget::Color{0x30, 0x30, 0x30});
    btn->setTextColor(active ? Widget::Color{0xff, 0xff, 0xff} : Widget::Color{0xb0, 0xb0, 0xb0});
}

void DefaultTrackerWindow::setSplitMode(bool split)
{
    _splitMode = split;
    if (_view && Touch::enabled())
        _view->setSplitMode(_splitMode, _splitPage);
    resetZoom();
    updateSplitButtons();
}

void DefaultTrackerWindow::setSplitPage(int page)
{
    _splitPage = page;
    if (_view)
        _view->setSplitPage(page);
    resetZoom();
    for (size_t i = 0; i < _splitButtons.size(); i++)
        styleSplitButton(_splitButtons[i], (int)i == page);
}

void DefaultTrackerWindow::updateSplitButtons()
{
    if (!_splitBox)
        return;
    // NOTE: never called from a split button's own click handler, since it deletes the buttons
    _splitBox->clearChildren();
    _splitButtons.clear();
    const int h = _splitBox->getHeight();
    int w = 0;
    if (_view && _splitMode && _view->getSplitPageCount() > 1) {
        const auto& names = _view->getSplitPageNames();
        _splitPage = _view->getSplitPage();
        for (size_t i = 0; i < names.size(); i++) {
            auto btn = new Button(0, 0, 0, h, _font, " " + names[i] + " ");
            btn->setTextAlignment(Label::HAlign::CENTER, Label::VAlign::MIDDLE);
            const int bw = std::max(btn->getAutoWidth() + 8, 56);
            btn->setMinSize({bw, h});
            btn->setSize({bw, h});
            styleSplitButton(btn, (int)i == _splitPage);
            const int page = (int)i;
            btn->onClick += {this, [this, page](void*, int, int, int button) {
                if (button == BUTTON_LEFT)
                    setSplitPage(page);
            }};
            _splitBox->addChild(btn);
            _splitButtons.push_back(btn);
            w += bw + (i ? 2 : 0);
        }
    }
    _splitBox->setMinSize({w, h});
    _splitBox->setSize({w, h});
    if (auto menu = dynamic_cast<HBox*>(_menu))
        menu->relayout();
}

} // namespace
