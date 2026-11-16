#include "weather.hpp"

#include "core/config_server.hpp"
#include "core/connectivity.hpp"
#include "core/weather_client.hpp"
#include "esp_timer.h"
#include "ui/app_icons.hpp"
#include "ui/compact_ui.hpp"
#include "ui/theme.hpp"

#include <cstdio>
#include <cstring>

namespace apps {
namespace c = ui::compact;
namespace t = ui::theme;

namespace {

constexpr int kCardX = 14;
constexpr int kCardY = 114;
constexpr int kCardW = 212;
constexpr int kCardH = 68;
constexpr int kColW = 70;  // 3 * 70 = 210 inside 1px borders

lv_obj_t *labelAt(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font,
                  lv_color_t color, int width = LV_SIZE_CONTENT) {
    auto label = c::label(parent, text, width);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_clickable(label, false);
    return label;
}

void clearChildren(lv_obj_t *obj) { lv_obj_clean(obj); }

void drawSunAt(lv_obj_t *parent, bool smallTopRight) {
    // Static point arrays because lv_line_set_points retains the pointer until redraw.
    static const lv_point_precise_t bigRays[4][2] = {
        {{4, 21}, {44, 21}}, {{24, 2}, {24, 40}}, {{10, 7}, {38, 35}}, {{38, 7}, {10, 35}}};
    static const lv_point_precise_t smallRays[4][2] = {
        {{22, 12}, {44, 12}}, {{33, 2}, {33, 22}}, {{25, 5}, {41, 19}}, {{41, 5}, {25, 19}}};
    const auto &rays = smallTopRight ? smallRays : bigRays;
    for (int i = 0; i < 4; ++i) {
        auto line = lv_line_create(parent);
        lv_line_set_points(line, rays[i], 2);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_color(line, t::warning(), 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_clickable(line, false);
    }
    const int d = smallTopRight ? 11 : 16;
    const int x = smallTopRight ? 28 : 16;
    const int y = smallTopRight ? 7 : 13;
    auto disc = c::panel(parent, d, d);
    lv_obj_set_pos(disc, x, y);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, t::warning(), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(disc, 0, 0);
    lv_obj_set_clickable(disc, false);
}

// Two-pass silhouette: outer border shapes in muted ink, inner inset shapes in bg,
// giving a continuous outline without internal seam lines.
void drawCloudOutline(lv_obj_t *parent, int oy) {
    struct Shape { int x, y, w, h, r; };
    const lv_color_t edge = t::light() ? lv_color_hex(0xC7B6A5) : lv_color_hex(0x9BB0C1);
    const lv_color_t fill = t::bg();
    const Shape outer[4] = {
        {4, 15 + oy, 18, 18, LV_RADIUS_CIRCLE},
        {13, 8 + oy, 22, 22, LV_RADIUS_CIRCLE},
        {25, 16 + oy, 16, 16, LV_RADIUS_CIRCLE},
        {6, 22 + oy, 30, 11, 6},
    };
    const Shape inner[4] = {
        {6, 17 + oy, 14, 14, LV_RADIUS_CIRCLE},
        {15, 10 + oy, 18, 18, LV_RADIUS_CIRCLE},
        {27, 18 + oy, 12, 12, LV_RADIUS_CIRCLE},
        {8, 24 + oy, 26, 7, 4},
    };
    auto drawSet = [&](const Shape *shapes, lv_color_t color) {
        for (int i = 0; i < 4; ++i) {
            auto obj = c::panel(parent, shapes[i].w, shapes[i].h);
            lv_obj_set_pos(obj, shapes[i].x, shapes[i].y);
            lv_obj_set_style_radius(obj, shapes[i].r, 0);
            lv_obj_set_style_bg_color(obj, color, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(obj, 0, 0);
            lv_obj_set_clickable(obj, false);
        }
    };
    drawSet(outer, edge);
    drawSet(inner, fill);
}

void drawRainDrops(lv_obj_t *parent) {
    static const lv_point_precise_t drops[3][2] = {
        {{13, 33}, {10, 40}}, {{22, 33}, {19, 40}}, {{31, 33}, {28, 40}}};
    const lv_color_t rainBlue = lv_color_hex(0x4EA3F8);
    for (int i = 0; i < 3; ++i) {
        auto line = lv_line_create(parent);
        lv_line_set_points(line, drops[i], 2);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_color(line, rainBlue, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_clickable(line, false);
    }
}

void drawSnowFlakes(lv_obj_t *parent) {
    static const int pts[3][2] = {{11, 35}, {21, 37}, {31, 35}};
    const lv_color_t snowBlue = lv_color_hex(0x7CC4FA);
    for (const auto &pt : pts) {
        auto dot = c::panel(parent, 4, 4);
        lv_obj_set_pos(dot, pt[0], pt[1]);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, snowBlue, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_clickable(dot, false);
    }
}

void drawLightning(lv_obj_t *parent) {
    static const lv_point_precise_t bolt[4] = {{25, 29}, {20, 35}, {25, 35}, {20, 42}};
    auto line = lv_line_create(parent);
    lv_line_set_points(line, bolt, 4);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, t::warning(), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_clickable(line, false);
}

const char *classifyIcon(const char *text) {
    if (!text || !*text) return "cloud";
    if (std::strstr(text, "雷")) return "thunder";
    if (std::strstr(text, "雪") || std::strstr(text, "冰") || std::strstr(text, "霜")) return "snow";
    if (std::strstr(text, "雨")) return "rain";
    if (std::strstr(text, "多云") || std::strstr(text, "少云")) return "partly";
    if (std::strstr(text, "晴")) return "sun";
    return "cloud";
}

lv_obj_t *makeFieldLabel(lv_obj_t *parent, const char *text) {
    auto lbl = c::label(parent, text, LV_PCT(100));
    lv_obj_set_style_text_font(lbl, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lbl, t::muted(), 0);
    return lbl;
}

} // namespace

const char *WeatherApp::icon() const { return ui::icons::Weather; }

void WeatherApp::onCreate(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, t::bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    core::config::acquire();
    core::WeatherClient::instance().init();
    buildMain(parent);
    buildConfig(parent);
    m_configAuto = !core::WeatherClient::instance().configured();
    showConfig(m_configAuto);

    updateHint();
    m_timer = lv_timer_create(timerCb, 1000, this);
    refreshUi();
}

void WeatherApp::buildMain(lv_obj_t *parent) {
    m_main = c::panel(parent, 240, 240);
    lv_obj_set_pos(m_main, 0, 0);
    lv_obj_set_style_pad_all(m_main, 0, 0);
    lv_obj_set_style_radius(m_main, 0, 0);
    lv_obj_set_style_border_width(m_main, 0, 0);
    lv_obj_set_style_bg_color(m_main, t::bg(), 0);
    lv_obj_set_style_bg_opa(m_main, LV_OPA_COVER, 0);
    lv_obj_set_clickable(m_main, false);

    // Enlarged 36px-tall city tap zone so tapping city name is effortless.
    auto cityHit = c::panel(m_main, 140, 36);
    lv_obj_set_pos(cityHit, 6, 2);
    lv_obj_set_style_bg_opa(cityHit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cityHit, 0, 0);
    lv_obj_set_style_pad_all(cityHit, 0, 0);
    lv_obj_set_clickable(cityHit, true);
    lv_obj_add_event_cb(cityHit, cityCb, LV_EVENT_CLICKED, this);

    m_city = labelAt(cityHit, "杭州", 10, 9, t::fontSmall(), t::fg(), LV_SIZE_CONTENT);
    m_cityArrow = c::label(cityHit, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(m_cityArrow, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_cityArrow, t::muted(), 0);
    lv_obj_set_clickable(m_cityArrow, false);
    lv_obj_align_to(m_cityArrow, m_city, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    // Compact status indicator shown only when connecting or experiencing errors.
    m_netBadge = c::panel(m_main, 24, 24);
    lv_obj_set_pos(m_netBadge, 152, 11);
    lv_obj_set_style_radius(m_netBadge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(m_netBadge, t::surface(), 0);
    lv_obj_set_style_border_width(m_netBadge, 1, 0);
    lv_obj_set_style_border_color(m_netBadge, t::danger(), 0);
    lv_obj_set_style_pad_all(m_netBadge, 0, 0);
    lv_obj_set_clickable(m_netBadge, false);
    lv_obj_set_hidden(m_netBadge, true);

    m_netIcon = c::label(m_netBadge, LV_SYMBOL_WIFI);
    lv_obj_center(m_netIcon);
    lv_obj_set_style_text_font(m_netIcon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_netIcon, t::danger(), 0);
    lv_obj_set_clickable(m_netIcon, false);

    // Enlarged 48x42 touch container moved slightly inward from the R-corner bezel,
    // and clear of m_icon below (y = 46).
    m_refresh = c::panel(m_main, 48, 42);
    lv_obj_set_pos(m_refresh, 180, 4);
    lv_obj_set_style_radius(m_refresh, 14, 0);
    lv_obj_set_style_bg_opa(m_refresh, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(m_refresh, t::accent(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(m_refresh, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(m_refresh, 0, 0);
    lv_obj_set_style_pad_all(m_refresh, 0, 0);
    lv_obj_set_clickable(m_refresh, true);
    lv_obj_add_event_cb(m_refresh, refreshCb, LV_EVENT_CLICKED, this);

    auto refreshBadge = c::panel(m_refresh, 28, 28);
    lv_obj_align(refreshBadge, LV_ALIGN_CENTER, -1, 1);
    lv_obj_set_style_radius(refreshBadge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(refreshBadge, t::surface(), 0);
    lv_obj_set_style_border_width(refreshBadge, 1, 0);
    lv_obj_set_style_border_color(refreshBadge, t::edge(), 0);
    lv_obj_set_style_pad_all(refreshBadge, 0, 0);
    lv_obj_set_clickable(refreshBadge, false);

    auto refreshIcon = c::label(refreshBadge, LV_SYMBOL_REFRESH);
    lv_obj_center(refreshIcon);
    lv_obj_set_style_text_font(refreshIcon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(refreshIcon, t::muted(), 0);
    lv_obj_set_clickable(refreshIcon, false);

    // The temperature is the only large element on the screen.
    m_temp = c::label(m_main, "--");
    lv_obj_set_pos(m_temp, 14, 32);
    lv_obj_set_style_text_font(m_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(m_temp, t::fg(), 0);

    // Crisp hollow accent ring as the degree indicator (matches mockup).
    m_deg = c::panel(m_main, 10, 10);
    lv_obj_set_pos(m_deg, 66, 40);
    lv_obj_set_style_radius(m_deg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(m_deg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_deg, 2, 0);
    lv_obj_set_style_border_color(m_deg, t::accent(), 0);
    lv_obj_set_clickable(m_deg, false);
    lv_obj_set_hidden(m_deg, true);

    m_cond = labelAt(m_main, "", 16, 92, t::fontSmall(), t::muted(), 208);

    m_icon = c::panel(m_main, 48, 44);
    lv_obj_set_pos(m_icon, 176, 46);
    lv_obj_set_style_pad_all(m_icon, 0, 0);
    lv_obj_set_style_border_width(m_icon, 0, 0);
    lv_obj_set_style_bg_opa(m_icon, LV_OPA_TRANSP, 0);
    lv_obj_set_clickable(m_icon, false);

    auto card = c::panel(m_main, kCardW, kCardH);
    lv_obj_set_pos(card, kCardX, kCardY);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, t::surface(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, t::edge(), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_clickable(card, false);

    static const char *kLabels[3] = {"最高", "最低", "降雨"};
    for (int i = 0; i < 3; ++i) {
        auto col = c::panel(card, kColW, kCardH - 2);
        lv_obj_set_pos(col, 1 + i * kColW, 0);
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_pad_all(col, 0, 0);
        lv_obj_set_clickable(col, false);

        m_statLabels[i] = labelAt(col, kLabels[i], 0, 10, t::fontSmall(), t::muted(), kColW);
        lv_obj_set_style_text_align(m_statLabels[i], LV_TEXT_ALIGN_CENTER, 0);

        m_statValues[i] = labelAt(col, "--", 0, 30, &lv_font_montserrat_24, t::fg(), kColW);
        lv_obj_set_style_text_align(m_statValues[i], LV_TEXT_ALIGN_CENTER, 0);
    }

    m_footer = labelAt(m_main, "", 16, 192, t::fontSmall(), t::muted(), 158);
    lv_obj_set_style_text_align(m_footer, LV_TEXT_ALIGN_LEFT, 0);

    m_footerTime = labelAt(m_main, "", 176, 192, t::fontSmall(), t::muted(), 48);
    lv_obj_set_style_text_align(m_footerTime, LV_TEXT_ALIGN_RIGHT, 0);
}

void WeatherApp::buildConfig(lv_obj_t *parent) {
    m_config = c::panel(parent, 240, 240, true);
    lv_obj_set_pos(m_config, 0, 0);
    lv_obj_set_style_pad_hor(m_config, 12, 0);
    lv_obj_set_style_pad_top(m_config, 14, 0);
    lv_obj_set_style_pad_bottom(m_config, 18, 0);
    lv_obj_set_style_radius(m_config, 0, 0);
    lv_obj_set_style_border_width(m_config, 0, 0);
    lv_obj_set_style_bg_color(m_config, t::bg(), 0);
    lv_obj_set_style_bg_opa(m_config, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(m_config, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_config, 6, 0);

    auto topRow = c::panel(m_config, 216, 38);
    lv_obj_set_style_bg_opa(topRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(topRow, 0, 0);
    lv_obj_set_style_pad_all(topRow, 0, 0);
    auto title = c::label(topRow, "天气设置");
    lv_obj_set_style_text_font(title, t::fontSmall(), 0);
    lv_obj_set_style_text_color(title, t::fg(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    auto backBtn = c::button(topRow, "返回", [](lv_event_t *e) {
        auto self = static_cast<WeatherApp *>(lv_event_get_user_data(e));
        if (self) self->showConfig(false);
    }, this, 76, 36);
    lv_obj_align(backBtn, LV_ALIGN_RIGHT_MID, -6, 0);

    m_hint = c::label(m_config, "和风天气 v1 | 填写后保存", 216);
    lv_label_set_long_mode(m_hint, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(m_hint, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_hint, t::muted(), 0);

    auto &client = core::WeatherClient::instance();
    makeFieldLabel(m_config, "API Host (如 abc.qweatherapi.com)");
    m_fHost = c::field(m_config, "abc.qweatherapi.com", client.host().c_str(), 63);

    makeFieldLabel(m_config, "API Key");
    m_fKey = c::field(m_config, "X-QW-Api-Key", client.apiKey().c_str(), 63);

    auto coordRow = c::panel(m_config, 216, 62);
    lv_obj_set_style_bg_opa(coordRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(coordRow, 0, 0);
    lv_obj_set_style_pad_all(coordRow, 0, 0);
    labelAt(coordRow, "纬度", 0, 0, t::fontSmall(), t::muted());
    labelAt(coordRow, "经度", 112, 0, t::fontSmall(), t::muted());

    m_fLat = c::field(coordRow, "30.27", client.latitude().c_str(), 9);
    lv_obj_set_pos(m_fLat, 0, 22);
    lv_obj_set_width(m_fLat, 104);

    m_fLon = c::field(coordRow, "120.16", client.longitude().c_str(), 9);
    lv_obj_set_pos(m_fLon, 112, 22);
    lv_obj_set_width(m_fLon, 104);

    makeFieldLabel(m_config, "城市名");
    m_fCity = c::field(m_config, "杭州", client.city().c_str(), 15);

    auto btnRow = c::panel(m_config, 216, 38);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_style_margin_top(btnRow, 6, 0);

    auto cancelBtn = c::button(btnRow, "取消", [](lv_event_t *e) {
        auto self = static_cast<WeatherApp *>(lv_event_get_user_data(e));
        if (self) self->showConfig(false);
    }, this, 68, 38);
    lv_obj_set_pos(cancelBtn, 0, 0);

    auto save = c::button(btnRow, "保存并刷新", saveCb, this, 140, 38);
    lv_obj_set_pos(save, 76, 0);
}

void WeatherApp::updateHint() {
    if (!m_hint) return;
    if (!core::config::running()) {
        c::setText(m_hint, "和风天气 v1 | 填写后保存");
        return;
    }
    const std::string url = core::config::url();
    c::setText(m_hint, url.empty() ? "连接 Wi-Fi 后也可手机配置" : ("手机打开 " + url).c_str());
}

void WeatherApp::loadFields() {
    auto &client = core::WeatherClient::instance();
    if (m_fHost) lv_textarea_set_text(m_fHost, client.host().c_str());
    if (m_fKey) lv_textarea_set_text(m_fKey, client.apiKey().c_str());
    if (m_fLat) lv_textarea_set_text(m_fLat, client.latitude().c_str());
    if (m_fLon) lv_textarea_set_text(m_fLon, client.longitude().c_str());
    if (m_fCity) lv_textarea_set_text(m_fCity, client.city().c_str());
}

void WeatherApp::showConfig(bool show) {
    if (!m_config || !m_main) return;
    if (show) {
        loadFields();
        lv_obj_scroll_to_y(m_config, 0, LV_ANIM_OFF);
    }
    lv_obj_set_hidden(m_config, !show);
    lv_obj_set_hidden(m_main, show);
}

void WeatherApp::saveConfig() {
    auto &client = core::WeatherClient::instance();
    const char *host = lv_textarea_get_text(m_fHost);
    const char *key = lv_textarea_get_text(m_fKey);
    const char *lat = lv_textarea_get_text(m_fLat);
    const char *lon = lv_textarea_get_text(m_fLon);
    const char *city = lv_textarea_get_text(m_fCity);
    client.setConfig(host ? host : "", key ? key : "", lat ? lat : "", lon ? lon : "",
                     city ? city : "");
    client.init();
    showConfig(false);
    refreshUi();
    if (client.configured()) client.fetchAsync();
}

void WeatherApp::refreshUi() {
    auto &client = core::WeatherClient::instance();

    // A key saved from the phone page must move the watch off the placeholder
    // screen by itself; otherwise the page just sits there with empty fields.
    if (m_configAuto && client.configured()) {
        m_configAuto = false;
        showConfig(false);
    }

    const auto snap = client.snapshot();

    // City + subtle settings cue aligned next to its actual text width.
    const char *city = snap.city.empty() ? "未设置" : snap.city.c_str();
    c::setText(m_city, city);
    if (m_cityArrow) {
        lv_obj_update_layout(m_city);
        lv_obj_align_to(m_cityArrow, m_city, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
    }

    if (snap.valid) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", snap.temp);
        c::setText(m_temp, buf);
        lv_obj_update_layout(m_temp);
        lv_obj_set_x(m_deg, 14 + lv_obj_get_width(m_temp) + 3);
        lv_obj_set_hidden(m_deg, false);

        char condBuf[48];
        if (!snap.text.empty()) {
            std::snprintf(condBuf, sizeof(condBuf), "%s · 体感 %d°", snap.text.c_str(), snap.feels);
        } else {
            std::snprintf(condBuf, sizeof(condBuf), "体感 %d°", snap.feels);
        }
        c::setText(m_cond, condBuf);

        std::snprintf(buf, sizeof(buf), "%d°", snap.tempMax);
        c::setText(m_statValues[0], buf);
        std::snprintf(buf, sizeof(buf), "%d°", snap.tempMin);
        c::setText(m_statValues[1], buf);
        if (snap.rainChance >= 0) {
            std::snprintf(buf, sizeof(buf), "%d%%", snap.rainChance);
            c::setText(m_statValues[2], buf);
        } else {
            c::setText(m_statValues[2], "--");
        }

        if (!snap.tomorrow.empty() && !snap.dayAfter.empty()) {
            char line[64];
            std::snprintf(line, sizeof(line), "明天 %s · 后天 %s", snap.tomorrow.c_str(),
                          snap.dayAfter.c_str());
            c::setText(m_footer, line);
        } else {
            c::setText(m_footer, "");
        }
        lv_obj_set_style_text_color(m_footer, t::muted(), 0);
        c::setText(m_footerTime, snap.updated.c_str());
    } else {
        c::setText(m_temp, "--");
        lv_obj_set_hidden(m_deg, true);
        c::setText(m_cond, "");
        for (auto *value : m_statValues) c::setText(value, "--");
        c::setText(m_footer, "");
        c::setText(m_footerTime, "");
    }

    // Rebuild the icon only when the condition class changes on this view.
    const char *text = snap.valid ? snap.text.c_str() : "";
    const char *iconKey = classifyIcon(text);
    if (std::strcmp(m_lastIcon, iconKey) != 0) {
        std::snprintf(m_lastIcon, sizeof(m_lastIcon), "%s", iconKey);
        if (m_icon) {
            clearChildren(m_icon);
            if (std::strcmp(iconKey, "sun") == 0) {
                drawSunAt(m_icon, false);
            } else if (std::strcmp(iconKey, "partly") == 0) {
                drawSunAt(m_icon, true);
                drawCloudOutline(m_icon, 2);
            } else if (std::strcmp(iconKey, "rain") == 0) {
                drawCloudOutline(m_icon, -2);
                drawRainDrops(m_icon);
            } else if (std::strcmp(iconKey, "snow") == 0) {
                drawCloudOutline(m_icon, -2);
                drawSnowFlakes(m_icon);
            } else if (std::strcmp(iconKey, "thunder") == 0) {
                drawCloudOutline(m_icon, -2);
                drawLightning(m_icon);
            } else {
                drawCloudOutline(m_icon, 1);
            }
        }
    }

    // Status overrides the forecast line while syncing, offline, or when fetch fails.
    const auto wifi = sys::Connectivity::snapshot();
    const bool wifiConnecting =
        wifi.transitioning || wifi.state == sys::WifiState::Searching || wifi.state == sys::WifiState::Connecting;
    const bool wifiDown =
        wifi.state != sys::WifiState::Connected && !wifiConnecting;

    if (m_netBadge && m_netIcon) {
        if (snap.fetching || (wifi.state == sys::WifiState::Connected && snap.error.empty())) {
            lv_obj_set_hidden(m_netBadge, true);
        } else if (wifiConnecting) {
            c::setText(m_netIcon, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(m_netIcon, t::warning(), 0);
            lv_obj_set_style_border_color(m_netBadge, t::warning(), 0);
            lv_obj_set_hidden(m_netBadge, false);
        } else {
            c::setText(m_netIcon, wifiDown ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(m_netIcon, t::danger(), 0);
            lv_obj_set_style_border_color(m_netBadge, t::danger(), 0);
            lv_obj_set_hidden(m_netBadge, false);
        }
    }

    if (snap.fetching) {
        c::setText(m_footer, "同步中...");
        lv_obj_set_style_text_color(m_footer, t::muted(), 0);
    } else if (wifiConnecting) {
        c::setText(m_footer, "连网中...");
        lv_obj_set_style_text_color(m_footer, t::warning(), 0);
        c::setText(m_footerTime, snap.valid ? snap.updated.c_str() : "");
    } else if (wifiDown) {
        const char *msg = "Wi-Fi 未连接";
        if (wifi.state == sys::WifiState::Off) msg = "Wi-Fi 已关闭";
        else if (wifi.state == sys::WifiState::Failed) msg = "Wi-Fi 连接失败";
        c::setText(m_footer, msg);
        lv_obj_set_style_text_color(m_footer, t::danger(), 0);
        c::setText(m_footerTime, snap.valid ? snap.updated.c_str() : "");
    } else if (!snap.error.empty()) {
        c::setText(m_footer, snap.error.c_str());
        lv_obj_set_style_text_color(m_footer, t::danger(), 0);
        c::setText(m_footerTime, snap.valid ? snap.updated.c_str() : "");
    }
}

void WeatherApp::timerCb(lv_timer_t *timer) {
    auto self = static_cast<WeatherApp *>(lv_timer_get_user_data(timer));
    if (!self) return;

    auto &client = core::WeatherClient::instance();
    const auto wifi = sys::Connectivity::snapshot();
    const bool wifiJustConnected =
        (wifi.state == sys::WifiState::Connected && self->m_prevWifi != sys::WifiState::Connected);
    self->m_prevWifi = wifi.state;

    if (client.configured() && !client.isFetching() && wifi.state == sys::WifiState::Connected) {
        const auto snap = client.snapshot();
        const bool needFetch = !snap.valid || !snap.error.empty() || client.stale(1800);
        const int64_t nowUs = esp_timer_get_time();
        constexpr int64_t kRetryCooldownUs = 120LL * 1000000LL;
        if (needFetch && (wifiJustConnected || self->m_lastAutoAttemptUs == 0 ||
                          (nowUs - self->m_lastAutoAttemptUs) >= kRetryCooldownUs)) {
            self->m_lastAutoAttemptUs = nowUs;
            client.fetchAsync();
        }
    }
    self->refreshUi();
}

void WeatherApp::refreshCb(lv_event_t *event) {
    auto self = static_cast<WeatherApp *>(lv_event_get_user_data(event));
    if (!self) return;
    auto &client = core::WeatherClient::instance();
    if (!client.configured()) {
        self->showConfig(true);
        return;
    }
    self->m_lastAutoAttemptUs = esp_timer_get_time();
    if (sys::Connectivity::snapshot().state == sys::WifiState::Connected) {
        client.fetchAsync();
    }
    self->refreshUi();
}

void WeatherApp::cityCb(lv_event_t *event) {
    auto self = static_cast<WeatherApp *>(lv_event_get_user_data(event));
    if (!self) return;
    self->m_configAuto = false;
    self->showConfig(true);
}

void WeatherApp::saveCb(lv_event_t *event) {
    auto self = static_cast<WeatherApp *>(lv_event_get_user_data(event));
    if (self) self->saveConfig();
}

void WeatherApp::onShow() {
    if (m_timer) {
        lv_timer_set_period(m_timer, 1000);
        lv_timer_resume(m_timer);
    }
    updateHint();
    auto &client = core::WeatherClient::instance();
    client.init();
    const auto wifi = sys::Connectivity::snapshot();
    m_prevWifi = wifi.state;
    if (client.configured() && client.stale() && wifi.state == sys::WifiState::Connected) {
        m_lastAutoAttemptUs = esp_timer_get_time();
        client.fetchAsync();
    }
    refreshUi();
}

uint32_t WeatherApp::powerHints() const {
    // Stay lit and keep Wi-Fi alive (Modem-sleep) on the weather display so
    // background refreshes and disconnect alerts work; dim when quiet.
    if (m_config && !lv_obj_is_hidden(m_config)) return sys::power::None;
    return sys::power::StayLit | sys::power::LowLoad | sys::power::Dim | sys::power::KeepSession;
}

void WeatherApp::onPowerSaving(bool enabled) {
    if (m_timer) {
        lv_timer_set_period(m_timer, enabled ? 5000 : 1000);
    }
}

void WeatherApp::onHide() {
    if (m_timer) lv_timer_pause(m_timer);
}

void WeatherApp::onDestroy() {
    core::config::release();
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
}

bool WeatherApp::onBack() {
    if (m_config && !lv_obj_is_hidden(m_config)) {
        showConfig(false);
        return true;
    }
    return false;
}

} // namespace apps
