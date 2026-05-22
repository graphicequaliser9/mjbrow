/**
 * @file PaintProfiler.cpp
 * @brief Paint profiler implementation: per-frame timing and overlay text.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "devtools/PaintProfiler.h"

#include "util/Logging.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace devtools {

PaintProfiler::PaintProfiler() = default;
PaintProfiler::~PaintProfiler() = default;

// ── timing markers ─────────────────────────────────────────────────────

void PaintProfiler::beginPaint()  { paintStartUs_   = getTimeUs(); }
void PaintProfiler::beginLayout() { layoutStartUs_  = getTimeUs(); }

void PaintProfiler::endPaint() {
    lastFrame_.paintUs = (getTimeUs() - paintStartUs_);
}

void PaintProfiler::endLayout() {
    lastFrame_.layoutUs = (getTimeUs() - layoutStartUs_);
}

void PaintProfiler::recordJSTime(double us) {
    lastFrame_.jsUs = us;
}

void PaintProfiler::endFrame() {
    // Advance the ring history
    if (frameHistory_.size() >= 60) frameHistory_.pop_front();
    frameHistory_.push_back(lastFrame_);

    // FPS = 1 / (paint + layout + JS) in seconds, clamped to sensible range
    double frameTotalUs = lastFrame_.paintUs + lastFrame_.layoutUs + lastFrame_.jsUs;
    if (frameTotalUs > 0.0) {
        lastFrame_.fps = 1'000'000.0 / frameTotalUs;
    }
    lastFrame_.fps = std::clamp(lastFrame_.fps, 0.0, 999.0);

    // Update rolling averages
    if (sampleCount_ == 0) {
        avgPaintUs_  = lastFrame_.paintUs;
        avgLayoutUs_ = lastFrame_.layoutUs;
        avgJsUs_     = lastFrame_.jsUs;
    } else {
        double alpha = 1.0 / double(sampleCount_ + 1);
        avgPaintUs_  = avgPaintUs_  * (1.0 - alpha) + lastFrame_.paintUs  * alpha;
        avgLayoutUs_ = avgLayoutUs_ * (1.0 - alpha) + lastFrame_.layoutUs * alpha;
        avgJsUs_     = avgJsUs_     * (1.0 - alpha) + lastFrame_.jsUs     * alpha;
    }
    ++sampleCount_;
}

// ── overlay text ───────────────────────────────────────────────────────

std::string PaintProfiler::formatTime(double us) {
    if (us < 1000.0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0) << us << " µs";
        return ss.str();
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (us / 1000.0) << " ms";
    return ss.str();
}

std::string PaintProfiler::getOverlayText(int /*width*/, int height) const {
    if (!visible_) return "";

    auto fmt = [this]() -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0)
           << "FPS: "      << std::setw(5) << lastFrame_.fps
           << "  Paint: "  << std::setw(8) << formatTime(lastFrame_.paintUs)
           << "  Layout: " << std::setw(8) << formatTime(lastFrame_.layoutUs)
           << "  JS: "     << std::setw(8) << formatTime(lastFrame_.jsUs);
        return ss.str();
    };

    std::string line1 = fmt();

    switch (height) {
        case 1: return line1;
        case 3: {
            std::ostringstream ss;
            ss << "FPS: "      << std::setw(5) << lastFrame_.fps << '\n'
               << "Paint:  "   << std::setw(8) << formatTime(avgPaintUs_)  << '\n'
               << "Layout: "   << std::setw(8) << formatTime(avgLayoutUs_) << '\n'
               << "JS:         " << std::setw(8) << formatTime(avgJsUs_);
            return ss.str();
        }
        default: {
            std::ostringstream ss;
            ss << "FPS: "      << std::setw(5) << lastFrame_.fps << "  "
               << "Paint: "  << std::setw(8) << formatTime(avgPaintUs_)
               << "  Layout: " << std::setw(8) << formatTime(avgLayoutUs_)
               << "  JS: "    << std::setw(8) << formatTime(avgJsUs_);
            return ss.str();
        }
    }
}

FrameTiming PaintProfiler::getLastFrameTiming() const {
    return lastFrame_;
}

void PaintProfiler::reset() {
    lastFrame_      = FrameTiming{};
    frameHistory_.clear();
    avgPaintUs_     = 0.0;
    avgLayoutUs_    = 0.0;
    avgJsUs_        = 0.0;
    sampleCount_    = 0;
}

// ── portable time source ───────────────────────────────────────────────

double PaintProfiler::getTimeUs() {
    using namespace std::chrono;
    auto now  = steady_clock::now();
    auto us   = duration_cast<microseconds>(now.time_since_epoch());
    return static_cast<double>(us.count());
}

} // namespace devtools
