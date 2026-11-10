#include "cubepaw_os.h"
#include <material_symbols.h>

LV_FONT_DECLARE(font_material_symbols_30_4);

namespace cubepaw {

struct LauncherItem {
    const char* symbol;
    uint32_t bg_color;
    AppId target_app;
};

static const LauncherItem kApps[6] = {
    {MATERIAL_SYMBOLS_CHAT_BUBBLE,    0xFFB74D, AppId::AI_CHAT},
    {MATERIAL_SYMBOLS_MUSIC_NOTE,     0xBA68C8, AppId::MUSIC_PLAYER},
    {MATERIAL_SYMBOLS_IMAGE,          0x4DB6AC, AppId::PHOTO_GALLERY},
    {MATERIAL_SYMBOLS_SPORTS_ESPORTS, 0x4FC3F7, AppId::MINI_GAMES},
    {MATERIAL_SYMBOLS_SCHEDULE,       0xFF8A80, AppId::WATCHFACE},
    {MATERIAL_SYMBOLS_SETTINGS,       0x90A4AE, AppId::SETTINGS},
};

static void OnAppIconClickCb(lv_event_t* e) {
    int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (idx >= 0 && idx < 6) {
        CubePawOS::GetInstance().OpenApp(kApps[idx].target_app);
    }
}

void BuildLauncherApp(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFF6B3), 0);

    // 纯图标 3列 x 2行 6宫格大触控按钮（去掉文字名称，居中大图标设计）
    const int kCardW = 66;
    const int kCardH = 92;
    const int kGapX = 10;
    const int kGapY = 16;
    const int kStartX = 11;
    const int kStartY = 20;

    for (int i = 0; i < 6; ++i) {
        int row = i / 3;
        int col = i % 3;
        int x = kStartX + col * (kCardW + kGapX);
        int y = kStartY + row * (kCardH + kGapY);

        lv_obj_t* card = lv_btn_create(parent);
        lv_obj_set_size(card, kCardW, kCardH);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_style_bg_color(card, lv_color_hex(kApps[i].bg_color), 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x262D5C), 0);
        lv_obj_set_style_border_width(card, 3, 0);
        lv_obj_set_style_shadow_width(card, 6, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
        lv_obj_set_ext_click_area(card, 5);
        lv_obj_add_flag(card, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(card, OnAppIconClickCb, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        lv_obj_t* icon = lv_label_create(card);
        lv_obj_set_style_text_font(icon, &font_material_symbols_30_4, 0);
        lv_label_set_text(icon, kApps[i].symbol);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(icon);
    }
}

} // namespace cubepaw
