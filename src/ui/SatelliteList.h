#pragma once

#include <string>
#include <vector>

namespace globe {

struct SatelliteState;
class UiContext;

// A searchable, scrollable panel of the whole catalog on the right edge. Type to
// filter by name or NORAD id, scroll the wheel while hovering to move through it,
// click a row to select that satellite. Immediate-mode: feed it input, call
// rebuild() each frame, then draw().
class SatelliteList {
public:
    void setViewport(int width, int height);

    void toggleVisible() { visible_ = !visible_; }
    [[nodiscard]] bool visible() const { return visible_; }

    void toggleMinimized() { minimized_ = !minimized_; }
    [[nodiscard]] bool minimized() const { return minimized_; }

    // True if (px,py) is on the panel's header bar (the collapse/expand strip).
    [[nodiscard]] bool hitHeader(double px, double py) const;

    // Search-box editing, fed from the window's text-entry queue.
    void appendText(const std::string& s);
    void backspace(int count);
    void clearFilter();
    [[nodiscard]] bool filterEmpty() const { return filter_.empty(); }

    [[nodiscard]] bool contains(double px, double py) const; // cursor over panel?
    void scroll(double wheelDelta);                          // scroll the rows

    // Rebuild the filtered list of catalog indices from the current filter.
    void rebuild(const std::vector<SatelliteState>& states);

    // Catalog index under the cursor, or -1 if the cursor isn't over a row.
    [[nodiscard]] int rowAt(double px, double py) const;

    // Draw the panel. `selectedIndex`/`hoverIndex` are catalog indices to
    // highlight (hover is usually rowAt(cursor)); `time` drives the caret blink.
    void draw(UiContext& ui, const std::vector<SatelliteState>& states,
              int selectedIndex, int hoverIndex, double time) const;

private:
    struct Layout {
        float x, y, w, h;   // panel rect (header only, when minimized)
        float headerH;      // top bar with the title + collapse control
        float rowsTop;      // first row's top (below header + search box)
        float rowsH;        // height of the scrollable rows area
        float rowH;         // per-row height
    };
    [[nodiscard]] Layout layout() const;
    void clampScroll();

    int screenW_ = 0;
    int screenH_ = 0;
    bool visible_ = true;
    bool minimized_ = false;
    std::string filter_;
    float scroll_ = 0.0f;
    std::vector<int> filtered_; // indices into the catalog's states()
};

} // namespace globe
