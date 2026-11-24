# CMake build for PopTracker.
#
# Meson (meson.build) is still the primary desktop build system.
# This file exists so PopTracker can be built for Android with the NDK/Gradle toolchain,
# and it also works for a quick desktop build:
#
#   cmake -S . -B build-cmake -DCMAKE_PREFIX_PATH=/path/to/sdl2/prefix
#   cmake --build build-cmake
#
# On Android, android/app/build.gradle drives this file. SDL2, SDL2_image and SDL2_ttf are
# built from source (see android/fetch-deps.sh) and OpenSSL comes from a prefab package
# (or from POPTRACKER_OPENSSL_ROOT if you built it yourself).

cmake_minimum_required(VERSION 3.18)

project(PopTracker VERSION 0.36.0 LANGUAGES C CXX)
set(POPTRACKER_VERSION_EXTRA "")
set(POPTRACKER_VERSION_PRERELEASE 0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # gnu++17, matching meson's c++1z on gcc/clang

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(PT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# --- version.h / version.cpp -------------------------------------------------

set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(VERSION_REVISION ${PROJECT_VERSION_PATCH})
set(VERSION_EXTRA "${POPTRACKER_VERSION_EXTRA}")
set(VERSION_PRERELEASE_FLAG ${POPTRACKER_VERSION_PRERELEASE})
configure_file(${PT_ROOT}/src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/gen/version.h @ONLY)
if(NOT DEFINED VCS_TAG)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --dirty
            WORKING_DIRECTORY ${PT_ROOT} OUTPUT_VARIABLE VCS_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    if(NOT VCS_TAG)
        set(VCS_TAG "dev")
    endif()
endif()
configure_file(${PT_ROOT}/src/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/gen/version.cpp @ONLY)

# --- dependencies -------------------------------------------------------------

option(POPTRACKER_BUNDLED_SDL "Build SDL2, SDL2_image and SDL2_ttf from POPTRACKER_DEPS_DIR (always on for Android)" OFF)
set(POPTRACKER_DEPS_DIR "${PT_ROOT}/android/deps" CACHE PATH "Folder containing SDL2, SDL2_image, SDL2_ttf sources")

if(ANDROID OR POPTRACKER_BUNDLED_SDL)
    # SDL2 + satellite libs from source (see android/fetch-deps.sh)

    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(${POPTRACKER_DEPS_DIR}/SDL2 ${CMAKE_CURRENT_BINARY_DIR}/SDL2 EXCLUDE_FROM_ALL)

    # SDL2_image and SDL2_ttf are built as shared libs (libSDL2_image.so, libSDL2_ttf.so);
    # PopTrackerActivity.getLibraries() loads them before libmain.so.
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    # vendored libjpeg + libpng (same decoders as desktop; stb_image rejects some valid JPEGs)
    set(SDL2IMAGE_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_STB OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_SVG OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE) # no install/export rules for vendored zlib/libpng/libjpeg
    add_subdirectory(${POPTRACKER_DEPS_DIR}/SDL2_image ${CMAKE_CURRENT_BINARY_DIR}/SDL2_image EXCLUDE_FROM_ALL)

    set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${POPTRACKER_DEPS_DIR}/SDL2_ttf ${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf EXCLUDE_FROM_ALL)

    # PopTracker includes <SDL2/SDL_image.h> and <SDL2/SDL_ttf.h>; the source trees don't have that layout.
    configure_file(${POPTRACKER_DEPS_DIR}/SDL2_image/include/SDL_image.h ${CMAKE_CURRENT_BINARY_DIR}/sdlinc/SDL2/SDL_image.h COPYONLY)
    configure_file(${POPTRACKER_DEPS_DIR}/SDL2_ttf/SDL_ttf.h ${CMAKE_CURRENT_BINARY_DIR}/sdlinc/SDL2/SDL_ttf.h COPYONLY)

    set(PT_SDL_LIBS SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)
else()
    find_package(SDL2 REQUIRED CONFIG)
    find_package(SDL2_image REQUIRED CONFIG)
    find_package(SDL2_ttf REQUIRED CONFIG)
    set(PT_SDL_LIBS SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)
endif()

if(ANDROID)
    # OpenSSL: by default a static build from android/build-openssl.sh (android/deps/openssl/<abi>),
    # otherwise the prefab package com.android.ndk.thirdparty:openssl (needs -PuseOpenSSLPrefab).
    set(POPTRACKER_OPENSSL_ROOT "" CACHE PATH "Optional: prefix of a self-built OpenSSL for this ABI")
    if(NOT POPTRACKER_OPENSSL_ROOT AND EXISTS "${POPTRACKER_DEPS_DIR}/openssl/${ANDROID_ABI}/include/openssl/ssl.h")
        set(POPTRACKER_OPENSSL_ROOT "${POPTRACKER_DEPS_DIR}/openssl/${ANDROID_ABI}")
    endif()
    if(POPTRACKER_OPENSSL_ROOT)
        message(STATUS "Using OpenSSL from ${POPTRACKER_OPENSSL_ROOT}")
        # the NDK toolchain only searches the sysroot; let FindOpenSSL see our prefix too
        list(APPEND CMAKE_FIND_ROOT_PATH "${POPTRACKER_OPENSSL_ROOT}")
        set(OPENSSL_ROOT_DIR "${POPTRACKER_OPENSSL_ROOT}")
        set(OPENSSL_USE_STATIC_LIBS TRUE)
        find_package(OpenSSL REQUIRED)
        set(PT_SSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
    else()
        # prefab package com.android.ndk.thirdparty:openssl (see android/app/build.gradle)
        find_package(openssl REQUIRED CONFIG)
        set(PT_SSL_LIBS openssl::ssl openssl::crypto)
    endif()
    if(NOT TARGET ZLIB::ZLIB) # SDL2_image's vendored zlib provides it
        find_package(ZLIB REQUIRED)
    endif()
    set(PT_PLATFORM_LIBS ZLIB::ZLIB log android)
else()
    find_package(OpenSSL REQUIRED)
    if(NOT TARGET ZLIB::ZLIB)
        find_package(ZLIB REQUIRED)
    endif()
    find_package(Threads REQUIRED)
    set(PT_SSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
    set(PT_PLATFORM_LIBS ZLIB::ZLIB Threads::Threads)
    if(WIN32)
        list(APPEND PT_PLATFORM_LIBS wsock32 ws2_32 crypt32)
    endif()
endif()

# --- lua (compiled as C++) ---------------------------------------------------

set(LUA_SRC
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c
    lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c ltests.c
    lauxlib.c
    lbaselib.c lcorolib.c lmathlib.c ltablib.c lstrlib.c lutf8lib.c loslib.c
)
list(TRANSFORM LUA_SRC PREPEND ${PT_ROOT}/lib/lua/)
set_source_files_properties(${LUA_SRC} PROPERTIES LANGUAGE CXX)
add_library(pt_lua STATIC ${LUA_SRC})
target_include_directories(pt_lua PUBLIC ${PT_ROOT}/lib/lua)
target_compile_definitions(pt_lua PUBLIC LUA_CPP PRIVATE _REENTRANT)
if(NOT WIN32)
    target_compile_definitions(pt_lua PRIVATE LUA_USE_POSIX)
endif()
set_target_properties(pt_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- PopTracker ---------------------------------------------------------------

set(PT_SRC
    poptracker.cpp
    app.cpp
    main.cpp
    ap/aptracker.cpp
    ap/archipelago.cpp
    appupdater/appupdater.cpp
    core/assets.cpp
    core/autotracker.cpp
    core/baseitem.cpp
    core/gameinfo.cpp
    core/imagereference.cpp
    core/jsonitem.cpp
    core/layoutclass.cpp
    core/layoutnode.cpp
    core/location.cpp
    core/locationsection.cpp
    core/log.cpp
    core/luaitem.cpp
    core/map.cpp
    core/pack.cpp
    core/scripthost.cpp
    core/sha256.cpp
    core/statemanager.cpp
    core/tracker.cpp
    core/version.cpp
    core/zip.cpp
    gh/api/release.cpp
    gh/api/releaseasset.cpp
    http/http.cpp
    http/httpcache.cpp
    http/httputil.cpp
    luaconnector/connection.cpp
    luaconnector/luaconnector.cpp
    luaconnector/server.cpp
    luasandbox/luapackio.cpp
    packmanager/packmanager.cpp
    uat/uatclient.cpp
    ui/broadcastwindow.cpp
    ui/defaulttrackerwindow.cpp
    ui/item.cpp
    ui/loadpackwidget.cpp
    ui/maptooltip.cpp
    ui/mapwidget.cpp
    ui/mobilemenu.cpp
    ui/packbrowser.cpp
    ui/settingswindow.cpp
    ui/trackerview.cpp
    ui/trackerwindow.cpp
    uilib/button.cpp
    uilib/dock.cpp
    uilib/drawhelper.cpp
    uilib/fontstore.cpp
    uilib/group.cpp
    uilib/image.cpp
    uilib/imagebutton.cpp
    uilib/imagefilter.cpp
    uilib/label.cpp
    uilib/progressbar.cpp
    uilib/tabs.cpp
    uilib/textfield.cpp
    uilib/texturemanager.cpp
    uilib/timer.cpp
    uilib/tooltip.cpp
    uilib/touch.cpp
    uilib/ui.cpp
    uilib/widget.cpp
    uilib/window.cpp
    uilib/zoomview.cpp
    usb2snes/usb2snes.cpp
)
if(ANDROID)
    list(APPEND PT_SRC uilib/dlg_android.cpp platform/android.cpp)
elseif(WIN32)
    list(APPEND PT_SRC uilib/dlg_win32.cpp)
elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND PT_SRC uilib/dlg_cocoa.mm)
else()
    list(APPEND PT_SRC uilib/dlg_tinyfd.cpp)
endif()
list(TRANSFORM PT_SRC PREPEND ${PT_ROOT}/src/)
list(APPEND PT_SRC ${PT_ROOT}/lib/fmt/src/format.cc ${CMAKE_CURRENT_BINARY_DIR}/gen/version.cpp)

if(ANDROID)
    # SDLActivity loads "libmain.so" and calls SDL_main
    add_library(main SHARED ${PT_SRC})
    set(PT_TARGET main)
else()
    add_executable(poptracker ${PT_SRC})
    set(PT_TARGET poptracker)
endif()

target_include_directories(${PT_TARGET} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/gen
    ${CMAKE_CURRENT_BINARY_DIR}/sdlinc
    ${PT_ROOT}
    ${PT_ROOT}/lib
    ${PT_ROOT}/lib/asio/include
    ${PT_ROOT}/lib/fmt/include
    ${PT_ROOT}/lib/json/include
    ${PT_ROOT}/lib/lua
    ${PT_ROOT}/lib/miniz
    ${PT_ROOT}/lib/tinyfiledialogs
    ${PT_ROOT}/lib/valijson/include
    ${PT_ROOT}/lib/wswrap/include
)
target_compile_definitions(${PT_TARGET} PRIVATE ASIO_STANDALONE ASIO_HEADER_ONLY JSON_HAS_CPP_17 LUA_CPP)
target_compile_options(${PT_TARGET} PRIVATE
    -Wno-unused-function -Wno-deprecated-declarations -ffunction-sections -fdata-sections)
if(ANDROID)
    # no self-updater on Android; updates come through the APK
    target_compile_definitions(${PT_TARGET} PRIVATE WITHOUT_UPDATE_CHECK)
    target_link_options(${PT_TARGET} PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(${PT_TARGET} PRIVATE pt_lua ${PT_SDL_LIBS} ${PT_SSL_LIBS} ${PT_PLATFORM_LIBS})
