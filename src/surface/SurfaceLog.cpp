#include "workstation/surface/SurfaceLog.h"

#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

namespace workstation {
namespace surface {

namespace {

std::mutex g_mutex;
FILE* g_file = nullptr;
std::string g_path;

// Called with g_mutex held.
void OpenLocked() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string exe(buf, len);
        auto pos = exe.find_last_of("\\/");
        if (pos != std::string::npos) {
            // Derive the file name from the executable so every binary gets
            // its own log: wk_gui.exe -> wk_gui_surface.log,
            // wk_renderer.exe -> wk_renderer_surface.log, etc.
            auto stem = exe.substr(pos + 1);
            auto dot = stem.find_last_of('.');
            if (dot != std::string::npos) stem.resize(dot);
            g_path = exe.substr(0, pos + 1) + stem + "_surface.log";
        } else {
            g_path = "surface_pipeline.log";
        }
    } else
#endif
    {
        g_path = "surface_pipeline.log";
    }
    // "w": every wk_gui.exe run starts a fresh log -- easier to correlate
    // a test session with its file.
    g_file = std::fopen(g_path.c_str(), "w");
}

const char* LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Error: return "ERROR";
        case LogLevel::Warn:  return "WARN ";
        default:              return "INFO ";
    }
}

} // namespace

void SurfaceLog::Write(LogLevel level, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file) {
        OpenLocked();
        if (!g_file) return; // disk failure: logging silently disabled
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{};
    // Serialised by g_mutex, so the non-reentrant localtime is safe here.
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    tmv = *std::localtime(&t);
#endif
    const int ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000);

    std::fprintf(g_file, "[%02d:%02d:%02d.%03d] [%s] ",
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms, LevelTag(level));

    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_file, fmt, args);
    va_end(args);

    std::fputc('\n', g_file);
    std::fflush(g_file);

    // Problems stay visible on stderr as well as in the file.
    if (level != LogLevel::Info) {
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
        std::fputc('\n', stderr);
    }
}

const char* SurfaceLog::GetPath() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file) {
        OpenLocked();
    }
    return g_path.c_str();
}

} // namespace surface
} // namespace workstation
