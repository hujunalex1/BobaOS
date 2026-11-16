#pragma once

#include "core/connectivity.hpp"
#include "ui/app.hpp"
#include <cstdint>

namespace apps {

class WeatherApp : public ui::App {
public:
    const char *name() const override { return "天气"; }
    const char *icon() const override;
    bool allowsScreenProtection() const override { return false; }
    uint32_t powerHints() const override;
    void onPowerSaving(bool enabled) override;

    void onCreate(lv_obj_t *parent) override;
    void onShow() override;
    void onHide() override;
    void onDestroy() override;
    bool onBack() override;

private:
    void buildMain(lv_obj_t *parent);
    void buildConfig(lv_obj_t *parent);
    void showConfig(bool show);
    void saveConfig();
    void updateHint();
    void loadFields();
    void refreshUi();
    void drawIcon(const char *text);

    static void timerCb(lv_timer_t *timer);
    static void refreshCb(lv_event_t *event);
    static void cityCb(lv_event_t *event);
    static void saveCb(lv_event_t *event);

    lv_obj_t *m_main{nullptr};
    lv_obj_t *m_config{nullptr};
    lv_timer_t *m_timer{nullptr};

    lv_obj_t *m_city{nullptr};
    lv_obj_t *m_cityArrow{nullptr};
    lv_obj_t *m_netBadge{nullptr};
    lv_obj_t *m_netIcon{nullptr};
    lv_obj_t *m_refresh{nullptr};
    lv_obj_t *m_temp{nullptr};
    lv_obj_t *m_deg{nullptr};
    lv_obj_t *m_cond{nullptr};
    lv_obj_t *m_icon{nullptr};
    lv_obj_t *m_statLabels[3]{};
    lv_obj_t *m_statValues[3]{};
    lv_obj_t *m_footer{nullptr};
    lv_obj_t *m_footerTime{nullptr};
    lv_obj_t *m_hint{nullptr};
    bool m_configAuto{false};  // opened because nothing was configured yet
    char m_lastIcon[16]{};
    sys::WifiState m_prevWifi{sys::WifiState::Off};
    int64_t m_lastAutoAttemptUs{0};

    lv_obj_t *m_fHost{nullptr};
    lv_obj_t *m_fKey{nullptr};
    lv_obj_t *m_fLat{nullptr};
    lv_obj_t *m_fLon{nullptr};
    lv_obj_t *m_fCity{nullptr};
};

} // namespace apps
