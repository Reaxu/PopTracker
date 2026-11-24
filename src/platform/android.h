#pragma once

#ifdef __ANDROID__

#include <jni.h>
#include <string>

namespace Platform {

/// Prepare the process before PopTracker starts: log to logcat, set up HOME / app path / working directory and
/// SDL hints. Call first thing in main().
void androidInit();

/// Helpers to call methods of org.poptracker.PopTrackerActivity from the SDL thread.
/// All return false / empty on error.
namespace Android {
    /// Calls a String method(String, String, String, boolean) - used for dialogs.
    bool callStringMethod(const char* name, const char* signature, std::string& out,
                          const std::string& a, const std::string& b, const std::string& c, bool flag);
    /// Calls a boolean method(String, boolean)
    bool callBoolMethod(const char* name, const char* signature, const std::string& a, bool flag);
}

} // namespace Platform

#endif
