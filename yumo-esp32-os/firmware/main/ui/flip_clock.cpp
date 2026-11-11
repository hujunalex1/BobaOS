#include "flip_clock.hpp"
#include "theme.hpp"
#include "compact_ui.hpp"
#include "core/battery_mgr.hpp"
#include <cstdio>

namespace ui {

namespace c = compact;
namespace t = theme;

void FlipClock::create(lv_obj_t *parent) {
    if (area) return;

    area = c::panel(parent, 240, 240);
    lv_obj_set_pos(area, 0, 0);
    lv_obj_set_style_pad_all(area, 0, 0);
    lv_obj_set_style_radius(area, 0, 0);
    lv_obj_set_style_border_width(area, 0, 0);
    lv_obj_set_style_bg_color(area, lv_color_hex(0x0D0F14), 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_COVER, 0);

    // --- 1. Header Row (y = 8) ---
    // AM/PM tag
    labelAmPm = c::label(area, "PM", 40);
    lv_obj_set_pos(labelAmPm, 18, 10);
    lv_obj_set_style_text_font(labelAmPm, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(labelAmPm, lv_color_hex(0xFADB14), 0);

    // Date
    labelDate = c::label(area, "11.11 WED", 110);
    lv_obj_set_pos(labelDate, 64, 10);
    lv_obj_set_style_text_align(labelDate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(labelDate, lv_color_hex(0x8C9AA8), 0);

    // BOBA pill
    auto brandPill = c::panel(area, 44, 20);
    lv_obj_set_pos(brandPill, 178, 8);
    lv_obj_set_style_pad_all(brandPill, 0, 0);
    lv_obj_set_style_radius(brandPill, 4, 0);
    lv_obj_set_style_bg_color(brandPill, lv_color_hex(0x1E222D), 0);
    lv_obj_set_style_border_width(brandPill, 1, 0);
    lv_obj_set_style_border_color(brandPill, lv_color_hex(0x333C4E), 0);
    labelBrand = c::label(brandPill, "BOBA", 44);
    lv_obj_center(labelBrand);
    lv_obj_set_style_text_align(labelBrand, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(labelBrand, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(labelBrand, lv_color_hex(0x94A3B8), 0);

    // Helper to build a flip leaf with top/bottom shades and metal hinge rivets
    auto buildLeaf = [this](int x, int y, lv_obj_t *&leafCard, lv_obj_t *&leafLabel) {
        leafCard = c::panel(area, 94, 126);
        lv_obj_set_pos(leafCard, x, y);
        lv_obj_set_style_pad_all(leafCard, 0, 0);
        lv_obj_set_style_radius(leafCard, 10, 0);
        lv_obj_set_style_bg_color(leafCard, lv_color_hex(0x161922), 0);
        lv_obj_set_style_border_width(leafCard, 1, 0);
        lv_obj_set_style_border_color(leafCard, lv_color_hex(0x334155), 0);
        lv_obj_set_style_clip_corner(leafCard, true, 0);

        // Top half shade
        auto topHalf = c::panel(leafCard, 94, 63);
        lv_obj_set_pos(topHalf, 0, 0);
        lv_obj_set_style_pad_all(topHalf, 0, 0);
        lv_obj_set_style_radius(topHalf, 0, 0);
        lv_obj_set_style_bg_color(topHalf, lv_color_hex(0x1F2433), 0);
        lv_obj_set_style_border_width(topHalf, 0, 0);

        // Horizontal crease / gap
        auto crease = c::panel(leafCard, 94, 2);
        lv_obj_set_pos(crease, 0, 62);
        lv_obj_set_style_pad_all(crease, 0, 0);
        lv_obj_set_style_radius(crease, 0, 0);
        lv_obj_set_style_bg_color(crease, lv_color_hex(0x0A0B0E), 0);
        lv_obj_set_style_border_width(crease, 0, 0);

        // Left hinge tab
        auto hLeft = c::panel(leafCard, 4, 8);
        lv_obj_set_pos(hLeft, 0, 59);
        lv_obj_set_style_pad_all(hLeft, 0, 0);
        lv_obj_set_style_radius(hLeft, 2, 0);
        lv_obj_set_style_bg_color(hLeft, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_border_width(hLeft, 0, 0);

        // Right hinge tab
        auto hRight = c::panel(leafCard, 4, 8);
        lv_obj_set_pos(hRight, 90, 59);
        lv_obj_set_style_pad_all(hRight, 0, 0);
        lv_obj_set_style_radius(hRight, 2, 0);
        lv_obj_set_style_bg_color(hRight, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_border_width(hRight, 0, 0);

        // Center digits
        leafLabel = c::label(leafCard, "00", 94);
        lv_obj_center(leafLabel);
        lv_obj_set_style_text_align(leafLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(leafLabel, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(leafLabel, lv_color_hex(0xF1F5F9), 0);
    };

    // --- 2. Main Flip Leaves (y = 44) ---
    buildLeaf(16, 44, hourCard, hourLabel);

    // Colon dots
    colonDot1 = c::panel(area, 6, 6);
    lv_obj_set_pos(colonDot1, 117, 92);
    lv_obj_set_style_radius(colonDot1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(colonDot1, lv_color_hex(0x4F46E5), 0);
    lv_obj_set_style_border_width(colonDot1, 0, 0);

    colonDot2 = c::panel(area, 6, 6);
    lv_obj_set_pos(colonDot2, 117, 112);
    lv_obj_set_style_radius(colonDot2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(colonDot2, lv_color_hex(0x4F46E5), 0);
    lv_obj_set_style_border_width(colonDot2, 0, 0);

    buildLeaf(128, 44, minuteCard, minuteLabel);

    // --- 3. Bottom Row (y = 186) ---
    // Left: Seconds Box
    secBox = c::panel(area, 76, 36);
    lv_obj_set_pos(secBox, 16, 186);
    lv_obj_set_style_pad_all(secBox, 0, 0);
    lv_obj_set_style_radius(secBox, 8, 0);
    lv_obj_set_style_bg_color(secBox, lv_color_hex(0x161922), 0);
    lv_obj_set_style_border_width(secBox, 1, 0);
    lv_obj_set_style_border_color(secBox, lv_color_hex(0x293548), 0);

    secLabel = c::label(secBox, "SEC 00", 76);
    lv_obj_center(secLabel);
    lv_obj_set_style_text_align(secLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(secLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(secLabel, lv_color_hex(0x10B981), 0);

    // Right: Battery & Status Box
    infoBox = c::panel(area, 118, 36);
    lv_obj_set_pos(infoBox, 104, 186);
    lv_obj_set_style_pad_all(infoBox, 0, 0);
    lv_obj_set_style_radius(infoBox, 8, 0);
    lv_obj_set_style_bg_color(infoBox, lv_color_hex(0x161922), 0);
    lv_obj_set_style_border_width(infoBox, 1, 0);
    lv_obj_set_style_border_color(infoBox, lv_color_hex(0x293548), 0);

    infoLabel = c::label(infoBox, LV_SYMBOL_BATTERY_FULL " 95%", 118);
    lv_obj_center(infoLabel);
    lv_obj_set_style_text_align(infoLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(infoLabel, t::fontSmall(), 0);
    lv_obj_set_style_text_color(infoLabel, lv_color_hex(0x94A3B8), 0);
}

void FlipClock::show(lv_obj_t *parent) {
    if (!area) create(parent);
    lv_obj_set_hidden(area, false);
    lastHour = lastMin = lastSec = lastDay = -1;
}

void FlipClock::hide() {
    if (area) lv_obj_set_hidden(area, true);
}

void FlipClock::stop() {
    // Keep area visible; stop any dynamic animation/timer if active.
}

void FlipClock::refresh(const tm &time, bool animated) {
    if (!area || lv_obj_has_flag(area, LV_OBJ_FLAG_HIDDEN)) return;

    // 1. Check Day / AM-PM change
    if (time.tm_mday != lastDay || time.tm_hour != lastHour) {
        lastDay = time.tm_mday;
        const char *ampm = time.tm_hour >= 12 ? "PM" : "AM";
        c::setText(labelAmPm, ampm);

        static const char *kWeeks[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        char dateBuf[32];
        std::snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d %s",
                      time.tm_mon + 1, time.tm_mday, kWeeks[time.tm_wday % 7]);
        c::setText(labelDate, dateBuf);
    }

    // 2. Check Hour change
    if (time.tm_hour != lastHour) {
        lastHour = time.tm_hour;
        char hBuf[8];
        std::snprintf(hBuf, sizeof(hBuf), "%02d", lastHour);
        c::setText(hourLabel, hBuf);
    }

    // 3. Check Minute change
    if (time.tm_min != lastMin) {
        lastMin = time.tm_min;
        char mBuf[8];
        std::snprintf(mBuf, sizeof(mBuf), "%02d", lastMin);
        c::setText(minuteLabel, mBuf);
    }

    // 4. Seconds update
    if (time.tm_sec != lastSec) {
        lastSec = time.tm_sec;
        char sBuf[16];
        std::snprintf(sBuf, sizeof(sBuf), "SEC %02d", lastSec);
        c::setText(secLabel, sBuf);

        // Update battery info every 10 seconds
        if (lastSec % 10 == 0) {
            const auto bat = sys::Battery::snapshot();
            char batBuf[32];
            const char *icon = bat.percent >= 90 ? LV_SYMBOL_BATTERY_FULL :
                               bat.percent >= 60 ? LV_SYMBOL_BATTERY_3 :
                               bat.percent >= 30 ? LV_SYMBOL_BATTERY_2 : LV_SYMBOL_BATTERY_1;
            if (bat.chargingKnown && bat.charging) icon = LV_SYMBOL_CHARGE;
            std::snprintf(batBuf, sizeof(batBuf), "%s %d%%", icon, bat.valid ? bat.percent : 88);
            c::setText(infoLabel, batBuf);
        }
    }
}

} // namespace ui
