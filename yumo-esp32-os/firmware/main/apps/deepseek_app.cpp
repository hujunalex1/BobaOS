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
    // --- 1. Header (y = 6) ---
    auto headerBox = c::panel(parent, 224, 28);
    lv_obj_set_pos(headerBox, 8, 6);
    lv_obj_set_style_pad_all(headerBox, 0, 0);
    lv_obj_set_style_border_width(headerBox, 0, 0);
    lv_obj_set_style_bg_opa(headerBox, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(headerBox, LV_OBJ_FLAG_CLICKABLE);

    // DeepSeek icon + Title
    auto iconBox = c::panel(headerBox, 20, 20);
    lv_obj_set_pos(iconBox, 0, 4);
    lv_obj_set_style_pad_all(iconBox, 0, 0);
    lv_obj_set_style_radius(iconBox, 5, 0);
    lv_obj_set_style_bg_color(iconBox, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_border_width(iconBox, 0, 0);
    auto iconLbl = c::label(iconBox, LV_SYMBOL_CHARGE, 20);
    lv_obj_center(iconLbl);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(iconLbl, lv_color_hex(0xFFFFFF), 0);

    auto brandLbl = c::label(headerBox, "DeepSeek", 80);
    lv_obj_set_pos(brandLbl, 24, 2);
    lv_obj_set_style_text_font(brandLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(brandLbl, lv_color_hex(0xFFFFFF), 0);

    m_headerSub = c::label(headerBox, "TOKEN 统计", 80);
    lv_obj_set_pos(m_headerSub, 24, 15);
    lv_obj_set_style_text_font(m_headerSub, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(m_headerSub, lv_color_hex(0x60A5FA), 0);

    // Status Pill on top-right
    m_statusPill = c::panel(headerBox, 58, 20);
    lv_obj_set_pos(m_statusPill, 166, 4);
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

    m_statusText = c::label(m_statusPill, "在线", 40);
    lv_obj_set_pos(m_statusText, 16, 3);
    lv_obj_set_style_text_font(m_statusText, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_statusText, lv_color_hex(0x34D399), 0);

    // --- 2. VIEW A: Token Usage View ---
    m_viewToken = c::panel(parent, 224, 166);
    lv_obj_set_pos(m_viewToken, 8, 38);
    lv_obj_set_style_pad_all(m_viewToken, 0, 0);
    lv_obj_set_style_border_width(m_viewToken, 0, 0);
    lv_obj_set_style_bg_opa(m_viewToken, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(m_viewToken, LV_OBJ_FLAG_CLICKABLE);

    // Hero Token Card
    auto tokHero = c::panel(m_viewToken, 224, 76);
    lv_obj_set_pos(tokHero, 0, 0);
    lv_obj_set_style_pad_all(tokHero, 8, 0);
    lv_obj_set_style_radius(tokHero, 12, 0);
    lv_obj_set_style_bg_color(tokHero, lv_color_hex(0x131728), 0);
    lv_obj_set_style_border_width(tokHero, 1, 0);
    lv_obj_set_style_border_color(tokHero, lv_color_hex(0x3730A3), 0);
    lv_obj_clear_flag(tokHero, LV_OBJ_FLAG_CLICKABLE);

    auto lblTokTitle = c::label(tokHero, "今日消耗 Token", 120);
    lv_obj_set_pos(lblTokTitle, 8, 6);
    lv_obj_set_style_text_font(lblTokTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblTokTitle, lv_color_hex(0x94A3B8), 0);

    m_lblTodayCost = c::label(tokHero, "≈ ¥ 0.00", 80);
    lv_obj_set_pos(m_lblTodayCost, 140, 6);
    lv_obj_set_style_text_align(m_lblTodayCost, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblTodayCost, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(m_lblTodayCost, lv_color_hex(0xA5B4FC), 0);

    m_lblTodayToken = c::label(tokHero, "0.00 M", 200);
    lv_obj_set_pos(m_lblTodayToken, 8, 28);
    lv_obj_set_style_text_font(m_lblTodayToken, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(m_lblTodayToken, lv_color_hex(0xC7D2FE), 0);

    // Bento Sub-metrics
    auto boxPrompt = c::panel(m_viewToken, 108, 48);
    lv_obj_set_pos(boxPrompt, 0, 82);
    lv_obj_set_style_pad_all(boxPrompt, 6, 0);
    lv_obj_set_style_radius(boxPrompt, 8, 0);
    lv_obj_set_style_bg_color(boxPrompt, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_width(boxPrompt, 1, 0);
    lv_obj_set_style_border_color(boxPrompt, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(boxPrompt, LV_OBJ_FLAG_CLICKABLE);

    auto lblPTitle = c::label(boxPrompt, "输入 (Prompt)", 100);
    lv_obj_set_style_text_font(lblPTitle, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblPTitle, lv_color_hex(0x818CF8), 0);
    m_lblPromptTok = c::label(boxPrompt, "0 k", 100);
    lv_obj_set_pos(m_lblPromptTok, 6, 22);
    lv_obj_set_style_text_font(m_lblPromptTok, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblPromptTok, lv_color_hex(0xF1F5F9), 0);

    auto boxCompl = c::panel(m_viewToken, 108, 48);
    lv_obj_set_pos(boxCompl, 116, 82);
    lv_obj_set_style_pad_all(boxCompl, 6, 0);
    lv_obj_set_style_radius(boxCompl, 8, 0);
    lv_obj_set_style_bg_color(boxCompl, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_width(boxCompl, 1, 0);
    lv_obj_set_style_border_color(boxCompl, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(boxCompl, LV_OBJ_FLAG_CLICKABLE);

    auto lblCTitle = c::label(boxCompl, "输出 (Output)", 100);
    lv_obj_set_style_text_font(lblCTitle, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblCTitle, lv_color_hex(0x34D399), 0);
    m_lblComplTok = c::label(boxCompl, "0 k", 100);
    lv_obj_set_pos(m_lblComplTok, 6, 22);
    lv_obj_set_style_text_font(m_lblComplTok, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblComplTok, lv_color_hex(0xF1F5F9), 0);

    // Cache Hit Capsule
    auto cacheCap = c::panel(m_viewToken, 224, 28);
    lv_obj_set_pos(cacheCap, 0, 136);
    lv_obj_set_style_pad_all(cacheCap, 4, 0);
    lv_obj_set_style_radius(cacheCap, 6, 0);
    lv_obj_set_style_bg_color(cacheCap, lv_color_hex(0x10141D), 0);
    lv_obj_set_style_border_width(cacheCap, 1, 0);
    lv_obj_set_style_border_color(cacheCap, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(cacheCap, LV_OBJ_FLAG_CLICKABLE);

    auto cLabel = c::label(cacheCap, "上下文缓存命中", 110);
    lv_obj_set_pos(cLabel, 8, 4);
    lv_obj_set_style_text_font(cLabel, t::fontSmall(), 0);
    lv_obj_set_style_text_color(cLabel, lv_color_hex(0x94A3B8), 0);

    m_lblCacheHit = c::label(cacheCap, "0.0% Hit", 90);
    lv_obj_set_pos(m_lblCacheHit, 126, 4);
    lv_obj_set_style_text_align(m_lblCacheHit, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblCacheHit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblCacheHit, lv_color_hex(0x34D399), 0);

    // --- 3. VIEW B: Balance View ---
    m_viewBalance = c::panel(parent, 224, 166);
    lv_obj_set_pos(m_viewBalance, 8, 38);
    lv_obj_set_style_pad_all(m_viewBalance, 0, 0);
    lv_obj_set_style_border_width(m_viewBalance, 0, 0);
    lv_obj_set_style_bg_opa(m_viewBalance, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(m_viewBalance, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_hidden(m_viewBalance, true);

    auto balHero = c::panel(m_viewBalance, 224, 76);
    lv_obj_set_pos(balHero, 0, 0);
    lv_obj_set_style_pad_all(balHero, 8, 0);
    lv_obj_set_style_radius(balHero, 12, 0);
    lv_obj_set_style_bg_color(balHero, lv_color_hex(0x0E1A2B), 0);
    lv_obj_set_style_border_width(balHero, 1, 0);
    lv_obj_set_style_border_color(balHero, lv_color_hex(0x1D4ED8), 0);
    lv_obj_clear_flag(balHero, LV_OBJ_FLAG_CLICKABLE);

    auto lblBalTitle = c::label(balHero, "总可用额度", 120);
    lv_obj_set_pos(lblBalTitle, 8, 6);
    lv_obj_set_style_text_font(lblBalTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblBalTitle, lv_color_hex(0x93C5FD), 0);

    m_lblModeTag = c::label(balHero, "平台Token模式", 90);
    lv_obj_set_pos(m_lblModeTag, 124, 6);
    lv_obj_set_style_text_align(m_lblModeTag, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblModeTag, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblModeTag, lv_color_hex(0x60A5FA), 0);

    m_lblTotalBal = c::label(balHero, "¥ 0.00", 200);
    lv_obj_set_pos(m_lblTotalBal, 8, 28);
    lv_obj_set_style_text_font(m_lblTotalBal, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(m_lblTotalBal, lv_color_hex(0xBAE6FD), 0);

    // Granted and Topped-up boxes
    auto boxGranted = c::panel(m_viewBalance, 108, 52);
    lv_obj_set_pos(boxGranted, 0, 82);
    lv_obj_set_style_pad_all(boxGranted, 6, 0);
    lv_obj_set_style_radius(boxGranted, 8, 0);
    lv_obj_set_style_bg_color(boxGranted, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_width(boxGranted, 1, 0);
    lv_obj_set_style_border_color(boxGranted, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(boxGranted, LV_OBJ_FLAG_CLICKABLE);

    auto lblGTitle = c::label(boxGranted, "赠送额度", 100);
    lv_obj_set_style_text_font(lblGTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblGTitle, lv_color_hex(0x34D399), 0);
    m_lblGrantedBal = c::label(boxGranted, "¥ 0.00", 100);
    lv_obj_set_pos(m_lblGrantedBal, 6, 24);
    lv_obj_set_style_text_font(m_lblGrantedBal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblGrantedBal, lv_color_hex(0xF1F5F9), 0);

    auto boxTopped = c::panel(m_viewBalance, 108, 52);
    lv_obj_set_pos(boxTopped, 116, 82);
    lv_obj_set_style_pad_all(boxTopped, 6, 0);
    lv_obj_set_style_radius(boxTopped, 8, 0);
    lv_obj_set_style_bg_color(boxTopped, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_width(boxTopped, 1, 0);
    lv_obj_set_style_border_color(boxTopped, lv_color_hex(0x1E293B), 0);
    lv_obj_clear_flag(boxTopped, LV_OBJ_FLAG_CLICKABLE);

    auto lblTTitle = c::label(boxTopped, "充值本金", 100);
    lv_obj_set_style_text_font(lblTTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(lblTTitle, lv_color_hex(0x60A5FA), 0);
    m_lblToppedBal = c::label(boxTopped, "¥ 0.00", 100);
    lv_obj_set_pos(m_lblToppedBal, 6, 24);
    lv_obj_set_style_text_font(m_lblToppedBal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblToppedBal, lv_color_hex(0xF1F5F9), 0);

    // Tip capsule
    auto tipCap = c::panel(m_viewBalance, 224, 24);
    lv_obj_set_pos(tipCap, 0, 140);
    lv_obj_set_style_pad_all(tipCap, 2, 0);
    lv_obj_set_style_radius(tipCap, 6, 0);
    lv_obj_set_style_bg_color(tipCap, lv_color_hex(0x10141D), 0);
    lv_obj_set_style_border_width(tipCap, 0, 0);
    auto tLbl = c::label(tipCap, "DeepSeek 官方平台账单已联动", 220);
    lv_obj_center(tLbl);
    lv_obj_set_style_text_align(tLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(tLbl, t::fontSmall(), 0);
    lv_obj_set_style_text_color(tLbl, lv_color_hex(0x64748B), 0);

    // --- 4. VIEW C: Unconfigured View ---
    m_viewUnconfigured = c::panel(parent, 224, 166);
    lv_obj_set_pos(m_viewUnconfigured, 8, 38);
    lv_obj_set_style_pad_all(m_viewUnconfigured, 12, 0);
    lv_obj_set_style_radius(m_viewUnconfigured, 12, 0);
    lv_obj_set_style_bg_color(m_viewUnconfigured, lv_color_hex(0x111624), 0);
    lv_obj_set_style_border_width(m_viewUnconfigured, 1, 0);
    lv_obj_set_style_border_color(m_viewUnconfigured, lv_color_hex(0x2563EB), 0);
    lv_obj_clear_flag(m_viewUnconfigured, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_hidden(m_viewUnconfigured, true);

    auto uIcon = c::label(m_viewUnconfigured, LV_SYMBOL_SETTINGS, 40);
    lv_obj_set_pos(uIcon, 92, 10);
    lv_obj_set_style_text_font(uIcon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(uIcon, lv_color_hex(0x60A5FA), 0);

    auto uTitle = c::label(m_viewUnconfigured, "未配置 DeepSeek 凭据", 200);
    lv_obj_set_pos(uTitle, 0, 42);
    lv_obj_set_style_text_align(uTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(uTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(uTitle, lv_color_hex(0xFFFFFF), 0);

    auto uDesc = c::label(m_viewUnconfigured, "在同一WiFi手机访问配置页:", 200);
    lv_obj_set_pos(uDesc, 0, 68);
    lv_obj_set_style_text_align(uDesc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(uDesc, t::fontSmall(), 0);
    lv_obj_set_style_text_color(uDesc, lv_color_hex(0x94A3B8), 0);

    m_lblIpGuide = c::label(m_viewUnconfigured, "http://<IP>/ds", 200);
    lv_obj_set_pos(m_lblIpGuide, 0, 96);
    lv_obj_set_style_text_align(m_lblIpGuide, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(m_lblIpGuide, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblIpGuide, lv_color_hex(0x38BDF8), 0);

    auto uSub = c::label(m_viewUnconfigured, "或在SD卡放入 deepseek.json", 200);
    lv_obj_set_pos(uSub, 0, 126);
    lv_obj_set_style_text_align(uSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(uSub, t::fontSmall(), 0);
    lv_obj_set_style_text_color(uSub, lv_color_hex(0x64748B), 0);

    // --- 5. Footer (y = 210) ---
    auto footerBox = c::panel(parent, 224, 22);
    lv_obj_set_pos(footerBox, 8, 210);
    lv_obj_set_style_pad_all(footerBox, 0, 0);
    lv_obj_set_style_border_width(footerBox, 0, 0);
    lv_obj_set_style_bg_opa(footerBox, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(footerBox, LV_OBJ_FLAG_CLICKABLE);

    m_lblFooterLeft = c::label(footerBox, "本月: 0.0M", 120);
    lv_obj_set_pos(m_lblFooterLeft, 0, 3);
    lv_obj_set_style_text_font(m_lblFooterLeft, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblFooterLeft, lv_color_hex(0x94A3B8), 0);

    m_lblFooterRight = c::label(footerBox, "左右滑屏切换 ⇄", 100);
    lv_obj_set_pos(m_lblFooterRight, 124, 3);
    lv_obj_set_style_text_align(m_lblFooterRight, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblFooterRight, t::fontSmall(), 0);
    lv_obj_set_style_text_color(m_lblFooterRight, lv_color_hex(0x60A5FA), 0);
}

void DeepSeekApp::switchView(int viewIdx) {
    m_currentView = viewIdx % 2;
    lv_obj_set_hidden(m_viewToken, m_currentView != 0);
    lv_obj_set_hidden(m_viewBalance, m_currentView != 1);

    if (m_currentView == 0) {
        c::setText(m_headerSub, "TOKEN 统计");
        lv_obj_set_style_text_color(m_headerSub, lv_color_hex(0x818CF8), 0);
    } else {
        c::setText(m_headerSub, "资产余额");
        lv_obj_set_style_text_color(m_headerSub, lv_color_hex(0x38BDF8), 0);
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
    char tokBuf[32];
    if (data.todayTokens >= 1000000) {
        std::snprintf(tokBuf, sizeof(tokBuf), "%.2f M", data.todayTokens / 1000000.0);
    } else if (data.todayTokens >= 1000) {
        std::snprintf(tokBuf, sizeof(tokBuf), "%.1f k", data.todayTokens / 1000.0);
    } else {
        std::snprintf(tokBuf, sizeof(tokBuf), "%lld", (long long)data.todayTokens);
    }
    c::setText(m_lblTodayToken, tokBuf);

    char costBuf[32];
    std::snprintf(costBuf, sizeof(costBuf), "≈ ¥ %.2f", data.todayCost);
    c::setText(m_lblTodayCost, costBuf);

    char pBuf[32];
    if (data.promptTokens >= 1000000) std::snprintf(pBuf, sizeof(pBuf), "%.2f M", data.promptTokens / 1000000.0);
    else std::snprintf(pBuf, sizeof(pBuf), "%.1f k", data.promptTokens / 1000.0);
    c::setText(m_lblPromptTok, pBuf);

    char cBuf[32];
    if (data.completionTokens >= 1000000) std::snprintf(cBuf, sizeof(cBuf), "%.2f M", data.completionTokens / 1000000.0);
    else std::snprintf(cBuf, sizeof(cBuf), "%.1f k", data.completionTokens / 1000.0);
    c::setText(m_lblComplTok, cBuf);

    char hitBuf[32];
    std::snprintf(hitBuf, sizeof(hitBuf), "%.1f%% Hit", data.cacheHitRate);
    c::setText(m_lblCacheHit, hitBuf);

    // 2. Balance View update
    char balBuf[32];
    std::snprintf(balBuf, sizeof(balBuf), "¥ %.2f", data.totalBalance);
    c::setText(m_lblTotalBal, balBuf);

    char grtBuf[32];
    std::snprintf(grtBuf, sizeof(grtBuf), "¥ %.2f", data.grantedBalance);
    c::setText(m_lblGrantedBal, grtBuf);

    char topBuf[32];
    std::snprintf(topBuf, sizeof(topBuf), "¥ %.2f", data.toppedUpBalance);
    c::setText(m_lblToppedBal, topBuf);

    c::setText(m_lblModeTag, data.usingPlatformToken ? "平台Token模式" : "标准API模式");

    // 3. Footer update
    char mBuf[32];
    if (data.monthTokens >= 1000000) std::snprintf(mBuf, sizeof(mBuf), "本月: %.1fM", data.monthTokens / 1000000.0);
    else std::snprintf(mBuf, sizeof(mBuf), "本月: %.0fk", data.monthTokens / 1000.0);
    c::setText(m_lblFooterLeft, mBuf);
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
