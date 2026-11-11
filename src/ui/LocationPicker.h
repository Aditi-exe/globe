#pragma once

#include <string>
#include <vector>

namespace globe {

struct Place;
class Gazetteer;
class UiContext;

// A centered modal for choosing the observer's location: type a city name to
// search the gazetteer and click a result, or type raw "lat, lon". Immediate-
// mode, like SatelliteList: feed input, rebuild() each frame, then draw().
class LocationPicker {
public:
    void setViewport(int width, int height);
    void setGazetteer(const Gazetteer* g) { gaz_ = g; }

    void toggle();
    void open();
    void close() { open_ = false; }
    [[nodiscard]] bool isOpen() const { return open_; }

    void appendText(const std::string& s);
    void backspace(int count);

    [[nodiscard]] bool contains(double px, double py) const; // over the modal?
    void scroll(double wheelDelta);

    void rebuild(); // recompute the results when the query changed

    // Result row under the cursor (index into results()), or -1.
    [[nodiscard]] int rowAt(double px, double py) const;

    // If the typed text is a valid "lat, lon", fill them and return true.
    [[nodiscard]] bool parseLatLon(double& lat, double& lon) const;

    [[nodiscard]] const std::vector<const Place*>& results() const { return results_; }

    void draw(UiContext& ui, double cursorX, double cursorY, double time) const;

private:
    struct Layout {
        float x, y, w, h; // modal rect
        float searchY;    // search box top
        float rowsTop;    // first result row top
        float rowsH;      // scrollable results height
        float rowH;
    };
    [[nodiscard]] Layout layout() const;
    void clampScroll();

    const Gazetteer* gaz_ = nullptr;
    int screenW_ = 0;
    int screenH_ = 0;
    bool open_ = false;
    std::string query_;
    std::string lastQuery_ = "\x01"; // force a first rebuild
    float scroll_ = 0.0f;
    std::vector<const Place*> results_;
};

} // namespace globe
