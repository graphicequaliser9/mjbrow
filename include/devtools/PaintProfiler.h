/**
 * @file PaintProfiler.h
 * @brief Frame-timing overlay (paint profiler).
 * @details Tracks per-frame paint time, layout (reflow) time, and JS
 *          execution time.  Renders as a compact on-screen overlay so
 *          developers can see performance regressions in real time.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef DEVTOOLS_PAINTPROFILER_H
#define DEVTOOLS_PAINTPROFILER_H

#include <cstdint>
#include <string>
#include <deque>

namespace devtools {

/**
 * @struct FrameTiming
 * @brief Timing breakdown for a single rendered frame.
 */
struct FrameTiming {
    double paintUs{0.0};      ///< Microseconds spent painting the last frame
    double layoutUs{0.0};     ///< Microseconds spent in layout/reflow
    double jsUs{0.0};         ///< Microseconds spent executing JS in the VM
    double fps{0.0};          ///< Instantaneous frames-per-second this frame
};

/**
 * @class PaintProfiler
 * @brief Collects per-frame timing samples and produces an overlay string.
 */
class PaintProfiler {
public:
    PaintProfiler();
    ~PaintProfiler();

    /**
     * @brief Marks the start of a paint phase for the current frame.
     * Call this right before the painting traversal begins.
     */
    void beginPaint();

    /**
     * @brief Marks the end of the paint phase; records paint elapsed time.
     */
    void endPaint();

    /**
     * @brief Marks the start of the layout/reflow phase.
     */
    void beginLayout();

    /**
     * @brief Marks the end of the layout/reflow phase.
     */
    void endLayout();

    /**
     * @brief Registers elapsed JS execution time for the current frame.
     * @param microseconds Time spent in the JavaScript VM this frame.
     */
    void recordJSTime(double microseconds);

    /**
     * @brief Called once per frame after paint, layout, and JS are recorded.
     *        Advances internal stats, computes FPS, and slices the frame ring.
     */
    void endFrame();

    /**
     * @brief Returns a human-readable overlay string for on-screen display.
     * @param width  Overlay panel width in characters (self-clip at this width).
     * @param height Number of lines to show (1 = single-line, 3 = full three-line view).
     * @return Formatted string ready for Painter::drawText.
     */
    std::string getOverlayText(int width = 60, int height = 3) const;

    /**
     * @brief Returns the most recent frame's timing data.
     */
    FrameTiming getLastFrameTiming() const;

    /**
     * @brief Whether the profiler overlay is currently visible.
     */
    bool isVisible() const { return visible_; }

    /**
     * @brief Toggle  visibility of the overlay.
     */
    void setVisible(bool v) { visible_ = v; }

    /**
     * @brief Reset all accumulated statistics.
     */
    void reset();

private:
    /**
     * @brief Format a double microsecond value into a readable string.
     * @param us Microseconds.
     * @return "N us" or "N.NN ms" depending on magnitude.
     */
    static std::string formatTime(double us);

    double paintStartUs_{0.0};
    double layoutStartUs_{0.0};

    FrameTiming lastFrame_;
    std::deque<FrameTiming> frameHistory_;  ///< Ring history (up to 60 frames)

    bool visible_{false};                   ///< Overlay visibility flag

    // Rolling averages
    double avgPaintUs_{0.0};
    double avgLayoutUs_{0.0};
    double avgJsUs_{0.0};
    size_t sampleCount_{0};

    /// @brief Current high-resolution tick in microseconds (steady-clock).
    double getTimeUs();
};

} // namespace devtools

#endif // DEVTOOLS_PAINTPROFILER_H
