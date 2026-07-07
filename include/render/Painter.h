/**
 * @file Painter.h
 * @brief Painting back-end: abstract HAL + GDI+/Win32 concrete implementation.
 * @details Painter is the rendering back-end contract called once per paint
 *          primitive during the paint pass.  The platform-independent pieces
 *          (GlyphCache, TextBatch, TextRun) implement the glyph-caching and
 *          DrawText-batching strategies required for fast text rendering and are
 *          fully unit-testable on any platform.  The concrete Win32Painter
 *          (GDI+ compositing) is only compiled under _WIN32 and implements the
 *          Painter HAL against a Win32 HDC.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef RENDER_PAINTER_H
#define RENDER_PAINTER_H

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace render {

/**
 * @struct TextRun
 * @brief A single text draw request with its resolved style.
 * @details Instances are accumulated into a TextBatch so that runs sharing the
 *          same visual style can be flushed together, minimising GDI+ brush /
 *          font state changes and re-measurement.
 */
struct TextRun {
    int      x{0};            ///< Left edge (device px).
    int      y{0};            ///< Top edge / baseline hint (device px).
    std::string text;         ///< UTF-8 glyphs to draw.
    uint32_t color{0xFF000000}; ///< ARGB (0xAARRGGBB).
    std::string fontFamily{"Arial"};
    float    fontSize{16.0f};
    int      fontWeight{400};  ///< CSS weight 100..900.
    bool     italic{false};

    /// @brief Stable signature identifying runs that may be batched together.
    std::string styleKey() const {
        std::string k;
        k.reserve(fontFamily.size() + 32);
        k += fontFamily;
        k += '\x1f';
        k += std::to_string(static_cast<int>(fontSize * 100.0f));
        k += '\x1f';
        k += std::to_string(fontWeight);
        k += '\x1f';
        k += (italic ? '1' : '0');
        k += '\x1f';
        k += std::to_string(color);
        return k;
    }
};

/**
 * @class TextBatch
 * @brief Buffers TextRun submissions and groups consecutive same-style runs.
 * @details GDI+ cannot truly batch DrawString calls, so the optimisation is to
 *          (a) keep runs with identical style adjacent and (b) let the flushing
 *          layer reuse one brush + one cached font for the whole group.  This
 *          class is pure C++ and unit-tested cross-platform.
 */
class TextBatch {
public:
    void add(const TextRun& run) { runs_.push_back(run); }

    void clear() { runs_.clear(); }
    bool empty() const { return runs_.empty(); }
    size_t size() const { return runs_.size(); }
    const TextRun& run(size_t i) const { return runs_[i]; }

    /// @brief Calls @p fn with the [start,end) half-open range of every maximal
    ///        contiguous run of identical style.  A range never crosses a style
    ///        boundary, so the flusher can bind state once per range.
    template <typename Fn>
    void forEachBatch(Fn fn) const {
        if (runs_.empty()) return;
        size_t start = 0;
        for (size_t i = 1; i < runs_.size(); ++i) {
            if (runs_[i].styleKey() != runs_[start].styleKey()) {
                fn(start, i);
                start = i;
            }
        }
        fn(start, runs_.size());
    }

private:
    std::vector<TextRun> runs_;
};

/**
 * @class GlyphCache
 * @brief Caches measured glyph-run extents keyed by text + font descriptor.
 * @details The actual measurement is delegated to a platform MeasureFn (GDI+
 *          on Windows, a mock in tests), so the caching policy itself is
 *          platform-independent and verifiable on any toolchain.  Repeated
 *          strings (UI labels, table cells, repeated words) hit the cache and
 *          avoid another MeasureString round-trip.
 */
class GlyphCache {
public:
    using MeasureFn = std::function<std::pair<float, float>(
        const std::string& text,
        const std::string& family,
        float              size,
        int               weight,
        bool              italic)>;

    explicit GlyphCache(MeasureFn measure) : measure_(std::move(measure)) {}

    /// @brief Returns (width, height) in px for the given run, from cache if possible.
    std::pair<float, float> measure(const std::string& text,
                                    const std::string& family,
                                    float              size,
                                    int               weight,
                                    bool              italic) {
        std::string key = makeKey(text, family, size, weight, italic);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            ++hits_;
            return it->second;
        }
        ++misses_;
        std::pair<float, float> ext = measure_(text, family, size, weight, italic);
        cache_.emplace(std::move(key), ext);
        return ext;
    }

    void clear() { cache_.clear(); hits_ = 0; misses_ = 0; }
    size_t size()   const { return cache_.size(); }
    size_t hits()   const { return hits_; }
    size_t misses() const { return misses_; }

private:
    static std::string makeKey(const std::string& text, const std::string& family,
                               float size, int weight, bool italic) {
        std::string k;
        k.reserve(text.size() + family.size() + 32);
        k += family;   k += '\x1f';
        k += std::to_string(static_cast<int>(size * 100.0f)); k += '\x1f';
        k += std::to_string(weight); k += '\x1f';
        k += (italic ? '1' : '0');    k += '\x1f';
        k += text;
        return k;
    }

    std::unordered_map<std::string, std::pair<float, float>> cache_;
    MeasureFn measure_;
    size_t    hits_{0};
    size_t    misses_{0};
};

/**
 * @class Painter
 * @brief Abstract painting HAL.  Concrete back-ends (Win32Painter) implement it.
 */
class Painter {
public:
    virtual ~Painter() = default;

    /// @brief Fills a rectangle with the specified ARGB colour.
    virtual void fillRect(int x, int y, int width, int height, uint32_t color) = 0;

    /// @brief Draws text at the specified location.
    virtual void drawText(int x, int y, const std::string& text, uint32_t color) = 0;

    /// @brief Draws a border around a rectangle.
    virtual void drawBorder(int x, int y, int width, int height, uint32_t color, int thickness) = 0;
};

#ifdef _WIN32
namespace html { class Document; }

/**
 * @class Win32Painter
 * @brief GDI+ implementation of Painter bound to a Win32 HDC.
 * @details Compositing is performed with GDI+ (ARGB-aware, alpha-composited),
 *          which satisfies the compositing requirement and maps cleanly onto the
 *          ARGB colour space used by ComputedStyle.  Text rendering is
 *          accelerated by GlyphCache (cached measurements) and TextBatch
 *          (grouped DrawString flushes that reuse fonts and cached widths).
 */
class Win32Painter : public Painter {
public:
    explicit Win32Painter(void* hdc);   // HDC, void* to avoid <windows.h> in header
    ~Win32Painter() override;

    Win32Painter(const Win32Painter&) = delete;
    Win32Painter& operator=(const Win32Painter&) = delete;

    /// @brief Begin a paint frame: start GDI+, create the Graphics surface.
    void begin();
    /// @brief End a paint frame: flush batched text, tear down GDI+.
    void end();

    // --- Painter HAL ---
    void fillRect(int x, int y, int width, int height, uint32_t color) override;
    void drawText(int x, int y, const std::string& text, uint32_t color) override;
    void drawBorder(int x, int y, int width, int height, uint32_t color, int thickness) override;

    /// @brief Paint an entire DOM document using a simple block/inline flow layout.
    void paintDocument(html::Document* doc, int viewportWidth, int scrollX = 0, int scrollY = 0);

    const GlyphCache& glyphCache() const { return glyphs_; }

private:
    struct Impl;
    Impl* impl_{nullptr};
    GlyphCache glyphs_;

    /// @brief Styled text submission (uses the per-element font descriptor).
    void drawTextStyled(int x, int y, const std::string& text, uint32_t color,
                        const std::string& family, float size, int weight, bool italic);

    /// @brief Recursive block/inline flow layout + paint for one node.
    void paintNode(html::DOMNode* node, float x, float contentWidth, float& cursorY);

    /// @brief Height (px) this node occupies in its parent's content flow, used
    ///        by the measure pass so backgrounds can be drawn behind children.
    float boxHeightOf(html::DOMNode* node, float contentWidth);
};
#endif // _WIN32

} // namespace render

#endif // RENDER_PAINTER_H
