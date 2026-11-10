#include "cubepaw_os.h"
#include <cstdio>

namespace cubepaw {

struct SettingsState {
    lv_obj_t* net_info_lbl = nullptr;
    lv_obj_t* wifi_btn_lbl = nullptr;
    lv_timer_t* refresh_timer = nullptr;
};

static SettingsState g_set;

static void UpdateSettingsNetInfoCb(lv_timer_t* timer) {
    (void)timer;
    if (!g_set.net_info_lbl) return;
    auto& cb = CubePawOS::GetInstance().Callbacks();
    std::string ssid = cb.on_get_wifi_ssid ? cb.on_get_wifi_ssid() : "未连接";
    std::string ip = cb.on_get_ip_address ? cb.on_get_ip_address() : "点击右侧配网";

    char net_buf[128];
    snprintf(net_buf, sizeof(net_buf), "%s\n%s", ssid.c_str(), ip.c_str());
    lv_label_set_text(g_set.net_info_lbl, net_buf);
}

static void OnSettingsDeleteCb(lv_event_t* e) {
    (void)e;
    if (g_set.refresh_timer) {
        lv_timer_del(g_set.refresh_timer);
        g_set.refresh_timer = nullptr;
    }
    g_set.net_info_lbl = nullptr;
    g_set.wifi_btn_lbl = nullptr;
}

void BuildSettingsApp(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xECEFF1), 0);
    lv_obj_add_event_cb(parent, OnSettingsDeleteCb, LV_EVENT_DELETE, nullptr);

    // 顶部返回按钮 (加大触控热区)
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 68, 30);
    lv_obj_set_pos(back_btn, 8, 6);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_radius(back_btn, 15, 0);
    lv_obj_set_ext_click_area(back_btn, 10);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        CubePawOS::GetInstance().OpenApp(AppId::LAUNCHER);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< 桌面");
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "网络与系统设置");
    lv_obj_set_style_text_color(title, lv_color_hex(0x262D5C), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 24, 12);

    auto& os = CubePawOS::GetInstance();
    auto& cb = os.Callbacks();

    // 1. 当前 Wi-Fi 信息与一键配网按钮
    g_set.net_info_lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(g_set.net_info_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_set_pos(g_set.net_info_lbl, 10, 42);

    lv_obj_t* wifi_btn = lv_btn_create(parent);
    lv_obj_set_size(wifi_btn, 82, 36);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_RIGHT, -8, 42);
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0xFF8A80), 0);
    lv_obj_set_style_radius(wifi_btn, 12, 0);
    lv_obj_set_ext_click_area(wifi_btn, 10);
    lv_obj_add_event_cb(wifi_btn, [](lv_event_t*) {
        auto& c = CubePawOS::GetInstance().Callbacks();
        if (c.on_reset_wifi) c.on_reset_wifi();
        if (g_set.wifi_btn_lbl) {
            lv_label_set_text(g_set.wifi_btn_lbl, "热点已开");
        }
        UpdateSettingsNetInfoCb(nullptr);
    }, LV_EVENT_CLICKED, nullptr);
    g_set.wifi_btn_lbl = lv_label_create(wifi_btn);
    lv_label_set_text(g_set.wifi_btn_lbl, "手机配网");
    lv_obj_set_style_text_color(g_set.wifi_btn_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_center(g_set.wifi_btn_lbl);

    // 2. 音量调节滑块 (推荐 60~70 防共振)
    int cur_vol = cb.on_get_volume ? cb.on_get_volume() : 65;
    lv_obj_t* vol_lbl = lv_label_create(parent);
    lv_label_set_text(vol_lbl, "喇叭音量:");
    lv_obj_set_style_text_color(vol_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_set_pos(vol_lbl, 10, 96);

    lv_obj_t* vol_slider = lv_slider_create(parent);
    lv_obj_set_size(vol_slider, 130, 14);
    lv_obj_set_pos(vol_slider, 92, 98);
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, cur_vol, LV_ANIM_OFF);
    lv_obj_set_ext_click_area(vol_slider, 10);
    lv_obj_add_event_cb(vol_slider, [](lv_event_t* e) {
        int v = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        auto& c = CubePawOS::GetInstance().Callbacks();
        if (c.on_set_volume) c.on_set_volume(v);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // 3. 屏幕背光亮度滑块
    int cur_bri = cb.on_get_brightness ? cb.on_get_brightness() : 85;
    lv_obj_t* bri_lbl = lv_label_create(parent);
    lv_label_set_text(bri_lbl, "屏幕亮度:");
    lv_obj_set_style_text_color(bri_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_set_pos(bri_lbl, 10, 136);

    lv_obj_t* bri_slider = lv_slider_create(parent);
    lv_obj_set_size(bri_slider, 130, 14);
    lv_obj_set_pos(bri_slider, 92, 138);
    lv_slider_set_range(bri_slider, 10, 100);
    lv_slider_set_value(bri_slider, cur_bri, LV_ANIM_OFF);
    lv_obj_set_ext_click_area(bri_slider, 10);
    lv_obj_add_event_cb(bri_slider, [](lv_event_t* e) {
        int b = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        auto& c = CubePawOS::GetInstance().Callbacks();
        if (c.on_set_brightness) c.on_set_brightness(b);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // 4. 防回声自激闭麦开关（彻底解决讲一半暂停乱说话）
    lv_obj_t* echo_lbl = lv_label_create(parent);
    lv_label_set_text(echo_lbl, "说话时闭麦防打断:");
    lv_obj_set_style_text_color(echo_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_set_pos(echo_lbl, 10, 182);

    lv_obj_t* echo_sw = lv_switch_create(parent);
    lv_obj_set_size(echo_sw, 54, 28);
    lv_obj_set_pos(echo_sw, 172, 176);
    lv_obj_set_ext_click_area(echo_sw, 12);
    if (os.IsAntiEchoMuteEnabled()) {
        lv_obj_add_state(echo_sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(echo_sw, [](lv_event_t* e) {
        auto* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
        bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        CubePawOS::GetInstance().SetAntiEchoMuteEnabled(checked);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    UpdateSettingsNetInfoCb(nullptr);
    g_set.refresh_timer = lv_timer_create(UpdateSettingsNetInfoCb, 1000, nullptr);
}

} // namespace cubepaw
