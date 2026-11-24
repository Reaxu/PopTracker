# PopTracker for Android

This folder builds PopTracker as an Android app (phones and tablets, Android 7.0+).
The app is the normal PopTracker C++ code running on SDL2, with a **touch UI mode** that is always on in the Android
build and can also be enabled on desktop (handy for Windows/Linux tablets and for testing).

## What's different from desktop

| Desktop | Android / touch mode |
|---|---|
| Window grows to fit the pack's layout | Layout is **fitted to the screen**; pinch to zoom, drag to pan, tap **Fit** to see everything again |
| Left click | **Tap** |
| Right click | **Long press** (about half a second) |
| Mouse hover over a map location | **Tap** the location to open its popup, tap elsewhere to close it. Taps near a small location marker still hit it |
| Mouse wheel in lists | **Drag** with one finger |
| Hotkeys (F5, Ctrl+H, ...) | **☰ menu**: load/get/import packs, reload, hide cleared locations, export/import state, connect/disconnect auto trackers, larger/smaller UI, help |
| Extra windows (pack settings, broadcast) | Pack settings open **inside the main window**; broadcast view is not available |
| Drag & drop a pack | **Get more packs** (download from the community pack list), **Import pack (.zip)**, or "Open with PopTracker" from a browser/file manager |
| File dialogs | Android's file picker |
| Back key | Closes the menu/overlay, resets zoom, then sends the app to the background |

Other details:
- UI and text are scaled to the screen density, so text stays sharp. *Larger UI* / *Smaller UI* in the menu adjust it.
- Packs that ship a `tracker_vertical` layout use it in portrait, `tracker_horizontal` in landscape.
- The screen stays on while PopTracker is open (set `"enable_screensaver": true` in the config to change that).
- Auto tracking (Archipelago etc.) keeps running in the background for as long as Android lets the app run; state is saved whenever you leave the app.
- Packs, config and saves live in `Android/data/org.poptracker/files/` on the phone's storage
  (`PopTracker/packs` for packs, `.config/PopTracker` for config and saves). You can also copy packs there over USB.

## Building

You need: Android Studio (or the Android command-line tools), the **NDK 27.2.12479018** and **CMake 3.22.1**
(both installable from Android Studio's SDK Manager), and git. Building OpenSSL needs perl and make
(Linux, macOS, or WSL on Windows).

```sh
git clone --recurse-submodules <this repository>
cd PopTracker/android
./fetch-deps.sh                                  # SDL2, SDL2_image, SDL2_ttf sources -> android/deps
ANDROID_NDK_HOME=~/Android/Sdk/ndk/27.2.12479018 ./build-openssl.sh   # static OpenSSL -> android/deps/openssl
./gradlew assembleDebug                          # or open the android/ folder in Android Studio and press Run
```

The APK ends up in `app/build/outputs/apk/debug/`. Install it with `adb install` or by copying it to the phone.

No perl on Windows? Skip `build-openssl.sh` and build with `./gradlew assembleDebug -PuseOpenSSLPrefab`
to use Google's prebuilt OpenSSL 1.1.1 package instead.

### Without installing anything: GitHub Actions

`.github/workflows/android.yaml` builds a release APK on every push. Fork the repository, push this branch,
open the *Actions* tab, and download the `poptracker-android` artifact.
By default it's signed with a throw-away debug key, so a newer build can't be installed over an older one
(uninstall first, your packs and saves in `Android/data` are removed with it). To sign with your own key, add the secrets
`ANDROID_KEYSTORE_BASE64` (`base64 -w0 my.jks`), `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and
`ANDROID_KEY_PASSWORD`. Locally, set `POPTRACKER_KEYSTORE`, `POPTRACKER_KEYSTORE_PASSWORD`, `POPTRACKER_KEY_ALIAS`
and `POPTRACKER_KEY_PASSWORD` before `./gradlew assembleRelease`.

## Trying the touch UI on desktop

```sh
POPTRACKER_TOUCH_UI=1 POPTRACKER_UI_SCALE=2 ./poptracker
```

or set `"touch_ui": true` in `PopTracker.json`. Ctrl + mouse wheel zooms like a pinch.
A touch screen works with real finger gestures.

## How it fits together

- `CMakeLists.txt` (repository root): CMake build used by Gradle; also builds on desktop.
- `src/main.cpp` → `SDL_main`, called by `PopTrackerActivity` (extends SDL's `SDLActivity`).
- `src/platform/android.cpp`: storage paths, logcat output, SDL hints, JNI helpers.
- `src/uilib/dlg_android.cpp`: message boxes (SDL), text input and file pickers (Java).
- `src/uilib/touch.*`: touch mode settings and gesture recognition.
- `src/uilib/zoomview.*`: fits a layout to the screen with pinch-zoom and pan.
- `src/ui/mobilemenu.*`, `src/ui/packbrowser.*`: the ☰ menu and the pack download list.
- `app/src/main/java/org/poptracker/PopTrackerActivity.java`: asset extraction, dialogs, "Open with".

## Known limitations

- Broadcast view and "always on top" are desktop only.
- USB2SNES/SNI and the Lua connector expect the bridge on the same device (`localhost`); point them at
  a PC on your network where supported, or use Archipelago auto tracking.
- Large maps are drawn at the pack's resolution and get soft when zoomed in a lot.
