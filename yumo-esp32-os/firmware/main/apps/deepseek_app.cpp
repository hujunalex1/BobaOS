#include "deepseek_app.hpp"
#include "core/deepseek_client.hpp"
#include "core/connectivity.hpp"
#include "ui/compact_ui.hpp"
#include "ui/theme.hpp"

#include <cstdio>
#include <cmath>

namespace apps {

namespace c = ui::compact;
namespace t = ui::theme;

void DeepSeekApp::onCreate(lv_obj_t *parent) {
    m_root = c::panel(parent, 240, 240);
    lv_obj_set_pos(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 0, 0);
    lv_obj_set_style_radius(m_root, 0, 0);
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_bg_color(m_root, lv_color_hex(0x0A0D14), 0);
    lv_obj_set_style_bg_opa(m_root, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(m_root, false);
    lv_obj_set_clickable(m_root, true);

    createUi(m_root);

    core::DeepSeekClient::instance().init();

    lv_obj_add_event_cb(m_root, touchCb, LV_EVENT_ALL, this);

    m_timer = lv_timer_create(timerCb, 1000, this);

    // Initial fetch if has credentials
    if (core::DeepSeekClient::instance().hasCredentials()) {
        triggerRefresh();
    }
    updateUi();
}

void DeepSeekApp::createUi(lv_obj_t *parent) {
    // --- 1. Header (y = 6, h = 24) ---
    auto headerBox = c::panel(parent, 220, 24);
    lv_obj_set_pos(headerBox, 10, 6);
    lv_obj_set_style_pad_all(headerBox, 0, 0);
    lv_obj_set_style_border_width(headerBox, 0, 0);
    lv_obj_set_style_bg_opa(headerBox, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(headerBox, LV_OBJ_FLAG_CLICKABLE);

    // DeepSeek icon + Title on the same line
    auto iconBox = c::panel(headerBox, 18, 18);
    lv_obj_set_pos(iconBox, 0, 3);
    lv_obj_set_style_pad_all(iconBox, 0, 0);
    lv_obj_set_style_radius(iconBox, 4, 0);
    lv_obj_set_style_bg_color(iconBox, lv_color_hex(0x1D4ED8), 0);
    lv_obj_set_style_border_width(iconBox, 0, 0);
    auto iconLbl = c::label(iconBox, LV_SYMBOL_CHARGE, 18);
    lv_obj_center(iconLbl);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(iconLbl, lv_color_hex(0xFFFFFF), 0);

    auto brandLbl = lv_label_create(headerBox);
    lv_label_set_text(brandLbl, "DeepSeek");
    lv_obj_set_pos(brandLbl, 22, 3);
    lv_obj_set_style_text_font(brandLbl, t::fontSmall(), 0);
    lv_obj_set_style_text_color(brandLbl, lv_color_hex(0xFFFFFF), 0);

    m_headerSub = lv_label_create(headerBox);
    lv_label_set_text(m_headerSub, "· 用量");
    lv_obj_set_style_text_font(m_headerSub, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_headerSub, lv_color_hex(0x60A5FA), 0);
    lv_obj_align_to(m_headerSub, brandLbl, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    // Status Pill on top-right (w = 66, h = 20). Wide enough for the 3-char
    // states ("同步中"/"待配置") without wrapping inside a 60px pill.
    m_statusPill = c::panel(headerBox, 66, 20);
    lv_obj_set_pos(m_statusPill, 154, 2);
    lv_obj_set_style_pad_all(m_statusPill, 0, 0);
    lv_obj_set_style_radius(m_statusPill, 10, 0);
    lv_obj_set_style_bg_color(m_statusPill, lv_color_hex(0x13231B), 0);
    lv_obj_set_style_border_width(m_statusPill, 1, 0);
    lv_obj_set_style_border_color(m_statusPill, lv_color_hex(0x059669), 0);

    m_statusDot = c::panel(m_statusPill, 6, 6);
    lv_obj_set_pos(m_statusDot, 6, 7);
    lv_obj_set_style_radius(m_statusDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_border_width(m_statusDot, 0, 0);

    m_statusText = c::label(m_statusPill, "在线", 50);
    lv_obj_set_pos(m_statusText, 16, 2);
    lv_obj_set_style_text_font(m_statusText, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_statusText, lv_color_hex(0x34D399), 0);

    // --- 2. VIEW A: Token Usage View (y = 34, w = 220, h = 176) ---
    m_viewToken = c::panel(parent, 220, 176);
    lv_obj_set_pos(m_viewToken, 10, 34);
    lv_obj_set_style_pad_all(m_viewToken, 0, 0);
    lv_obj_set_style_border_width(m_viewToken, 0, 0);
    lv_obj_set_style_bg_opa(m_viewToken, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(m_viewToken, LV_OBJ_FLAG_CLICKABLE);

    // Hero Token Card (h = 84)
    auto tokHero = c::panel(m_viewToken, 220, 84);
    lv_obj_set_pos(tokHero, 0, 0);
    lv_obj_set_style_pad_all(tokHero, 0, 0);
    lv_obj_set_style_radius(tokHero, 12, 0);
    lv_obj_set_style_bg_color(tokHero, lv_color_hex(0x13192B), 0);
    lv_obj_set_style_bg_opa(tokHero, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tokHero, 1, 0);
    lv_obj_set_style_border_color(tokHero, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(tokHero, LV_OBJ_FLAG_CLICKABLE);

    auto lblTokTitle = c::label(tokHero, "今日消耗", 100);
    lv_obj_set_pos(lblTokTitle, 12, 8);
    lv_obj_set_style_text_font(lblTokTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblTokTitle, lv_color_hex(0x94A3B8), 0);

    m_lblTodayCost = c::label(tokHero, "", 98);
    lv_obj_set_pos(m_lblTodayCost, 110, 8);
    lv_obj_set_style_text_align(m_lblTodayCost, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblTodayCost, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblTodayCost, lv_color_hex(0xA5B4FC), 0);

    m_lblTodayToken = lv_label_create(tokHero);
    lv_obj_set_pos(m_lblTodayToken, 12, 28);
    lv_obj_set_style_text_font(m_lblTodayToken, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(m_lblTodayToken, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(m_lblTodayToken, "");

    // Unit suffix: deliberately smaller than the value so "0 Token" never
    // renders as one oversized run, and aligned to the value's right edge so
    // it follows the number whether it is "0" or "1.20 M".
    m_lblTokenUnit = c::label(tokHero, "Token");
    lv_obj_set_style_text_font(m_lblTokenUnit, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblTokenUnit, lv_color_hex(0x94A3B8), 0);

    m_lblMonthTok = c::label(tokHero, "", 196);
    lv_obj_set_pos(m_lblMonthTok, 12, 58);
    lv_obj_set_style_text_font(m_lblMonthTok, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblMonthTok, lv_color_hex(0x64748B), 0);

    // Secondary Bento Container (h = 84)
    auto tokDetail = c::panel(m_viewToken, 220, 84);
    lv_obj_set_pos(tokDetail, 0, 92);
    lv_obj_set_style_pad_all(tokDetail, 0, 0);
    lv_obj_set_style_radius(tokDetail, 12, 0);
    lv_obj_set_style_bg_color(tokDetail, lv_color_hex(0x101522), 0);
    lv_obj_set_style_bg_opa(tokDetail, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tokDetail, 1, 0);
    lv_obj_set_style_border_color(tokDetail, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(tokDetail, LV_OBJ_FLAG_CLICKABLE);

    // Left column: Prompt
    auto lblPTitle = c::label(tokDetail, "输入 Token", 90);
    lv_obj_set_pos(lblPTitle, 12, 8);
    lv_obj_set_style_text_font(lblPTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblPTitle, lv_color_hex(0x818CF8), 0);

    m_lblPromptTok = lv_label_create(tokDetail);
    lv_obj_set_pos(m_lblPromptTok, 12, 30);
    lv_obj_set_style_text_font(m_lblPromptTok, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblPromptTok, lv_color_hex(0xF1F5F9), 0);
    lv_label_set_text(m_lblPromptTok, "");

    // Right column: Completion
    auto lblCTitle = c::label(tokDetail, "输出 Token", 90);
    lv_obj_set_pos(lblCTitle, 116, 8);
    lv_obj_set_style_text_font(lblCTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblCTitle, lv_color_hex(0x34D399), 0);

    m_lblComplTok = lv_label_create(tokDetail);
    lv_obj_set_pos(m_lblComplTok, 116, 30);
    lv_obj_set_style_text_font(m_lblComplTok, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblComplTok, lv_color_hex(0xF1F5F9), 0);
    lv_label_set_text(m_lblComplTok, "");

    // Bottom row: Cache Hit
    auto cLabel = c::label(tokDetail, "缓存命中率", 90);
    lv_obj_set_pos(cLabel, 12, 56);
    lv_obj_set_style_text_font(cLabel, t::fontSmall(), 0);
    lv_obj_set_style_text_color(cLabel, lv_color_hex(0x64748B), 0);

    m_lblCacheHit = c::label(tokDetail, "", 92);
    lv_obj_set_pos(m_lblCacheHit, 116, 56);
    lv_obj_set_style_text_align(m_lblCacheHit, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblCacheHit, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblCacheHit, lv_color_hex(0x38BDF8), 0);

    // --- 3. VIEW B: Balance View (y = 34, w = 220, h = 176) ---
    m_viewBalance = c::panel(parent, 220, 176);
    lv_obj_set_pos(m_viewBalance, 10, 34);
    lv_obj_set_style_pad_all(m_viewBalance, 0, 0);
    lv_obj_set_style_border_width(m_viewBalance, 0, 0);
    lv_obj_set_style_bg_opa(m_viewBalance, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(m_viewBalance, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_hidden(m_viewBalance, true);

    auto balHero = c::panel(m_viewBalance, 220, 84);
    lv_obj_set_pos(balHero, 0, 0);
    lv_obj_set_style_pad_all(balHero, 0, 0);
    lv_obj_set_style_radius(balHero, 12, 0);
    lv_obj_set_style_bg_color(balHero, lv_color_hex(0x0E1B2E), 0);
    lv_obj_set_style_border_width(balHero, 1, 0);
    lv_obj_set_style_border_color(balHero, lv_color_hex(0x1D4ED8), 0);
    lv_obj_clear_flag(balHero, LV_OBJ_FLAG_CLICKABLE);

    auto lblBalTitle = c::label(balHero, "可用总额度", 100);
    lv_obj_set_pos(lblBalTitle, 12, 8);
    lv_obj_set_style_text_font(lblBalTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblBalTitle, lv_color_hex(0x93C5FD), 0);

    m_lblModeTag = c::label(balHero, "CNY 账户", 98);
    lv_obj_set_pos(m_lblModeTag, 110, 8);
    lv_obj_set_style_text_align(m_lblModeTag, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblModeTag, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblModeTag, lv_color_hex(0x60A5FA), 0);

    m_lblTotalBal = c::label(balHero, "CNY 0.00", 196);
    lv_obj_set_pos(m_lblTotalBal, 12, 28);
    lv_obj_set_style_text_font(m_lblTotalBal, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(m_lblTotalBal, lv_color_hex(0xBAE6FD), 0);

    auto lblBalState = c::label(balHero, "平台额度校验正常", 196);
    lv_obj_set_pos(lblBalState, 12, 58);
    lv_obj_set_style_text_font(lblBalState, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblBalState, lv_color_hex(0x34D399), 0);

    // Balance Detail Container
    auto balDetail = c::panel(m_viewBalance, 220, 84);
    lv_obj_set_pos(balDetail, 0, 92);
    lv_obj_set_style_pad_all(balDetail, 0, 0);
    lv_obj_set_style_radius(balDetail, 12, 0);
    lv_obj_set_style_bg_color(balDetail, lv_color_hex(0x101522), 0);
    lv_obj_set_style_border_width(balDetail, 1, 0);
    lv_obj_set_style_border_color(balDetail, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(balDetail, LV_OBJ_FLAG_CLICKABLE);

    // Left col: Topped-up
    auto lblTTitle = c::label(balDetail, "充值本金", 90);
    lv_obj_set_pos(lblTTitle, 12, 8);
    lv_obj_set_style_text_font(lblTTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblTTitle, lv_color_hex(0x60A5FA), 0);

    m_lblToppedBal = c::label(balDetail, "CNY 0.00", 90);
    lv_obj_set_pos(m_lblToppedBal, 12, 28);
    lv_obj_set_style_text_font(m_lblToppedBal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblToppedBal, lv_color_hex(0xF1F5F9), 0);

    // Right col: Granted
    auto lblGTitle = c::label(balDetail, "赠送额度", 90);
    lv_obj_set_pos(lblGTitle, 116, 8);
    lv_obj_set_style_text_font(lblGTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblGTitle, lv_color_hex(0x34D399), 0);

    m_lblGrantedBal = c::label(balDetail, "CNY 0.00", 90);
    lv_obj_set_pos(m_lblGrantedBal, 116, 28);
    lv_obj_set_style_text_font(m_lblGrantedBal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblGrantedBal, lv_color_hex(0xF1F5F9), 0);

    // Bottom row: Billing info
    auto lblBillMode = c::label(balDetail, "计费数据源", 90);
    lv_obj_set_pos(lblBillMode, 12, 56);
    lv_obj_set_style_text_font(lblBillMode, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblBillMode, lv_color_hex(0x64748B), 0);

    auto lblBillVal = c::label(balDetail, "官方账单已同步", 116);
    lv_obj_set_pos(lblBillVal, 100, 56);
    lv_obj_set_style_text_align(lblBillVal, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(lblBillVal, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblBillVal, lv_color_hex(0x94A3B8), 0);

    // --- 4. VIEW C: Unconfigured View ---
    m_viewUnconfigured = c::panel(parent, 220, 176);
    lv_obj_set_pos(m_viewUnconfigured, 10, 34);
    lv_obj_set_style_pad_all(m_viewUnconfigured, 12, 0);
    lv_obj_set_style_radius(m_viewUnconfigured, 12, 0);
    lv_obj_set_style_bg_color(m_viewUnconfigured, lv_color_hex(0x111624), 0);
    lv_obj_set_style_border_width(m_viewUnconfigured, 1, 0);
    lv_obj_set_style_border_color(m_viewUnconfigured, lv_color_hex(0x2563EB), 0);
    lv_obj_clear_flag(m_viewUnconfigured, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_hidden(m_viewUnconfigured, true);

    auto uIcon = c::label(m_viewUnconfigured, LV_SYMBOL_SETTINGS, 40);
    lv_obj_set_pos(uIcon, 90, 8);
    lv_obj_set_style_text_font(uIcon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(uIcon, lv_color_hex(0x60A5FA), 0);

    auto uTitle = c::label(m_viewUnconfigured, "未配置 DeepSeek 凭据", 196);
    lv_obj_set_pos(uTitle, 0, 42);
    lv_obj_set_style_text_align(uTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(uTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(uTitle, lv_color_hex(0xFFFFFF), 0);

    auto uDesc = c::label(m_viewUnconfigured, "手机连热点, 浏览器访问:", 196);
    lv_obj_set_pos(uDesc, 0, 68);
    lv_obj_set_style_text_align(uDesc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(uDesc, t::fontSmall(), 0);
    lv_obj_set_style_text_color(uDesc, lv_color_hex(0x94A3B8), 0);

    m_lblIpGuide = c::label(m_viewUnconfigured, "http://<IP>/ds", 196);
    lv_obj_set_pos(m_lblIpGuide, 0, 96);
    lv_obj_set_style_text_align(m_lblIpGuide, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(m_lblIpGuide, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblIpGuide, lv_color_hex(0x38BDF8), 0);

    auto uSub = c::label(m_viewUnconfigured, "或SD卡放 deepseek.json", 196);
    lv_obj_set_pos(uSub, 0, 126);
    lv_obj_set_style_text_align(uSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(uSub, t::fontSmall(), 0);
    lv_obj_set_style_text_color(uSub, lv_color_hex(0x64748B), 0);

    // --- 5. Footer (y = 214, h = 20) ---
    auto footerBox = c::panel(parent, 220, 20);
    lv_obj_set_pos(footerBox, 10, 214);
    lv_obj_set_style_pad_all(footerBox, 0, 0);
    lv_obj_set_style_border_width(footerBox, 0, 0);
    lv_obj_set_style_bg_opa(footerBox, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(footerBox, LV_OBJ_FLAG_CLICKABLE);

    m_lblFooterLeft = c::label(footerBox, "[1/2] 切换", 110);
    lv_obj_set_pos(m_lblFooterLeft, 0, 2);
    lv_obj_set_style_text_font(m_lblFooterLeft, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblFooterLeft, lv_color_hex(0x64748B), 0);

    m_lblFooterRight = c::label(footerBox, LV_SYMBOL_REFRESH " 刷新", 100);
    lv_obj_set_pos(m_lblFooterRight, 120, 2);
    lv_obj_set_style_text_align(m_lblFooterRight, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblFooterRight, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblFooterRight, lv_color_hex(0x60A5FA), 0);
}

void DeepSeekApp::switchView(int viewIdx) {
    m_currentView = viewIdx % 2;
    lv_obj_set_hidden(m_viewToken, m_currentView != 0);
    lv_obj_set_hidden(m_viewBalance, m_currentView != 1);

    if (m_currentView == 0) {
        lv_label_set_text(m_headerSub, "· 用量");
        lv_obj_set_style_text_color(m_headerSub, lv_color_hex(0x60A5FA), 0);
        c::setText(m_lblFooterLeft, "[1/2] 切余额");
    } else {
        lv_label_set_text(m_headerSub, "· 余额");
        lv_obj_set_style_text_color(m_headerSub, lv_color_hex(0x38BDF8), 0);
        c::setText(m_lblFooterLeft, "[2/2] 切用量");
    }
}

void DeepSeekApp::triggerRefresh() {
    auto &client = core::DeepSeekClient::instance();
    if (!client.hasCredentials()) {
        lv_obj_set_hidden(m_viewToken, true);
        lv_obj_set_hidden(m_viewBalance, true);
        lv_obj_set_hidden(m_viewUnconfigured, false);
        return;
    }

    c::setText(m_statusText, "同步中");
    lv_obj_set_style_text_color(m_statusText, lv_color_hex(0x60A5FA), 0);
    lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0x3B82F6), 0);

    client.fetchAsync([this](bool success) {
        // UI updates handled on next timer tick
    });
}

void DeepSeekApp::setHeroValue(const char *value, const char *unit) {
    if (m_lblTodayToken) c::setText(m_lblTodayToken, value);
    if (!m_lblTokenUnit) return;
    c::setText(m_lblTokenUnit, unit);
    // Re-align every time: the value label is auto-sized, so its width changes
    // with "0" -> "12.4" -> "1.20 M".
    lv_obj_align_to(m_lblTokenUnit, m_lblTodayToken, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, 0);
}

void DeepSeekApp::updateUi() {
    auto &client = core::DeepSeekClient::instance();
    bool hasCred = client.hasCredentials();

    if (!hasCred) {
        lv_obj_set_hidden(m_viewToken, true);
        lv_obj_set_hidden(m_viewBalance, true);
        lv_obj_set_hidden(m_viewUnconfigured, false);

        c::setText(m_statusText, "待配置");
        lv_obj_set_style_text_color(m_statusText, lv_color_hex(0xFBBF24), 0);
        lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0xF59E0B), 0);

        std::string ip = sys::Connectivity::snapshot().ip;
        if (!ip.empty()) {
            char ipBuf[64];
            std::snprintf(ipBuf, sizeof(ipBuf), "http://%s/ds", ip.c_str());
            c::setText(m_lblIpGuide, ipBuf);
        }
        return;
    }

    lv_obj_set_hidden(m_viewUnconfigured, true);
    switchView(m_currentView);

    auto data = client.getData();

    // Status pill
    if (client.isFetching()) {
        c::setText(m_statusText, "同步中");
        lv_obj_set_style_text_color(m_statusText, lv_color_hex(0x60A5FA), 0);
        lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0x3B82F6), 0);
    } else if (data.valid) {
        if (data.totalBalance < 5.0 && data.totalBalance > 0.0) {
            c::setText(m_statusText, "低额");
            lv_obj_set_style_text_color(m_statusText, lv_color_hex(0xF87171), 0);
            lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0xEF4444), 0);
        } else {
            c::setText(m_statusText, "在线");
            lv_obj_set_style_text_color(m_statusText, lv_color_hex(0x34D399), 0);
            lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0x10B981), 0);
        }
    } else {
        c::setText(m_statusText, "异常");
        lv_obj_set_style_text_color(m_statusText, lv_color_hex(0xF87171), 0);
        lv_obj_set_style_bg_color(m_statusDot, lv_color_hex(0xEF4444), 0);
    }

    // 1. Token View update
    if (data.hasTokenStats) {
        char numBuf[24];
        const char *unit = "Token";
        if (data.todayTokens >= 1000000) {
            std::snprintf(numBuf, sizeof(numBuf), "%.2f", data.todayTokens / 1000000.0);
            unit = "M Token";
        } else if (data.todayTokens >= 1000) {
            std::snprintf(numBuf, sizeof(numBuf), "%.1f", data.todayTokens / 1000.0);
            unit = "k Token";
        } else {
            std::snprintf(numBuf, sizeof(numBuf), "%lld", (long long)data.todayTokens);
        }
        setHeroValue(numBuf, unit);

        char costBuf[32];
        if (data.costKnown) {
            std::snprintf(costBuf, sizeof(costBuf), "约 ¥ %.2f", data.todayCost);
        } else {
            std::snprintf(costBuf, sizeof(costBuf), "费用 --");
        }
        c::setText(m_lblTodayCost, costBuf);

        char pBuf[24];
        if (data.promptTokens >= 1000000) std::snprintf(pBuf, sizeof(pBuf), "%.2f M", data.promptTokens / 1000000.0);
        else if (data.promptTokens >= 1000) std::snprintf(pBuf, sizeof(pBuf), "%.1f k", data.promptTokens / 1000.0);
        else std::snprintf(pBuf, sizeof(pBuf), "%lld", (long long)data.promptTokens);
        c::setText(m_lblPromptTok, pBuf);

        char cBuf[24];
        if (data.completionTokens >= 1000000) std::snprintf(cBuf, sizeof(cBuf), "%.2f M", data.completionTokens / 1000000.0);
        else if (data.completionTokens >= 1000) std::snprintf(cBuf, sizeof(cBuf), "%.1f k", data.completionTokens / 1000.0);
        else std::snprintf(cBuf, sizeof(cBuf), "%lld", (long long)data.completionTokens);
        c::setText(m_lblComplTok, cBuf);

        char hitBuf[32];
        std::snprintf(hitBuf, sizeof(hitBuf), "%.1f%% Hit", data.cacheHitRate);
        c::setText(m_lblCacheHit, hitBuf);

        char mBuf[40];
        if (data.monthTokens >= 1000000) std::snprintf(mBuf, sizeof(mBuf), "本月累计: %.2f M", data.monthTokens / 1000000.0);
        else if (data.monthTokens >= 1000) std::snprintf(mBuf, sizeof(mBuf), "本月累计: %.1f k", data.monthTokens / 1000.0);
        else std::snprintf(mBuf, sizeof(mBuf), "本月累计: %lld", (long long)data.monthTokens);
        c::setText(m_lblMonthTok, mBuf);
    } else {
        // Fallback or guidance when only API Key is configured
        if (client.getUserToken().empty()) {
            setHeroValue("--", "");
            c::setText(m_lblTodayCost, "需平台Token");
            c::setText(m_lblPromptTok, "--");
            c::setText(m_lblComplTok, "--");
            c::setText(m_lblCacheHit, "--");
            c::setText(m_lblMonthTok, "访问 /ds 填入平台 Token");
        } else {
            // userToken was entered but the platform usage API returned nothing
            // usable. Never render zeros here: "0 Token" reads as "you spent 0"
            // when in fact the data never arrived.
            setHeroValue("--", "");
            c::setText(m_lblTodayCost, "用量不可用");
            c::setText(m_lblPromptTok, "--");
            c::setText(m_lblComplTok, "--");
            c::setText(m_lblCacheHit, "--");
            char whyBuf[96];
            std::snprintf(whyBuf, sizeof(whyBuf), "用量: %s",
                          data.platformError.empty() ? "平台未返回数据" : data.platformError.c_str());
            c::setText(m_lblMonthTok, whyBuf);
        }
    }

    // 2. Balance View update
    char balBuf[32];
    std::snprintf(balBuf, sizeof(balBuf), "CNY %.2f", data.totalBalance);
    c::setText(m_lblTotalBal, balBuf);

    char grtBuf[32];
    std::snprintf(grtBuf, sizeof(grtBuf), "CNY %.2f", data.grantedBalance);
    c::setText(m_lblGrantedBal, grtBuf);

    char topBuf[32];
    std::snprintf(topBuf, sizeof(topBuf), "CNY %.2f", data.toppedUpBalance);
    c::setText(m_lblToppedBal, topBuf);

    c::setText(m_lblModeTag, data.usingPlatformToken ? "平台授权" : "官方APIKey");
}

void DeepSeekApp::timerCb(lv_timer_t *timer) {
    auto self = static_cast<DeepSeekApp *>(lv_timer_get_user_data(timer));
    if (self) {
        self->updateUi();
    }
}

void DeepSeekApp::touchCb(lv_event_t *e) {
    auto self = static_cast<DeepSeekApp *>(lv_event_get_user_data(e));
    lv_indev_t *indev = lv_indev_active();
    if (!self || !indev) return;

    const lv_event_code_t code = lv_event_get_code(e);
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        self->m_pressStartX = (lv_coord_t)p.x;
        self->m_pressStartY = (lv_coord_t)p.y;
        self->m_tracking = true;
        return;
    }

    if (code == LV_EVENT_RELEASED && self->m_tracking) {
        self->m_tracking = false;
        int dx = (int)p.x - (int)self->m_pressStartX;
        int dy = (int)p.y - (int)self->m_pressStartY;
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;

        // Horizontal swipe: switch view
        if (adx >= 30 && adx > ady) {
            self->switchView(self->m_currentView + 1);
            return;
        }

        // Tap on footer or top: refresh
        if (adx < 15 && ady < 15) {
            if (p.y > 190 || p.y < 35) {
                self->triggerRefresh();
            } else {
                self->switchView(self->m_currentView + 1);
            }
        }
    }
}

void DeepSeekApp::onShow() {
    if (m_timer) lv_timer_resume(m_timer);
    if (core::DeepSeekClient::instance().hasCredentials()) {
        triggerRefresh();
    }
    updateUi();
}

void DeepSeekApp::onHide() {
    if (m_timer) lv_timer_pause(m_timer);
}

void DeepSeekApp::onDestroy() {
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
}

void DeepSeekApp::onPowerSaving(bool enabled) {
    if (m_timer) {
        lv_timer_set_period(m_timer, enabled ? 5000 : 1000);
    }
}

} // namespace apps
