#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

namespace workstation { namespace renderer {

// Lightweight frame-timing accumulator for benchmark runs.
// Collects per-frame stats and computes aggregate metrics.
struct BenchmarkResult {
    double totalSeconds = 0.0;
    uint64_t totalFrames = 0;
    double avgFps = 0.0;
    double avgFrameTimeMs = 0.0;
    double minFrameTimeMs = 1e9;
    double maxFrameTimeMs = 0.0;
    double p99FrameTimeMs = 0.0;
    uint64_t totalPointsRendered = 0;
    double avgPointsPerFrame = 0.0;
};

class BenchmarkTimer {
public:
    void BeginRun(uint64_t targetFrames = 300);
    void BeginFrame();
    void EndFrame(uint64_t pointsRendered);
    void EndRun();

    bool IsRunning() const { return running_; }
    BenchmarkResult GetResult() const { return result_; }

    // Print a summary line to stderr.
    void PrintSummary() const;

private:
    bool running_ = false;
    uint64_t targetFrames_ = 300;
    uint64_t frameCount_ = 0;
    uint64_t totalPointsRendered_ = 0;
    std::chrono::high_resolution_clock::time_point runStart_;
    std::chrono::high_resolution_clock::time_point frameStart_;
    std::vector<double> frameTimesMs_;
    BenchmarkResult result_;
};

} // namespace renderer
} // namespace workstation
