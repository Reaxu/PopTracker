// Dialogs on Android: message boxes through SDL (AlertDialog), text input and file pickers through
// PopTrackerActivity (android/app/src/main/java/org/poptracker/PopTrackerActivity.java).
// All calls block the SDL thread until the user answered, like the desktop implementations.

#include "dlg.h"
#include "../platform/android.h"
#include <SDL2/SDL.h>
#include <mutex>
#include <string>
#include <vector>

namespace Ui {

static std::mutex _mutex;

static const char STRING_METHOD_SIG[] =
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/String;";

bool Dlg::InputBox(const std::string& title, const std::string& message, const std::string& dflt,
                   std::string& result, bool password)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::string res;
    if (!Platform::Android::callStringMethod("inputBox", STRING_METHOD_SIG, res, title, message,
            password ? "" : dflt, password))
        return false; // cancelled
    result = res;
    return true;
}

Dlg::Result Dlg::MsgBox(const std::string& title, const std::string& message, Dlg::Buttons btns,
                        Dlg::Icon icon, Dlg::Result dflt)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<SDL_MessageBoxButtonData> buttons;
    auto add = [&](Result r, const char* text) {
        Uint32 flags = 0;
        if (r == dflt)
            flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        if (r == Result::Cancel || (r == Result::No && btns == Buttons::YesNo))
            flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        buttons.push_back({flags, (int)r, text});
    };
    switch (btns) {
        case Buttons::OK:
            add(Result::OK, "OK");
            break;
        case Buttons::OKCancel:
            add(Result::Cancel, "Cancel");
            add(Result::OK, "OK");
            break;
        case Buttons::YesNo:
            add(Result::No, "No");
            add(Result::Yes, "Yes");
            break;
        case Buttons::YesNoCancel:
            add(Result::Cancel, "Cancel");
            add(Result::No, "No");
            add(Result::Yes, "Yes");
            break;
    }
    Uint32 flags = 0;
    if (icon == Icon::Error)
        flags = SDL_MESSAGEBOX_ERROR;
    else if (icon == Icon::Warning)
        flags = SDL_MESSAGEBOX_WARNING;
    else
        flags = SDL_MESSAGEBOX_INFORMATION;

    SDL_MessageBoxData data = {};
    data.flags = flags;
    data.window = nullptr;
    data.title = title.c_str();
    data.message = message.c_str();
    data.numbuttons = (int)buttons.size();
    data.buttons = buttons.data();
    int pressed = (int)Result::Cancel;
    if (SDL_ShowMessageBox(&data, &pressed) != 0) {
        fprintf(stderr, "MsgBox: %s\n", SDL_GetError());
        return btns == Buttons::OK ? Result::OK : Result::Cancel;
    }
    if (pressed < 0) // dismissed
        return btns == Buttons::OK ? Result::OK : btns == Buttons::YesNo ? Result::No : Result::Cancel;
    return (Result)pressed;
}

static std::string mimeTypesFor(const std::list<Dlg::FileType>& types)
{
    // map our patterns to MIME types for the system file picker
    std::string mimes;
    for (const auto& type : types) {
        for (const auto& pattern : type.patterns) {
            std::string mime;
            if (pattern.size() >= 5 && pattern.compare(pattern.size() - 5, 5, ".json") == 0)
                mime = "application/json";
            else if (pattern.size() >= 4 && pattern.compare(pattern.size() - 4, 4, ".zip") == 0)
                mime = "application/zip";
            if (!mime.empty() && mimes.find(mime) == std::string::npos) {
                if (!mimes.empty())
                    mimes += ",";
                mimes += mime;
            }
        }
    }
    return mimes;
}

bool Dlg::OpenFile(const std::string& title, const fs::path&, const std::list<FileType>& types, fs::path& out,
                   bool multi)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::string res;
    // The activity copies the picked document into the cache dir and returns that path.
    if (!Platform::Android::callStringMethod("openFile", STRING_METHOD_SIG, res, title, mimeTypesFor(types), "",
            multi))
        return false;
    out = fs::u8path(res);
    return true;
}

bool Dlg::SaveFile(const std::string& title, const fs::path& dflt, const std::list<FileType>& types, fs::path& out)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::string mime = mimeTypesFor(types);
    if (mime.find(',') != std::string::npos)
        mime = mime.substr(0, mime.find(','));
    if (mime.empty())
        mime = "application/octet-stream";
    std::string res;
    // The user picks the destination; we get a temporary file to write to. SaveFileDone copies it over.
    if (!Platform::Android::callStringMethod("saveFile", STRING_METHOD_SIG, res, title, dflt.filename().u8string(),
            mime, false))
        return false;
    out = fs::u8path(res);
    return true;
}

bool Dlg::SaveFileDone(const fs::path& file, bool success)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return Platform::Android::callBoolMethod("saveFileDone", "(Ljava/lang/String;Z)Z", file.u8string(), success)
            || !success;
}

bool Dlg::hasGUI()
{
    return true;
}

} // namespace Ui
