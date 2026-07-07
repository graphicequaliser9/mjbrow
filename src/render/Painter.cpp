/**
 * @file Painter.cpp
 * @brief Painter back-end: glyph cache, text batching, and GDI+/Win32 painter.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "render/Painter.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#include "html/DOMNode.h"
#include "css/ComputedStyle.h"
#endif

namespace render {

// ═══════════════════════════════════════════════════════════════════════════
//  Platform-independent pieces (compiled everywhere, unit-tested on Linux)
// ═══════════════════════════════════════════════════════════════════════════

// (GlyphCache / TextBatch are header-only template-free; no out-of-line code
//  is required for the cross-platform build.  Win32Painter below is the only
//  translation-unit body, and it is guarded by _WIN32.)

#ifdef _WIN32

namespace {

using namespace Gdiplus;

// ── small helpers ───────────────────────────────────────────────────────────
std::wstring toWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

inline ARGB packARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<ARGB>(a) << 24) | (static_cast<ARGB>(r) << 16) |
           (static_cast<ARGB>(g) << 8)  | (static_cast<ARGB>(b));
}

inline uint8_t alphaOf(uint32_t c) { return static_cast<uint8_t>((c >> 24) & 0xFF); }
inline uint8_t redOf(uint32_t c)   { return static_cast<uint8_t>((c >> 16) & 0xFF); }
inline uint8_t greenOf(uint32_t c) { return static_cast<uint8_t>((c >> 8)  & 0xFF); }
inline uint8_t blueOf(uint32_t c)  { return static_cast<uint8_t>(c & 0xFF); }

// ── float→int rounding ───────────────────────────────────────────────────────
inline int iround(float v) { return static_cast<int>(std::floor(v + 0.5f)); }

// ── DOM flow-layout predicates ───────────────────────────────────────────────
bool isBlockTag(const std::string& tag) {
    static const char* kBlocks[] = {
        "html","body","div","p","section","article","header","footer","main",
        "nav","ul","ol","li","h1","h2","h3","h4","h5","h6","blockquote","pre",
        "table","thead","tbody","tr","td","th","form","hr","address","dl","dt","dd"
    };
    for (const char* b : kBlocks) if (tag == b) return true;
    return false;
}

html::DOMNode* findBody(html::DOMNode* root) {
    if (!root) return nullptr;
    if (root->nodeType == html::NodeType::Element && root->tagName == "body")
        return root;
    for (html::DOMNode* c = root->firstChild; c; c = c->nextSibling) {
        if (html::DOMNode* found = findBody(c)) return found;
    }
    return nullptr;
}

} // anonymous namespace

// ── pImpl holding all GDI+ state (keeps <gdiplus.h> out of the header) ───────
struct Win32Painter::Impl {
    HDC             hdc{nullptr};
    Graphics*       g{nullptr};
    ULONG_PTR       token{0};
    bool            started{false};
    TextBatch       batch;

    // Cached font objects keyed by descriptor (avoids per-run Font allocation).
    struct FontKey {
        std::string family; int size{0}; int weight{400}; bool italic{false};
        bool operator==(const FontKey& o) const {
            return family == o.family && size == o.size &&
                   weight == o.weight && italic == o.italic;
        }
    };
    struct FontKeyHash {
        size_t operator()(const FontKey& k) const {
            size_t h = std::hash<std::string>()(k.family);
            h ^= std::hash<int>()(k.size)  + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(k.weight)+ 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>()(k.italic)+ 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<FontKey, std::unique_ptr<Font>, FontKeyHash> fonts;

    StringFormat* runFmt{nullptr};  // NoWrap / NoClip for single-run measurement+draw

    Impl() {
        runFmt = new StringFormat(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    }
    ~Impl() {
        delete runFmt;
        delete g;
    }

    Font* getFont(const std::string& family, float size, int weight, bool italic) {
        FontKey k{family, iround(size * 100.0f), weight, italic};
        auto it = fonts.find(k);
        if (it != fonts.end()) return it->second.get();

        int style = FontStyleRegular;
        if (italic)          style |= FontStyleItalic;
        if (weight >= 700)   style |= FontStyleBold;

        std::wstring wf = toWide(family.empty() ? "Arial" : family);
        Font* f = new Font(wf.c_str(), size, style, UnitPixel);
        Font*& slot = fonts[k];
        slot.reset(f);
        return f;
    }

    std::pair<float, float> measure(const std::string& text,
                                    const std::string& family,
                                    float size, int weight, bool italic) {
        if (text.empty() || !g) return {0.0f, 0.0f};
        Font* f = getFont(family, size, weight, italic);
        std::wstring w = toWide(text);
        SizeF sz;
        g->MeasureString(w.c_str(), static_cast<INT>(w.length()), f,
                         RectF(0, 0, 1.0e7f, 1.0e7f), runFmt, &sz);
        return {sz.Width, sz.Height};
    }

    void flushText(Win32Painter& self) {
        (void)self;
        if (batch.empty() || !g) { batch.clear(); return; }
        batch.forEachBatch([this](size_t start, size_t end) {
            const TextRun& r0 = batch.run(start);
            SolidBrush brush(packARGB(alphaOf(r0.color), redOf(r0.color),
                                       greenOf(r0.color), blueOf(r0.color)));
            Font* f = getFont(r0.fontFamily, r0.fontSize, r0.fontWeight, r0.italic);
            for (size_t i = start; i < end; ++i) {
                const TextRun& r = batch.run(i);
                std::wstring w = toWide(r.text);
                g->DrawString(w.c_str(), static_cast<INT>(w.length()), f,
                              PointF(static_cast<REAL>(r.x), static_cast<REAL>(r.y)),
                              runFmt, &brush);
            }
        });
        batch.clear();
    }
};

// ── Win32Painter lifecycle ───────────────────────────────────────────────────
Win32Painter::Win32Painter(void* hdc)
    : impl_(new Impl())
    , glyphs_([this](const std::string& t, const std::string& fam, float s, int w, bool it) {
          return impl_->measure(t, fam, s, w, it);
      })
{
    impl_->hdc = static_cast<HDC>(hdc);
}

Win32Painter::~Win32Painter() {
    if (impl_) {
        impl_->flushText(*this);
        if (impl_->started) {
            GdiplusShutdown(impl_->token);
            impl_->started = false;
        }
    }
    delete impl_;
}

void Win32Painter::begin() {
    if (impl_->started) return;
    GdiplusStartupInput inp;
    GdiplusStartup(&impl_->token, &inp, nullptr);
    impl_->started = true;
    impl_->g = new Graphics(impl_->hdc);
    impl_->g->SetSmoothingMode(SmoothingModeAntiAlias);
    impl_->g->SetTextRenderingHint(TextRenderingHintAntiAlias);
}

void Win32Painter::end() {
    if (!impl_) return;
    impl_->flushText(*this);
    if (impl_->started) {
        GdiplusShutdown(impl_->token);
        impl_->started = false;
    }
    delete impl_->g;
    impl_->g = nullptr;
}

// ── Painter HAL primitives ───────────────────────────────────────────────────
void Win32Painter::fillRect(int x, int y, int w, int h, uint32_t color) {
    if (!impl_->g) return;
    if (alphaOf(color) == 0 || w <= 0 || h <= 0) return;  // skip fully transparent
    SolidBrush brush(packARGB(alphaOf(color), redOf(color),
                              greenOf(color), blueOf(color)));
    impl_->g->FillRectangle(&brush, x, y, w, h);
}

void Win32Painter::drawBorder(int x, int y, int w, int h, uint32_t color, int thickness) {
    if (!impl_->g || thickness <= 0 || alphaOf(color) == 0) return;
    Pen pen(packARGB(alphaOf(color), redOf(color), greenOf(color), blueOf(color)),
            static_cast<REAL>(thickness));
    // Inset by half the pen width so the stroke stays inside the box edge.
    const REAL off = static_cast<REAL>(thickness) * 0.5f;
    impl_->g->DrawRectangle(&pen, x + off, y + off,
                            static_cast<REAL>(w) - thickness,
                            static_cast<REAL>(h) - thickness);
}

void Win32Painter::drawText(int x, int y, const std::string& text, uint32_t color) {
    if (text.empty()) return;
    TextRun r;
    r.x = x; r.y = y; r.text = text; r.color = color;
    impl_->batch.add(r);
}

// ── styled text (used by the DOM walk; carries font descriptor) ──────────────
void Win32Painter::drawTextStyled(int x, int y, const std::string& text, uint32_t color,
                                  const std::string& family, float size, int weight, bool italic) {
    if (text.empty()) return;
    TextRun r;
    r.x = x; r.y = y; r.text = text; r.color = color;
    r.fontFamily = family; r.fontSize = size; r.fontWeight = weight; r.italic = italic;
    impl_->batch.add(r);
}

// ── DOM flow layout + paint ───────────────────────────────────────────────────
namespace {

// Effective ARGB after applying an element's opacity (premultiply alpha).
uint32_t applyOpacity(uint32_t c, float opacity) {
    if (opacity >= 1.0f) return c;
    int a = static_cast<int>(alphaOf(c) * opacity);
    return packARGB(static_cast<uint8_t>(a), redOf(c), greenOf(c), blueOf(c));
}

// Number of wrapped lines a text node occupies within contentWidth, using the
// glyph cache for word widths.  Shared by the measure pass (boxHeightOf) and the
// draw pass (paintNode) so the two stay in lock-step for correct cursor advance.
int textLineCount(const std::string& text, const std::string& family, float size,
                  float lineH, float contentWidth, render::GlyphCache& glyphs,
                  int weight, bool italic) {
    if (text.empty() || contentWidth <= 0.0f) return 0;
    int    lines = 1;
    int    words = 0;
    float  penX  = 0.0f;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (i >= text.size()) break;
        size_t j = i;
        while (j < text.size() && !std::isspace(static_cast<unsigned char>(text[j]))) ++j;
        std::string word = text.substr(i, j - i);
        i = j;
        ++words;
        float wW = glyphs.measure(word, family, size, weight, italic).first;
        if (penX > 0.0f && penX + wW > contentWidth) { penX = 0.0f; ++lines; }
        penX += wW + size * 0.25f;
    }
    (void)lineH;
    return words ? lines : 0;
}

} // anonymous namespace

// ── measure pass ──────────────────────────────────────────────────────────────
float Win32Painter::boxHeightOf(html::DOMNode* node, float contentWidth) {
    if (!node) return 0.0f;

    if (node->nodeType == html::NodeType::Text) {
        const css::ComputedStyle* s = node->style;
        std::string family = s ? s->fontFamily : "Arial";
        float size   = s ? s->fontSize : 16.0f;
        int   weight = s ? s->fontWeight : 400;
        bool  italic = false;
        float lineH  = size * (s ? s->lineHeight : 1.2f);
        return textLineCount(node->textContent, family, size, lineH,
                             contentWidth, glyphs_, weight, italic) * lineH;
    }

    if (node->nodeType != html::NodeType::Element) return 0.0f;

    const css::ComputedStyle* s = node->style;
    float ml = s ? s->marginLeft   : 0.0f;
    float mr = s ? s->marginRight  : 0.0f;
    float bl = s ? s->borderLeft   : 0.0f;
    float br = s ? s->borderRight  : 0.0f;
    float bt = s ? s->borderTop    : 0.0f;
    float bb = s ? s->borderBottom : 0.0f;
    float pl = s ? s->paddingLeft   : 0.0f;
    float pr = s ? s->paddingRight  : 0.0f;
    float pb = s ? s->paddingBottom : 0.0f;

    float avail = contentWidth - ml - mr;
    float innerW = avail - bl - br - pl - pr;
    if (s && s->width > 0.0f) innerW = s->width;
    if (innerW < 0.0f) innerW = 0.0f;

    float contentH = 0.0f;
    for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling) {
        const css::ComputedStyle* cs = c->style;
        float cmt = cs ? cs->marginTop    : 0.0f;
        float cmb = cs ? cs->marginBottom : 0.0f;
        contentH += cmt + boxHeightOf(c, innerW) + cmb;
    }
    return bt + pt + contentH + pb + bb;
}

// ── draw pass ─────────────────────────────────────────────────────────────────
void Win32Painter::paintNode(html::DOMNode* node, float x, float contentWidth, float& cursorY) {
    if (!node || !impl_->g) return;

    if (node->nodeType == html::NodeType::Text) {
        // Inline text: wrap words within [x, x+contentWidth].
        const css::ComputedStyle* s = node->style;
        std::string family = s ? s->fontFamily : "Arial";
        float size   = s ? s->fontSize : 16.0f;
        int   weight = s ? s->fontWeight : 400;
        bool  italic = false;
        uint32_t color = s ? applyOpacity(s->color, s->opacity) : 0xFF000000;
        float lineH = size * (s ? s->lineHeight : 1.2f);

        float penX = x;
        float y    = cursorY;
        const std::string& text = node->textContent;
        size_t i = 0;
        bool any = false;
        while (i < text.size()) {
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
            if (i >= text.size()) break;
            size_t j = i;
            while (j < text.size() && !std::isspace(static_cast<unsigned char>(text[j]))) ++j;
            std::string word = text.substr(i, j - i);
            i = j;
            any = true;

            float wW = glyphs_.measure(word, family, size, weight, italic).first;
            if (penX > x && penX + wW > x + contentWidth) {
                penX = x;            // wrap
                y += lineH;
            }
            drawTextStyled(iround(penX), iround(y), word, color, family, size, weight, italic);
            penX += wW + size * 0.25f;   // word + inter-word space
        }
        if (any) cursorY = y + lineH;   // advance by the used height
        return;
    }

    if (node->nodeType != html::NodeType::Element) return;

    const css::ComputedStyle* s = node->style;
    float ml = s ? s->marginLeft   : 0.0f;
    float mt = s ? s->marginTop    : 0.0f;
    float mr = s ? s->marginRight  : 0.0f;
    float mb = s ? s->marginBottom : 0.0f;
    float bl = s ? s->borderLeft   : 0.0f;
    float bt = s ? s->borderTop    : 0.0f;
    float br = s ? s->borderRight  : 0.0f;
    float bb = s ? s->borderBottom : 0.0f;
    float pl = s ? s->paddingLeft   : 0.0f;
    float pt = s ? s->paddingTop    : 0.0f;
    float pr = s ? s->paddingRight  : 0.0f;
    float pb = s ? s->paddingBottom : 0.0f;

    float boxX        = x + ml;
    float marginBoxTop = cursorY + mt;
    float avail       = contentWidth - ml - mr;
    float innerW      = avail - bl - br - pl - pr;
    if (s && s->width > 0.0f) innerW = s->width;
    if (innerW < 0.0f) innerW = 0.0f;

    // Measure first so the background/border (behind children) can be sized.
    float boxH = boxHeightOf(node, contentWidth);

    // Parent background + border are painted BEFORE children (correct z-order:
    // a child's background / text then composites on top of the parent).
    uint32_t bg = s ? applyOpacity(s->backgroundColor, s->opacity) : 0x00000000;
    if (alphaOf(bg) > 0) {
        fillRect(iround(boxX), iround(marginBoxTop), iround(avail), iround(boxH), bg);
    }
    bool hasBorder = (bl > 0.0f || bt > 0.0f || br > 0.0f || bb > 0.0f);
    if (hasBorder) {
        uint32_t bc = s ? applyOpacity(s->color, s->opacity) : 0xFF000000;
        int thick = iround(std::max({bl, bt, br, bb}));
        drawBorder(iround(boxX), iround(marginBoxTop), iround(avail), iround(boxH), bc, thick);
    }

    // Draw children on top, within the content box.
    float childX = boxX + bl + pl;
    float cc     = marginBoxTop + bt + pt;
    for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling) {
        const css::ComputedStyle* cs = c->style;
        cc += cs ? cs->marginTop : 0.0f;
        paintNode(c, childX, innerW, cc);
        cc += cs ? cs->marginBottom : 0.0f;
    }

    cursorY = marginBoxTop + boxH + mb;
}

void Win32Painter::paintDocument(html::Document* doc, int viewportWidth, int scrollX, int scrollY) {
    if (!doc || !impl_->g) return;
    html::DOMNode* root = findBody(doc);
    if (!root) root = doc;
    float cursorY = static_cast<float>(-scrollY);
    paintNode(root, static_cast<float>(-scrollX), static_cast<float>(viewportWidth), cursorY);
}

#endif // _WIN32

} // namespace render
