#pragma once
//
// SurfaceLog -- lightweight file logger for the surface reconstruction
// pipeline.
//
// Purpose: when wk_gui.exe runs, everything the surface pipeline does
// (initialisation, mesh generation, triangulation results, GPU uploads,
// render decisions and every reason the surface is skipped) is written to
// a log file next to the executable so display problems can be diagnosed
// from disk alone.
//
// Deliberately NOT a general logger: only surface-pipeline events and key
// renderer state transitions call it, and per-frame decisions are logged
// only when the state *changes*, keeping the file small and readable.
//
// The file is <exe_dir>/wk_gui_surface.log, truncated on each run so every
// test session starts from a clean, timestamped history.
//

#include <string>

namespace workstation {
namespace surface {

enum class LogLevel { Info = 0, Warn = 1, Error = 2 };

class SurfaceLog {
public:
    // Opens <exe_dir>/wk_gui_surface.log (truncated) on first use, then
    // appends a timestamped, formatted line and flushes. Thread-safe.
    static void Write(LogLevel level, const char* fmt, ...);

    // Full path of the log file (opens it if not yet opened).
    static const char* GetPath();
};

} // namespace surface
} // namespace workstation

#define SLOG_INFO(...) \
    ::workstation::surface::SurfaceLog::Write(::workstation::surface::LogLevel::Info, __VA_ARGS__)
#define SLOG_WARN(...) \
    ::workstation::surface::SurfaceLog::Write(::workstation::surface::LogLevel::Warn, __VA_ARGS__)
#define SLOG_ERROR(...) \
    ::workstation::surface::SurfaceLog::Write(::workstation::surface::LogLevel::Error, __VA_ARGS__)
