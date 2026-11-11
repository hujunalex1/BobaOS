#pragma once

#include "ui/app.hpp"
#include "lvgl.h"
#include <ctime>

namespace apps {

class DeepSeekApp : public ui::App {
public:
    const char *name() const override { return "DS用量"; }
    const char *icon() const override { return LV_SYMBOL_CHARGE; }
    bool handlesHorizontalGesture() const override { return true; }

    void onCreate(lv_obj_t *parent) override;
    void onShow() override;
    void onHide() override;
    void onDestroy() override;
    void onPowerSaving(bool enabled) override;

private:
    void createUi(lv_obj_t *parent);
    void updateUi();
    void switchView(int viewIdx);
    void triggerRefresh();

    static void timerCb(lv_timer_t *timer);
    static void touchCb(lv_event_t *e);

    lv_obj_t *m_root{nullptr};
    lv_timer_t *m_timer{nullptr};

    // Header elements
    lv_obj_t *m_headerSub{nullptr};
    lv_obj_t *m_statusPill{nullptr};
    lv_obj_t *m_statusDot{nullptr};
    lv_obj_t *m_statusText{nullptr};

    // Containers
    lv_obj_t *m_viewToken{nullptr};
    lv_obj_t *m_viewBalance{nullptr};
    lv_obj_t *m_viewUnconfigured{nullptr};

    // Token View widgets
    lv_obj_t *m_lblTodayToken{nullptr};
    lv_obj_t *m_lblTodayCost{nullptr};
    lv_obj_t *m_lblMonthTok{nullptr};
    lv_obj_t *m_lblPromptTok{nullptr};
    lv_obj_t *m_lblComplTok{nullptr};
    lv_obj_t *m_lblCacheHit{nullptr};

    // Balance View widgets
    lv_obj_t *m_lblTotalBal{nullptr};
    lv_obj_t *m_lblGrantedBal{nullptr};
    lv_obj_t *m_lblToppedBal{nullptr};
    lv_obj_t *m_lblModeTag{nullptr};

    // Unconfigured widgets
    lv_obj_t *m_lblIpGuide{nullptr};

    // Footer widgets
    lv_obj_t *m_lblFooterLeft{nullptr};
    lv_obj_t *m_lblFooterRight{nullptr};

    int m_currentView{0}; // 0 = Token, 1 = Balance, 2 = Unconfigured
    bool m_tracking{false};
    lv_coord_t m_pressStartX{0};
    lv_coord_t m_pressStartY{0};
    time_t m_lastUiUpdate{0};
};

} // namespace apps
