#ifdef __ANDROID__

#include "android.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_system.h>
#include <android/log.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

namespace Platform {

static int logPipe[2] = {-1, -1};

static void* logThread(void*)
{
    // forward everything written to stdout/stderr to logcat, line by line
    char buf[1024];
    std::string line;
    ssize_t n;
    while ((n = read(logPipe[0], buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                __android_log_write(ANDROID_LOG_INFO, "PopTracker", line.c_str());
                line.clear();
            } else if (buf[i] != '\r') {
                line += buf[i];
                if (line.size() > 4000) { // logcat line limit
                    __android_log_write(ANDROID_LOG_INFO, "PopTracker", line.c_str());
                    line.clear();
                }
            }
        }
    }
    return nullptr;
}

static void redirectOutputToLogcat()
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    if (pipe(logPipe) != 0)
        return;
    dup2(logPipe[1], STDOUT_FILENO);
    dup2(logPipe[1], STDERR_FILENO);
    pthread_t thread;
    if (pthread_create(&thread, nullptr, logThread, nullptr) == 0)
        pthread_detach(thread);
}

static void mkdirs(const std::string& path)
{
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1)
            mkdir(cur.c_str(), 0700);
    }
    mkdir(path.c_str(), 0700);
}

void androidInit()
{
    redirectOutputToLogcat();

    // Internal storage (/data/data/<pkg>/files): bundled assets, extracted by PopTrackerActivity.
    const char* internal = SDL_AndroidGetInternalStoragePath();
    // External app storage (/sdcard/Android/data/<pkg>/files): packs, config and saves,
    // reachable from a PC over USB so packs can be copied there by hand.
    const char* external = nullptr;
    if (SDL_AndroidGetExternalStorageState() & SDL_ANDROID_EXTERNAL_STORAGE_WRITE)
        external = SDL_AndroidGetExternalStoragePath();

    const std::string appPath = internal ? internal : ".";
    const std::string home = external ? external : appPath;
    printf("Android: app path %s, home %s\n", appPath.c_str(), home.c_str());

    setenv("POPTRACKER_APP_PATH", appPath.c_str(), 1);
    setenv("HOME", home.c_str(), 1);
    unsetenv("XDG_CONFIG_DIR");
    unsetenv("XDG_DOCUMENTS_DIR");
    mkdirs(home + "/PopTracker/packs");
    if (chdir(appPath.c_str()) != 0)
        fprintf(stderr, "Android: could not chdir to %s\n", appPath.c_str());

    // keep running (networking/auto tracking) while in background, we stop rendering ourselves
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    // back button arrives as SDLK_AC_BACK instead of closing the activity
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    // allow every orientation; layouts switch between tracker_vertical and tracker_horizontal
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    // gestures are handled by Ui (uilib/touch.cpp)
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
}

namespace Android {

static jstring newString(JNIEnv* env, const std::string& s)
{
    return env->NewStringUTF(s.c_str());
}

bool callStringMethod(const char* name, const char* signature, std::string& out,
                      const std::string& a, const std::string& b, const std::string& c, bool flag)
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity)
        return false;
    bool ok = false;
    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetMethodID(cls, name, signature);
    if (mid) {
        jstring ja = newString(env, a);
        jstring jb = newString(env, b);
        jstring jc = newString(env, c);
        auto res = (jstring)env->CallObjectMethod(activity, mid, ja, jb, jc, (jboolean)flag);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        } else if (res) {
            const char* chars = env->GetStringUTFChars(res, nullptr);
            if (chars) {
                out = chars;
                ok = true;
                env->ReleaseStringUTFChars(res, chars);
            }
            env->DeleteLocalRef(res);
        }
        env->DeleteLocalRef(ja);
        env->DeleteLocalRef(jb);
        env->DeleteLocalRef(jc);
    } else {
        env->ExceptionClear();
        fprintf(stderr, "Android: method %s%s not found\n", name, signature);
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return ok;
}

bool callBoolMethod(const char* name, const char* signature, const std::string& a, bool flag)
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity)
        return false;
    bool res = false;
    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetMethodID(cls, name, signature);
    if (mid) {
        jstring ja = newString(env, a);
        res = env->CallBooleanMethod(activity, mid, ja, (jboolean)flag) == JNI_TRUE;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            res = false;
        }
        env->DeleteLocalRef(ja);
    } else {
        env->ExceptionClear();
        fprintf(stderr, "Android: method %s%s not found\n", name, signature);
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return res;
}

} // namespace Android

} // namespace Platform

#endif // __ANDROID__
