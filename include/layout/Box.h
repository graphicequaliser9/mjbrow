/**
 * @file Layout.h
 * @brief Virtual-scroll table layout engine.
 * @details Provides a memory-bounded layout engine for very large HTML tables.
 *          Only boxes for rows intersecting the current viewport are ever
 *          materialised, so layout memory stays O(visibleRows x columns)
 *          instead of O(allRows x columns).  Row heights are measured once
 *          from a single template row and replicated, enabling virtual
 *          scrolling over tables with millions of cells.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_BOX_H
#define LAYOUT_BOX_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace html { class DOMNode; }

namespace layout {

/**
 * @struct LayoutBox
 * @brief A single positioned, paint-ready rectangle for one (possibly merged)
 *        table cell.  Kept intentionally small: 48 bytes on 64-bit targets.
 */
struct LayoutBox {
    float x{0.0f};            ///< Left edge in table-content coordinates (px).
    float y{0.0f};            ///< Top edge in table-content coordinates (px).
    float width{0.0f};        ///< Box width (px), spanning colSpan columns.
    float height{0.0f};       ///< Box height (px), spanning rowSpan rows.
    html::DOMNode* node{nullptr}; ///< Source <td>/<th> element (text source).
    int row{0};               ///< Anchor (logical) row index.
    int col{0};               ///< Starting column index.
    int colSpan{1};           ///< Number of columns this box covers.
    int rowSpan{1};           ///< Number of rows this box covers.
    bool isHeader{false};     ///< True for <th> cells.
};

/**
 * @class TableLayoutEngine
 * @brief Layout engine for one HTML <table> element.
 *
 * The engine is split into two phases:
 *   1. prepare()  – build persistent, O(rows + cols) metadata: an index of row
 *                   node pointers, resolved column widths/x-offsets, and a
 *                   single template row height.  This never allocates one box
 *                   per cell.
 *   2. layout()   – given a viewport, materialise LayoutBoxes for the rows that
 *                   intersect it (plus a small seed/overscan band).  The number
 *                   of boxes is capped at O(visibleRows x columns).
 */
class TableLayoutEngine {
public:
    TableLayoutEngine() = default;
    ~TableLayoutEngine() = default;

    TableLayoutEngine(const TableLayoutEngine&) = delete;
    TableLayoutEngine& operator=(const TableLayoutEngine&) = delete;

    /// @brief Build persistent table metadata from a <table> DOM node.
    /// @return true if a usable table (>=1 row, >=1 column) was found.
    bool prepare(html::DOMNode* table);

    /// @brief (Re)compute visible boxes for the supplied viewport.
    /// @param viewportTop     Top of the visible band in table-content px.
    /// @param viewportHeight  Height of the visible band in px.
    void layout(double viewportTop, double viewportHeight);

    // ── results / accessors ──────────────────────────────────────────────────
    const std::vector<LayoutBox>& boxes() const { return boxes_; }

    int rowCount() const { return rowCount_; }
    int columnCount() const { return columnCount_; }
    float rowHeight() const { return rowHeight_; }
    float totalHeight() const { return rowHeight_ * static_cast<float>(rowCount_); }
    float totalWidth() const { return totalWidth_; }

    /// @brief Approximate bytes of layout memory owned by this engine.
    size_t layoutMemoryBytes() const;

    /// @brief Theoretical maximum boxes for the last layout() call's viewport.
    size_t maxBoxesForLastLayout() const { return boxCapacity_; }

    // ── synthetic construction (used by large-table benchmarks) ───────────────
    struct Spec {
        int rows{0};
        int cols{0};
        float colWidth{100.0f};
        float rowHeight{22.0f};
    };

    /// @brief Build a uniform, span-free table directly from a spec.
    void prepareFromSpec(const Spec& spec);

private:
    void collectRows(html::DOMNode* node, std::vector<html::DOMNode*>& out);
    void computeColumnWidths();
    void measureRowHeight();

    static int parseFirstInt(const std::string& s);

    struct SpanInfo {
        bool active{false};
        int  endRow{0};       ///< Exclusive row the span stops covering.
        int  anchorRow{0};    ///< Row the span was declared on.
        int  colSpan{1};
        html::DOMNode* node{nullptr};
        bool header{false};
        bool emitted{false};  ///< Already produced a LayoutBox in the window.
    };

    void processRow(int r, bool emit);
    void emitCell(html::DOMNode* cell, int row, int col, int colSpan, int rowSpan, bool header);
    void emitSpan(const SpanInfo& sp, int row);

    html::DOMNode* table_{nullptr};
    std::vector<html::DOMNode*> rowNodes_;  ///< O(rows) pointer index.
    std::vector<float> colX_;               ///< Column left edges (size cols+1).
    std::vector<float> colWidth_;           ///< Column widths (size cols).
    int rowCount_{0};
    int columnCount_{0};
    float rowHeight_{22.0f};
    float totalWidth_{0.0f};

    // Per-layout pass state.
    std::vector<LayoutBox> boxes_;
    std::vector<SpanInfo> spanState_;       ///< Indexed by anchor column.
    std::vector<bool>     colCovered_;      ///< Column covered by an active span.
    std::vector<bool>     occupiedRow_;     ///< Column already filled this row.
    size_t boxCapacity_{0};
    int firstRow_{0};

    static constexpr int kSeedRows = 8;     ///< Rows read above viewport to seed spans.
    static constexpr int kOverscan = 4;     ///< Extra rows below viewport.
};

/**
 * @class Box
 * @brief Top-level layout entry point.  Discovers <table> elements in a DOM
 *        subtree and maintains a TableLayoutEngine per table.
 */
class Box {
public:
    Box() = default;
    ~Box() = default;

    Box(const Box&) = delete;
    Box& operator=(const Box&) = delete;

    /// @brief Scan root for tables and prepare + perform an initial layout.
    void layout(html::DOMNode* root);

    /// @brief Re-run layout for all owned tables with the given viewport.
    void setViewport(double scrollY, double viewportHeight);

    int tableCount() const { return static_cast<int>(tables_.size()); }
    const TableLayoutEngine* table(int i) const;
    TableLayoutEngine* table(int i);

private:
    struct Holder {
        html::DOMNode* node{nullptr};
        std::unique_ptr<TableLayoutEngine> engine;
    };
    std::vector<Holder> tables_;
    double viewportTop_{0.0};
    double viewportHeight_{600.0};
};

} // namespace layout

#endif // LAYOUT_BOX_H
