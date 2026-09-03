#include "workstation/renderer/BenchmarkTimer.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace workstation { namespace renderer {

void BenchmarkTimer::BeginRun(uint64_t targetFrames) {
    targetFrames_ = targetFrames;
    frameCount_ = 0;
    totalPointsRendered_ = 0;
    frameTimesMs_.clear();
    frameTimesMs_.reserve(targetFrames);
    runStart_ = std::chrono::high_resolution_clock::now();
    running_ = true;
}

void BenchmarkTimer::BeginFrame() {
    if (!running_) return;
    frameStart_ = std::chrono::high_resolution_clock::now();
}

void BenchmarkTimer::EndFrame(uint64_t pointsRendered) {
    if (!running_) return;
    auto now = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - frameStart_).count();
    frameTimesMs_.push_back(ms);
    totalPointsRendered_ += pointsRendered;
    ++frameCount_;

    if (frameCount_ >= targetFrames_) {
        EndRun();
    }
}

void BenchmarkTimer::EndRun() {
    if (!running_) return;
    running_ = false;

    auto runEnd = std::chrono::high_resolution_clock::now();
    result_.totalSeconds = std::chrono::duration<double>(runEnd - runStart_).count();
    result_.totalFrames = frameCount_;
    result_.totalPointsRendered = totalPointsRendered_;

    if (frameCount_ > 0) {
        result_.avgFps = static_cast<double>(frameCount_) / result_.totalSeconds;
        result_.avgPointsPerFrame = static_cast<double>(totalPointsRendered_) / frameCount_;
    }

    if (!frameTimesMs_.empty()) {
        std::sort(frameTimesMs_.begin(), frameTimesMs_.end());
        double sum = 0.0;
        for (double ms : frameTimesMs_) {
            sum += ms;
            result_.minFrameTimeMs = std::min(result_.minFrameTimeMs, ms);
            result_.maxFrameTimeMs = std::max(result_.maxFrameTimeMs, ms);
        }
        result_.avgFrameTimeMs = sum / frameTimesMs_.size();
        // 99th percentile
        size_t p99idx = static_cast<size_t>(frameTimesMs_.size() * 0.99);
        if (p99idx >= frameTimesMs_.size()) p99idx = frameTimesMs_.size() - 1;
        result_.p99FrameTimeMs = frameTimesMs_[p99idx];
    }
}

void BenchmarkTimer::PrintSummary() const {
    fprintf(stderr, "\n=== Benchmark Results ===\n");
    fprintf(stderr, "  Total time:       %.2f s\n", result_.totalSeconds);
    fprintf(stderr, "  Total frames:     %llu\n", result_.totalFrames);
    fprintf(stderr, "  Average FPS:      %.1f\n", result_.avgFps);
    fprintf(stderr, "  Frame time:       avg=%.2f ms  min=%.2f ms  max=%.2f ms  p99=%.2f ms\n",
            result_.avgFrameTimeMs, result_.minFrameTimeMs,
            result_.maxFrameTimeMs, result_.p99FrameTimeMs);
    fprintf(stderr, "  Points/frame:     %.0f\n", result_.avgPointsPerFrame);
    fprintf(stderr, "  Points/sec:       %.0f\n",
            result_.avgFps * result_.avgPointsPerFrame);
    fprintf(stderr, "=========================\n\n");
    fflush(stderr);
}

} // namespace renderer
} // namespace workstation
