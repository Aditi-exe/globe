#include "ui/SatelliteList.h"

#include "domain/SatelliteCatalog.h"
#include "ui/UiContext.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

namespace globe {

namespace {
constexpr float kMargin = 16.0f;
constexpr float kPanelW = 300.0f;
constexpr float kSearchH = 30.0f;
constexpr float kRowH = 22.0f;
constexpr float kPad = 10.0f;

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
} // namespace

SatelliteList::Layout SatelliteList::layout() const {
    Layout L{};
    L.x = static_cast<float>(screenW_) - kPanelW - kMargin;
    L.y = kMargin;
    L.w = kPanelW;
    L.h = static_cast<float>(screenH_) - 2.0f * kMargin;
    L.rowsTop = L.y + kSearchH + 6.0f;
    L.rowsH = (L.y + L.h) - L.rowsTop;
    L.rowH = kRowH;
    return L;
}

void SatelliteList::setViewport(int width, int height) {
    screenW_ = width;
    screenH_ = height;
}

void SatelliteList::appendText(const std::string& s) {
    if (s.empty()) {
        return;
    }
    filter_ += s;
    scroll_ = 0.0f; // jump back to the top when the query changes
}

void SatelliteList::backspace(int count) {
    for (int i = 0; i < count && !filter_.empty(); ++i) {
        filter_.pop_back();
    }
    if (count > 0) {
        scroll_ = 0.0f;
    }
}

void SatelliteList::clearFilter() {
    filter_.clear();
    scroll_ = 0.0f;
}

bool SatelliteList::contains(double px, double py) const {
    if (!visible_) {
        return false;
    }
    const Layout L = layout();
    return px >= L.x && px <= L.x + L.w && py >= L.y && py <= L.y + L.h;
}

void SatelliteList::scroll(double wheelDelta) {
    scroll_ -= static_cast<float>(wheelDelta) * kRowH * 3.0f; // 3 rows per notch
    clampScroll();
}

void SatelliteList::clampScroll() {
    const Layout L = layout();
    const float contentH = static_cast<float>(filtered_.size()) * L.rowH;
    const float maxScroll = std::max(0.0f, contentH - L.rowsH);
    scroll_ = std::clamp(scroll_, 0.0f, maxScroll);
}

void SatelliteList::rebuild(const std::vector<SatelliteState>& states) {
    filtered_.clear();
    const std::string f = toLower(filter_);
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (f.empty()) {
            filtered_.push_back(static_cast<int>(i));
            continue;
        }
        const bool nameHit = toLower(states[i].name).find(f) != std::string::npos;
        const bool noradHit = std::to_string(states[i].norad).find(f) != std::string::npos;
        if (nameHit || noradHit) {
            filtered_.push_back(static_cast<int>(i));
        }
    }
    clampScroll();
}

int SatelliteList::rowAt(double px, double py) const {
    if (!visible_) {
        return -1;
    }
    const Layout L = layout();
    if (px < L.x || px > L.x + L.w || py < L.rowsTop || py > L.y + L.h) {
        return -1;
    }
    const float local = static_cast<float>(py) - L.rowsTop + scroll_;
    const int row = static_cast<int>(std::floor(local / L.rowH));
    if (row < 0 || row >= static_cast<int>(filtered_.size())) {
        return -1;
    }
    return filtered_[static_cast<std::size_t>(row)];
}

void SatelliteList::draw(UiContext& ui, const std::vector<SatelliteState>& states,
                         int selectedIndex, int hoverIndex, double time) const {
    if (!visible_) {
        return;
    }
    const Layout L = layout();

    const glm::vec4 panelBg(0.04f, 0.06f, 0.10f, 0.86f);
    const glm::vec4 searchBg(0.10f, 0.13f, 0.18f, 0.95f);
    const glm::vec4 white(0.92f, 0.95f, 1.0f, 1.0f);
    const glm::vec4 dim(0.55f, 0.63f, 0.78f, 1.0f);
    const glm::vec4 selBg(0.20f, 0.45f, 0.58f, 0.85f);
    const glm::vec4 hoverBg(1.0f, 1.0f, 1.0f, 0.08f);

    ui.drawRect(L.x, L.y, L.w, L.h, panelBg);

    // Search box.
    ui.drawRect(L.x + 6.0f, L.y + 6.0f, L.w - 12.0f, kSearchH - 10.0f, searchBg);
    const float tx = L.x + 6.0f + kPad;
    const float ty = L.y + 6.0f + 3.0f;
    if (filter_.empty()) {
        ui.drawText(tx, ty, "Type to search...", dim);
    } else {
        ui.drawText(tx, ty, filter_, white);
    }
    // Blinking caret just past the current text.
    if (std::fmod(time, 1.0) < 0.5) {
        const float caretX = tx + ui.textWidth(filter_) + 1.0f;
        ui.drawRect(caretX, ty, 1.5f, ui.lineHeight(), white);
    }

    // Right-aligned match count in the search row.
    char count[32];
    std::snprintf(count, sizeof(count), "%zu", filtered_.size());
    ui.drawText(L.x + L.w - 6.0f - kPad - ui.textWidth(count), ty, count, dim);

    // Rows, clipped to the scrollable area.
    ui.pushClip(L.x, L.rowsTop, L.w, L.rowsH);
    for (std::size_t r = 0; r < filtered_.size(); ++r) {
        const float rowY = L.rowsTop - scroll_ + static_cast<float>(r) * L.rowH;
        if (rowY + L.rowH < L.rowsTop || rowY > L.y + L.h) {
            continue; // off-screen; skip
        }
        const int catIdx = filtered_[r];
        if (catIdx == selectedIndex) {
            ui.drawRect(L.x, rowY, L.w, L.rowH, selBg);
        } else if (catIdx == hoverIndex) {
            ui.drawRect(L.x, rowY, L.w, L.rowH, hoverBg);
        }
        const glm::vec4& c = (catIdx == selectedIndex) ? white : dim;
        ui.drawText(L.x + kPad, rowY + 3.0f,
                    states[static_cast<std::size_t>(catIdx)].name, c);
    }
    ui.popClip();
}

} // namespace globe
