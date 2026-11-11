#include "ui/LocationPicker.h"

#include "domain/Gazetteer.h"
#include "ui/UiContext.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace globe {

namespace {
constexpr float kModalW = 440.0f;
constexpr float kModalH = 360.0f;
constexpr float kPad = 16.0f;
constexpr float kSearchH = 28.0f;
constexpr float kRowH = 24.0f;
constexpr float kHintH = 28.0f;
constexpr int kMaxResults = 80;
} // namespace

LocationPicker::Layout LocationPicker::layout() const {
    Layout L{};
    L.w = kModalW;
    L.h = kModalH;
    L.x = (static_cast<float>(screenW_) - L.w) * 0.5f;
    L.y = (static_cast<float>(screenH_) - L.h) * 0.5f;
    L.searchY = L.y + 40.0f;
    L.rowsTop = L.searchY + kSearchH + 8.0f;
    L.rowsH = (L.y + L.h - kHintH) - L.rowsTop;
    L.rowH = kRowH;
    return L;
}

void LocationPicker::setViewport(int width, int height) {
    screenW_ = width;
    screenH_ = height;
}

void LocationPicker::toggle() {
    open_ = !open_;
    if (open_) {
        query_.clear();
        scroll_ = 0.0f;
    }
}

void LocationPicker::open() {
    open_ = true;
    query_.clear();
    scroll_ = 0.0f;
}

void LocationPicker::appendText(const std::string& s) {
    if (!s.empty()) {
        query_ += s;
        scroll_ = 0.0f;
    }
}

void LocationPicker::backspace(int count) {
    for (int i = 0; i < count && !query_.empty(); ++i) {
        query_.pop_back();
    }
    if (count > 0) {
        scroll_ = 0.0f;
    }
}

bool LocationPicker::contains(double px, double py) const {
    if (!open_) {
        return false;
    }
    const Layout L = layout();
    return px >= L.x && px <= L.x + L.w && py >= L.y && py <= L.y + L.h;
}

void LocationPicker::scroll(double wheelDelta) {
    scroll_ -= static_cast<float>(wheelDelta) * kRowH * 3.0f;
    clampScroll();
}

void LocationPicker::clampScroll() {
    const Layout L = layout();
    const float contentH = static_cast<float>(results_.size()) * L.rowH;
    const float maxScroll = std::max(0.0f, contentH - L.rowsH);
    scroll_ = std::clamp(scroll_, 0.0f, maxScroll);
}

void LocationPicker::rebuild() {
    if (query_ == lastQuery_) {
        return; // results only change when the query changes
    }
    lastQuery_ = query_;
    results_ = gaz_ ? gaz_->search(query_, kMaxResults) : std::vector<const Place*>{};
    clampScroll();
}

int LocationPicker::rowAt(double px, double py) const {
    if (!open_) {
        return -1;
    }
    const Layout L = layout();
    if (px < L.x || px > L.x + L.w || py < L.rowsTop || py > L.rowsTop + L.rowsH) {
        return -1;
    }
    const float local = static_cast<float>(py) - L.rowsTop + scroll_;
    const int row = static_cast<int>(std::floor(local / L.rowH));
    if (row < 0 || row >= static_cast<int>(results_.size())) {
        return -1;
    }
    return row;
}

bool LocationPicker::parseLatLon(double& lat, double& lon) const {
    const std::size_t comma = query_.find(',');
    if (comma == std::string::npos) {
        return false;
    }
    try {
        const double la = std::stod(query_.substr(0, comma));
        const double lo = std::stod(query_.substr(comma + 1));
        if (la >= -90.0 && la <= 90.0 && lo >= -180.0 && lo <= 180.0) {
            lat = la;
            lon = lo;
            return true;
        }
    } catch (const std::exception&) {
    }
    return false;
}

void LocationPicker::draw(UiContext& ui, double cursorX, double cursorY, double time) const {
    if (!open_) {
        return;
    }
    const Layout L = layout();

    const glm::vec4 bg(0.06f, 0.08f, 0.12f, 0.97f);
    const glm::vec4 accent(0.35f, 0.85f, 1.0f, 1.0f);
    const glm::vec4 searchBg(0.10f, 0.13f, 0.18f, 1.0f);
    const glm::vec4 white(0.92f, 0.95f, 1.0f, 1.0f);
    const glm::vec4 dim(0.55f, 0.63f, 0.78f, 1.0f);
    const glm::vec4 hoverBg(1.0f, 1.0f, 1.0f, 0.08f);

    ui.drawRect(L.x, L.y, L.w, L.h, bg);
    ui.drawRect(L.x, L.y, L.w, 3.0f, accent);
    ui.drawText(L.x + kPad, L.y + 12.0f, "Set location  (type a city, or lat, lon)", white);

    // Search box.
    ui.drawRect(L.x + kPad, L.searchY, L.w - 2.0f * kPad, kSearchH, searchBg);
    const float tx = L.x + kPad + 10.0f;
    const float ty = L.searchY + 5.0f;
    if (query_.empty()) {
        ui.drawText(tx, ty, "e.g. Mumbai   or   19.07, 72.88",
                    dim);
    } else {
        ui.drawText(tx, ty, query_, white);
    }
    if (std::fmod(time, 1.0) < 0.5) {
        ui.drawRect(tx + ui.textWidth(query_) + 1.0f, ty, 1.5f, ui.lineHeight(), white);
    }

    // Results, clipped to the scrollable area.
    const int hover = rowAt(cursorX, cursorY);
    ui.pushClip(L.x, L.rowsTop, L.w, L.rowsH);
    char line[128];
    for (std::size_t r = 0; r < results_.size(); ++r) {
        const float rowY = L.rowsTop - scroll_ + static_cast<float>(r) * L.rowH;
        if (rowY + L.rowH < L.rowsTop || rowY > L.rowsTop + L.rowsH) {
            continue;
        }
        if (static_cast<int>(r) == hover) {
            ui.drawRect(L.x, rowY, L.w, L.rowH, hoverBg);
        }
        const Place* p = results_[r];
        std::snprintf(line, sizeof(line), "%s, %s", p->name.c_str(), p->country.c_str());
        ui.drawText(L.x + kPad, rowY + 3.0f, line, white);
    }
    ui.popClip();

    // Hint / empty-state at the bottom.
    const char* hint = nullptr;
    if (gaz_ && !gaz_->ready()) {
        hint = "City data not installed - type  lat, lon  instead";
    } else if (query_.empty()) {
        hint = "Esc to cancel";
    } else if (results_.empty()) {
        hint = "No match - or type  lat, lon  and press Enter";
    } else {
        hint = "Click a city, or Enter for the top match / typed lat, lon";
    }
    ui.drawText(L.x + kPad, L.y + L.h - kHintH + 6.0f, hint, dim);
}

} // namespace globe
