#include "cute_clock.hpp"
#include "theme.hpp"
#include "compact_ui.hpp"
#include "core/battery_mgr.hpp"
#include <cstdio>

namespace ui {

namespace c = compact;
namespace t = theme;

void CuteClock::create(lv_obj_t *parent) {
    if (area) return;

    // Root background: Soft pastel gradient/cream
    area = c::panel(parent, 240, 240);
    lv_obj_set_pos(area, 0, 0);
    lv_obj_set_style_pad_all(area, 0, 0);
    lv_obj_set_style_radius(area, 0, 0);
    lv_obj_set_style_border_width(area, 0, 0);
    lv_obj_set_style_bg_color(area, lv_color_hex(0xFFF2F5), 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_COVER, 0);

    // --- 1. Top Header Pills (y = 6) ---
    // Left: Date pill (136x24)
    auto datePill = c::panel(area, 136, 24);
    lv_obj_set_pos(datePill, 8, 6);
    lv_obj_set_style_pad_all(datePill, 0, 0);
    lv_obj_set_style_radius(datePill, 12, 0);
    lv_obj_set_style_bg_color(datePill, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(datePill, 1, 0);
    lv_obj_set_style_border_color(datePill, lv_color_hex(0xFFCAD4), 0);

    labelDatePill = c::label(datePill, "11月11日 周三", 136);
    lv_obj_center(labelDatePill);
    lv_obj_set_style_text_align(labelDatePill, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(labelDatePill, t::fontSmall(), 0);
    lv_obj_set_style_text_color(labelDatePill, lv_color_hex(0xFF477E), 0);

    // Right: Battery heart pill (84x24)
    auto batPill = c::panel(area, 84, 24);
    lv_obj_set_pos(batPill, 148, 6);
    lv_obj_set_style_pad_all(batPill, 0, 0);
    lv_obj_set_style_radius(batPill, 12, 0);
    lv_obj_set_style_bg_color(batPill, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(batPill, 1, 0);
    lv_obj_set_style_border_color(batPill, lv_color_hex(0xFFCAD4), 0);

    labelBatPill = c::label(batPill, "♥ 95%", 84);
    lv_obj_center(labelBatPill);
    lv_obj_set_style_text_align(labelBatPill, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(labelBatPill, t::fontSmall(), 0);
    lv_obj_set_style_text_color(labelBatPill, lv_color_hex(0xFF5A76), 0);

    // --- 2. Center Character: Strawberry Boba Bunny (y = 34, h = 98) ---
    bunnyBox = lv_obj_create(area);
    lv_obj_remove_style_all(bunnyBox);
    lv_obj_set_pos(bunnyBox, 70, 34);
    lv_obj_set_size(bunnyBox, 100, 98);
    lv_obj_set_scrollbar_mode(bunnyBox, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(bunnyBox, LV_OBJ_FLAG_CLICKABLE);

    // Left Ear
    auto lEar = c::panel(bunnyBox, 16, 34);
    lv_obj_set_pos(lEar, 18, 0);
    lv_obj_set_style_pad_all(lEar, 0, 0);
    lv_obj_set_style_radius(lEar, 8, 0);
    lv_obj_set_style_bg_color(lEar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(lEar, 1, 0);
    lv_obj_set_style_border_color(lEar, lv_color_hex(0xFFCAD4), 0);

    auto lInner = c::panel(lEar, 8, 22);
    lv_obj_set_pos(lInner, 4, 6);
    lv_obj_set_style_radius(lInner, 4, 0);
    lv_obj_set_style_bg_color(lInner, lv_color_hex(0xFF85A2), 0);
    lv_obj_set_style_border_width(lInner, 0, 0);

    // Right Ear
    auto rEar = c::panel(bunnyBox, 16, 34);
    lv_obj_set_pos(rEar, 66, 0);
    lv_obj_set_style_pad_all(rEar, 0, 0);
    lv_obj_set_style_radius(rEar, 8, 0);
    lv_obj_set_style_bg_color(rEar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(rEar, 1, 0);
    lv_obj_set_style_border_color(rEar, lv_color_hex(0xFFCAD4), 0);

    auto rInner = c::panel(rEar, 8, 22);
    lv_obj_set_pos(rInner, 4, 6);
    lv_obj_set_style_radius(rInner, 4, 0);
    lv_obj_set_style_bg_color(rInner, lv_color_hex(0xFF85A2), 0);
    lv_obj_set_style_border_width(rInner, 0, 0);

    // Strawberry Hairclip on right ear
    auto sClip = c::panel(bunnyBox, 12, 12);
    lv_obj_set_pos(sClip, 76, 18);
    lv_obj_set_style_radius(sClip, 6, 0);
    lv_obj_set_style_bg_color(sClip, lv_color_hex(0xFF477E), 0);
    lv_obj_set_style_border_width(sClip, 0, 0);

    auto sLeaf = c::panel(bunnyBox, 6, 4);
    lv_obj_set_pos(sLeaf, 79, 15);
    lv_obj_set_style_radius(sLeaf, 2, 0);
    lv_obj_set_style_bg_color(sLeaf, lv_color_hex(0x52C41A), 0);
    lv_obj_set_style_border_width(sLeaf, 0, 0);

    // Bunny Head (w = 76, h = 58)
    auto head = c::panel(bunnyBox, 76, 58);
    lv_obj_set_pos(head, 12, 26);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_style_radius(head, 28, 0);
    lv_obj_set_style_bg_color(head, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(head, 2, 0);
    lv_obj_set_style_border_color(head, lv_color_hex(0xFFCAD4), 0);

    // Cheeks
    auto lCheek = c::panel(head, 12, 8);
    lv_obj_set_pos(lCheek, 4, 30);
    lv_obj_set_style_radius(lCheek, 4, 0);
    lv_obj_set_style_bg_color(lCheek, lv_color_hex(0xFFB3C6), 0);
    lv_obj_set_style_border_width(lCheek, 0, 0);

    auto rCheek = c::panel(head, 12, 8);
    lv_obj_set_pos(rCheek, 56, 30);
    lv_obj_set_style_radius(rCheek, 4, 0);
    lv_obj_set_style_bg_color(rCheek, lv_color_hex(0xFFB3C6), 0);
    lv_obj_set_style_border_width(rCheek, 0, 0);

    // Eyes
    leftEye = c::panel(head, 6, 10);
    lv_obj_set_pos(leftEye, 20, 22);
    lv_obj_set_style_radius(leftEye, 3, 0);
    lv_obj_set_style_bg_color(leftEye, lv_color_hex(0x3D2B3D), 0);
    lv_obj_set_style_border_width(leftEye, 0, 0);

    auto lPupil = c::panel(leftEye, 2, 2);
    lv_obj_set_pos(lPupil, 1, 1);
    lv_obj_set_style_radius(lPupil, 1, 0);
    lv_obj_set_style_bg_color(lPupil, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(lPupil, 0, 0);

    rightEye = c::panel(head, 6, 10);
    lv_obj_set_pos(rightEye, 48, 22);
    lv_obj_set_style_radius(rightEye, 3, 0);
    lv_obj_set_style_bg_color(rightEye, lv_color_hex(0x3D2B3D), 0);
    lv_obj_set_style_border_width(rightEye, 0, 0);

    auto rPupil = c::panel(rightEye, 2, 2);
    lv_obj_set_pos(rPupil, 1, 1);
    lv_obj_set_style_radius(rPupil, 1, 0);
    lv_obj_set_style_bg_color(rPupil, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(rPupil, 0, 0);

    // Nose
    auto nose = c::panel(head, 6, 4);
    lv_obj_set_pos(nose, 34, 30);
    lv_obj_set_style_radius(nose, 2, 0);
    lv_obj_set_style_bg_color(nose, lv_color_hex(0xFF758F), 0);
    lv_obj_set_style_border_width(nose, 0, 0);

    // Boba Milk Cup
    auto cup = c::panel(bunnyBox, 22, 24);
    lv_obj_set_pos(cup, 39, 66);
    lv_obj_set_style_pad_all(cup, 0, 0);
    lv_obj_set_style_radius(cup, 4, 0);
    lv_obj_set_style_bg_color(cup, lv_color_hex(0xFFE5D9), 0);
    lv_obj_set_style_border_width(cup, 1, 0);
    lv_obj_set_style_border_color(cup, lv_color_hex(0xE07A5F), 0);

    // Straw
    auto straw = c::panel(cup, 3, 8);
    lv_obj_set_pos(straw, 10, -4);
    lv_obj_set_style_radius(straw, 1, 0);
    lv_obj_set_style_bg_color(straw, lv_color_hex(0xFF758F), 0);
    lv_obj_set_style_border_width(straw, 0, 0);

    // Tapioca pearls
    int bobaPos[3][2] = {{4, 15}, {10, 16}, {15, 14}};
    for (int i = 0; i < 3; ++i) {
        auto boba = c::panel(cup, 4, 4);
        lv_obj_set_pos(boba, bobaPos[i][0], bobaPos[i][1]);
        lv_obj_set_style_radius(boba, 2, 0);
        lv_obj_set_style_bg_color(boba, lv_color_hex(0x6B4B3E), 0);
        lv_obj_set_style_border_width(boba, 0, 0);
    }

    // Paws
    auto lPaw = c::panel(bunnyBox, 8, 8);
    lv_obj_set_pos(lPaw, 33, 74);
    lv_obj_set_style_radius(lPaw, 4, 0);
    lv_obj_set_style_bg_color(lPaw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(lPaw, 1, 0);
    lv_obj_set_style_border_color(lPaw, lv_color_hex(0xFFCAD4), 0);

    auto rPaw = c::panel(bunnyBox, 8, 8);
    lv_obj_set_pos(rPaw, 59, 74);
    lv_obj_set_style_radius(rPaw, 4, 0);
    lv_obj_set_style_bg_color(rPaw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(rPaw, 1, 0);
    lv_obj_set_style_border_color(rPaw, lv_color_hex(0xFFCAD4), 0);

    // --- 3. Bottom Time Card (y = 138, h = 94) ---
    timeCard = c::panel(area, 224, 94);
    lv_obj_set_pos(timeCard, 8, 138);
    lv_obj_set_style_pad_all(timeCard, 0, 0);
    lv_obj_set_style_radius(timeCard, 16, 0);
    lv_obj_set_style_bg_color(timeCard, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(timeCard, 2, 0);
    lv_obj_set_style_border_color(timeCard, lv_color_hex(0xFFCAD4), 0);

    // Time text (HH:MM)
    labelTime = c::label(timeCard, "15:28", 160);
    lv_obj_set_pos(labelTime, 14, 4);
    lv_obj_set_style_text_align(labelTime, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(labelTime, lv_color_hex(0xFF477E), 0);

    // Seconds text
    labelSec = c::label(timeCard, "30s", 42);
    lv_obj_set_pos(labelSec, 174, 22);
    lv_obj_set_style_text_font(labelSec, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labelSec, lv_color_hex(0xFA8C16), 0);

    // Footnote row inside card
    labelFoot = c::label(timeCard, "5820步  •  ♥ 76  •  24°C", 224);
    lv_obj_set_pos(labelFoot, 0, 60);
    lv_obj_set_style_text_align(labelFoot, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(labelFoot, t::fontSmall(), 0);
    lv_obj_set_style_text_color(labelFoot, lv_color_hex(0x8C9AA8), 0);
}

void CuteClock::show(lv_obj_t *parent) {
    if (!area) create(parent);
    lv_obj_set_hidden(area, false);
    lastMin = lastSec = lastDay = -1;
    eyeBlinked = false;
}

void CuteClock::hide() {
    if (area) lv_obj_set_hidden(area, true);
}

void CuteClock::stop() {
    hide();
}

void CuteClock::refresh(const tm &time, bool animated) {
    if (!area || lv_obj_has_flag(area, LV_OBJ_FLAG_HIDDEN)) return;

    // 1. Day change update
    if (time.tm_mday != lastDay) {
        lastDay = time.tm_mday;
        static const char *kWeeksCn[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        char dateBuf[36];
        std::snprintf(dateBuf, sizeof(dateBuf), "%d月%d日 %s",
                      time.tm_mon + 1, time.tm_mday, kWeeksCn[time.tm_wday % 7]);
        c::setText(labelDatePill, dateBuf);
    }

    // 2. Minute change update
    if (time.tm_min != lastMin) {
        lastMin = time.tm_min;
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", time.tm_hour, time.tm_min);
        c::setText(labelTime, timeBuf);
    }

    // 3. Second change update
    if (time.tm_sec != lastSec) {
        lastSec = time.tm_sec;
        char secBuf[12];
        std::snprintf(secBuf, sizeof(secBuf), "%02ds", lastSec);
        c::setText(labelSec, secBuf);

        // Blinking animation: every 4 seconds, blink for 1 tick
        if (lastSec % 4 == 0) {
            lv_obj_set_height(leftEye, 2);
            lv_obj_set_height(rightEye, 2);
            eyeBlinked = true;
        } else if (eyeBlinked) {
            lv_obj_set_height(leftEye, 10);
            lv_obj_set_height(rightEye, 10);
            eyeBlinked = false;
        }

        // Update battery info every 10 seconds
        if (lastSec % 10 == 0) {
            const auto bat = sys::Battery::snapshot();
            char batBuf[24];
            std::snprintf(batBuf, sizeof(batBuf), "♥ %d%%", bat.valid ? bat.percent : 95);
            c::setText(labelBatPill, batBuf);
        }
    }
}

} // namespace ui
