#include "cubepaw_os.h"
#include "lvgl_theme.h"
#include <ctime>
#include <cstdio>

namespace cubepaw {

struct WatchfaceState {
    lv_obj_t* wifi_label = nullptr;
    lv_obj_t* time_label = nullptr;
    lv_obj_t* date_label = nullptr;
    lv_obj_t* batt_label = nullptr;
    lv_obj_t* avatar_img = nullptr;
    lv_timer_t* clock_timer = nullptr;
    int blink_tick = 0;
};

static WatchfaceState g_wf;

static const lv_image_dsc_t* GetXiaoyuEmojiDsc(const char* name) {
    auto* theme = LvglThemeManager::GetInstance().GetTheme("light");
    if (!theme || !theme->emoji_collection()) return nullptr;
    auto* img = theme->emoji_collection()->GetEmojiImage(name);
    return img ? img->image_dsc() : nullptr;
}

static void UpdateClockTimerCb(lv_timer_t* timer) {
    (void)timer;
    if (!g_wf.time_label) return;

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    lv_label_set_text(g_wf.time_label, time_buf);

    static const char* kWeekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%02d月%02d日 %s",
             timeinfo.tm_mon + 1, timeinfo.tm_mday, kWeekdays[timeinfo.tm_wday % 7]);
    lv_label_set_text(g_wf.date_label, date_buf);

    auto& cb = CubePawOS::GetInstance().Callbacks();
    if (cb.on_get_wifi_ssid && g_wf.wifi_label) {
        std::string w = cb.on_get_wifi_ssid();
        lv_label_set_text(g_wf.wifi_label, w.c_str());
    }
    if (cb.on_get_battery_percent && g_wf.batt_label) {
        char batt_buf[24];
        snprintf(batt_buf, sizeof(batt_buf), "电量 %d%%", cb.on_get_battery_percent());
        lv_label_set_text(g_wf.batt_label, batt_buf);
    }

    // 每 4 秒切换一次表盘上的小玉动态表情
    g_wf.blink_tick++;
    if (g_wf.avatar_img && (g_wf.blink_tick % 4 == 0)) {
        static const char* kIdleEmotions[] = {"happy", "winking", "loving", "cool", "relaxed"};
        const char* emo = kIdleEmotions[(g_wf.blink_tick / 4) % 5];
        if (const auto* dsc = GetXiaoyuEmojiDsc(emo)) {
            lv_image_set_src(g_wf.avatar_img, dsc);
        }
    }
}

static void OnWatchfaceAvatarClickCb(lv_event_t* e) {
    (void)e;
    CubePawOS::GetInstance().OpenApp(AppId::AI_CHAT);
}

static void OnOpenLauncherBtnCb(lv_event_t* e) {
    (void)e;
    CubePawOS::GetInstance().OpenApp(AppId::LAUNCHER);
}

static void OnWatchfaceDeleteCb(lv_event_t* e) {
    (void)e;
    if (g_wf.clock_timer) {
        lv_timer_del(g_wf.clock_timer);
        g_wf.clock_timer = nullptr;
    }
    g_wf.wifi_label = nullptr;
    g_wf.time_label = nullptr;
    g_wf.date_label = nullptr;
    g_wf.batt_label = nullptr;
    g_wf.avatar_img = nullptr;
}

void BuildWatchfaceApp(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFF6B3), 0);
    lv_obj_add_event_cb(parent, OnWatchfaceDeleteCb, LV_EVENT_DELETE, nullptr);

    // 顶部状态栏 (点击可直接进入网络设置)
    lv_obj_t* top_bar = lv_btn_create(parent);
    lv_obj_set_size(top_bar, 240, 26);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_shadow_width(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 10, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(top_bar, [](lv_event_t*) {
        CubePawOS::GetInstance().OpenApp(AppId::SETTINGS);
    }, LV_EVENT_CLICKED, nullptr);

    g_wf.wifi_label = lv_label_create(top_bar);
    lv_label_set_text(g_wf.wifi_label, "点此配网");
    lv_obj_set_style_text_color(g_wf.wifi_label, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_wf.wifi_label, LV_ALIGN_LEFT_MID, 0, 0);

    g_wf.batt_label = lv_label_create(top_bar);
    lv_label_set_text(g_wf.batt_label, "电量 95%");
    lv_obj_set_style_text_color(g_wf.batt_label, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_wf.batt_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // 大号时间显示
    g_wf.time_label = lv_label_create(parent);
    lv_label_set_text(g_wf.time_label, "12:00:00");
    lv_obj_set_style_text_color(g_wf.time_label, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_wf.time_label, LV_ALIGN_TOP_MID, 0, 26);

    // 日期与星期
    g_wf.date_label = lv_label_create(parent);
    lv_label_set_text(g_wf.date_label, "11月10日 周二");
    lv_obj_set_style_text_color(g_wf.date_label, lv_color_hex(0x525A8E), 0);
    lv_obj_align(g_wf.date_label, LV_ALIGN_TOP_MID, 0, 50);

    // 中央小玉互动卡片 (含 64x64 小玉真彩头像 + 点击进入 AI 语音对话)
    lv_obj_t* avatar_card = lv_btn_create(parent);
    lv_obj_set_size(avatar_card, 174, 112);
    lv_obj_align(avatar_card, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_color(avatar_card, lv_color_hex(0xFFEA79), 0);
    lv_obj_set_style_radius(avatar_card, 20, 0);
    lv_obj_set_style_border_color(avatar_card, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_border_width(avatar_card, 3, 0);
    lv_obj_set_ext_click_area(avatar_card, 8);
    lv_obj_add_flag(avatar_card, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(avatar_card, OnWatchfaceAvatarClickCb, LV_EVENT_CLICKED, nullptr);

    if (const auto* dsc = GetXiaoyuEmojiDsc("happy")) {
        g_wf.avatar_img = lv_image_create(avatar_card);
        lv_image_set_src(g_wf.avatar_img, dsc);
        lv_obj_align(g_wf.avatar_img, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_clear_flag(g_wf.avatar_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_wf.avatar_img, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    lv_obj_t* avatar_title = lv_label_create(avatar_card);
    lv_label_set_text(avatar_title, "点小玉开始聊天 ~");
    lv_obj_set_style_text_align(avatar_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(avatar_title, lv_color_hex(0x262D5C), 0);
    lv_obj_align(avatar_title, LV_ALIGN_BOTTOM_MID, 0, -2);

    // 底部左滑/点击进入 6 宫格桌面按钮
    lv_obj_t* menu_btn = lv_btn_create(parent);
    lv_obj_set_size(menu_btn, 196, 34);
    lv_obj_align(menu_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_radius(menu_btn, 17, 0);
    lv_obj_set_ext_click_area(menu_btn, 10);
    lv_obj_add_event_cb(menu_btn, OnOpenLauncherBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* menu_lbl = lv_label_create(menu_btn);
    lv_label_set_text(menu_lbl, "<<< 点此打开应用桌面");
    lv_obj_set_style_text_color(menu_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(menu_lbl);

    UpdateClockTimerCb(nullptr);
    g_wf.clock_timer = lv_timer_create(UpdateClockTimerCb, 1000, nullptr);
}

void BuildAiChatApp(lv_obj_t* parent) {
    // 透明浮层：透出小智原生的 21 种小玉动态表情、顶部状态栏与底部对话字幕
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_CLICKABLE);

    // 左上角悬浮返回桌面按钮
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 72, 32);
    lv_obj_set_pos(back_btn, 6, 28);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_radius(back_btn, 16, 0);
    lv_obj_set_ext_click_area(back_btn, 12);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        CubePawOS::GetInstance().OpenApp(AppId::LAUNCHER);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< 桌面");
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_lbl);

    // 右上角一键配网按钮
    lv_obj_t* cfg_btn = lv_btn_create(parent);
    lv_obj_set_size(cfg_btn, 76, 32);
    lv_obj_align(cfg_btn, LV_ALIGN_TOP_RIGHT, -6, 28);
    lv_obj_set_style_bg_color(cfg_btn, lv_color_hex(0xFF8A80), 0);
    lv_obj_set_style_radius(cfg_btn, 16, 0);
    lv_obj_set_ext_click_area(cfg_btn, 12);
    lv_obj_add_event_cb(cfg_btn, [](lv_event_t*) {
        auto& cb = CubePawOS::GetInstance().Callbacks();
        if (cb.on_reset_wifi) cb.on_reset_wifi();
        CubePawOS::GetInstance().OpenApp(AppId::SETTINGS);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cfg_lbl = lv_label_create(cfg_btn);
    lv_label_set_text(cfg_lbl, "连WiFi");
    lv_obj_set_style_text_color(cfg_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_center(cfg_lbl);

    // 中央透明对讲触发区（点击小玉面部即可开始/停止说话）
    lv_obj_t* touch_talk = lv_btn_create(parent);
    lv_obj_set_size(touch_talk, 160, 110);
    lv_obj_align(touch_talk, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(touch_talk, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_talk, 0, 0);
    lv_obj_set_style_shadow_width(touch_talk, 0, 0);
    lv_obj_add_event_cb(touch_talk, [](lv_event_t*) {
        auto& cb = CubePawOS::GetInstance().Callbacks();
        if (cb.on_enter_ai_chat) cb.on_enter_ai_chat();
    }, LV_EVENT_CLICKED, nullptr);
}

} // namespace cubepaw
