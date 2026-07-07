/**
 * @file PainterTest.cpp
 * @brief Cross-platform unit tests for the Painter glyph cache + text batching.
 * @details These verify the platform-independent rendering-acceleration logic
 *          (GlyphCache hit/miss policy and TextBatch grouping) that the GDI+
 *          Win32Painter relies on.  Runs on any toolchain — no Win32 needed.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "render/Painter.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  ok   - %s\n", msg);
    } else {
        std::printf("  FAIL - %s\n", msg);
        ++g_failures;
    }
}

// Deterministic mock measurer: width = chars * size * 0.5, height = size.
struct MockMeasurer {
    int calls{0};
    std::pair<float, float> operator()(const std::string& text,
                                       const std::string& /*family*/,
                                       float size, int /*weight*/, bool /*italic*/) {
        ++calls;
        return {static_cast<float>(text.size()) * size * 0.5f, size};
    }
};

} // namespace

int main() {
    std::printf("=== Painter glyph-cache + text-batching tests ===\n");

    // ── GlyphCache: repeated strings hit the cache (no re-measure) ────────────
    {
        MockMeasurer m;
        render::GlyphCache cache([&m](const std::string& t, const std::string& f,
                                      float s, int w, bool i) { return m(t, f, s, w, i); });

        auto a1 = cache.measure("Hello", "Arial", 16.0f, 400, false);
        auto a2 = cache.measure("Hello", "Arial", 16.0f, 400, false);
        check(a1 == a2, "identical repeated string returns identical extents");
        check(cache.misses() == 1, "first measure is a miss");
        check(cache.hits()   == 1, "second identical measure is a hit");
        check(m.calls        == 1, "measurer invoked exactly once for repeated string");

        // Different weight → distinct cache entry, new miss.
        auto b = cache.measure("Hello", "Arial", 16.0f, 700, false);
        check(b == a1, "same glyphs but bold still measures to same width (mock)");
        check(cache.misses() == 2, "bold 'Hello' is a separate miss");
        check(cache.size()   == 2, "cache holds two distinct entries");

        // Clear resets accounting.
        cache.clear();
        check(cache.size() == 0 && cache.hits() == 0 && cache.misses() == 0,
              "clear() empties the cache and resets counters");
    }

    // ── TextBatch: groups consecutive same-style runs, never crosses a style ──
    {
        render::TextBatch batch;
        auto mk = [](const std::string& key) {
            render::TextRun r;
            r.text = "x";
            // Force a style signature by varying colour per key.
            if (key == "A")      r.color = 0xFF000000;
            else if (key == "B") r.color = 0xFF0000FF;
            else                 r.color = 0xFF00FF00;
            return r;
        };
        batch.add(mk("A"));
        batch.add(mk("A"));
        batch.add(mk("B"));
        batch.add(mk("A"));

        std::vector<std::pair<size_t, size_t>> ranges;
        batch.forEachBatch([&](size_t s, size_t e) { ranges.emplace_back(s, e); });

        check(ranges.size() == 3, "A,A,B,A splits into 3 contiguous batches");
        check(ranges[0] == std::make_pair<size_t,size_t>(0, 2), "batch 0 covers runs [0,2)");
        check(ranges[1] == std::make_pair<size_t,size_t>(2, 3), "batch 1 covers run  [2,3)");
        check(ranges[2] == std::make_pair<size_t,size_t>(3, 4), "batch 2 covers run  [3,4)");

        // Every run inside a batch must share the same style signature.
        bool consistent = true;
        for (auto& rng : ranges) {
            std::string k = batch.run(rng.first).styleKey();
            for (size_t i = rng.first; i < rng.second; ++i)
                if (batch.run(i).styleKey() != k) { consistent = false; break; }
        }
        check(consistent, "all runs within a batch share the same style key");
        check(batch.run(0).styleKey() == batch.run(1).styleKey(), "adjacent A runs share style key");
        check(batch.run(0).styleKey() != batch.run(2).styleKey(), "A and B runs differ in style key");

        batch.clear();
        check(batch.empty(), "clear() empties the batch");
    }

    if (g_failures == 0) {
        std::printf("ALL PAINTER CHECKS PASSED\n");
        return 0;
    }
    std::printf("%d PAINTER CHECK(S) FAILED\n", g_failures);
    return 1;
}
