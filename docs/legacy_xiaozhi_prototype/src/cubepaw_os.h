#pragma once

#include <lvgl.h>
#include <string>
#include <functional>

namespace cubepaw {

enum class AppId {
    WATCHFACE = 0,   // 默认首页：小玉互动时钟表盘
    LAUNCHER,        // 左滑进入：6宫格纯图标应用抽屉
    AI_CHAT,         // 小玉 AI 语音助手（桥接 xiaozhi 核心状态机）
    MUSIC_PLAYER,    // 音乐播放器（TF卡 + 内置 Flash）
    PHOTO_GALLERY,   // 小玉与家庭相册（支持手动翻页 + 幻灯片轮播）
    MINI_GAMES,      // 4合1 游戏厅（接星星、2048、打砖块、陀螺仪迷宫/骰子）
    SETTINGS         // 网络连接、音量、屏幕亮度、防打断开关
};

struct HardwareCallbacks {
    std::function<void()> on_enter_ai_chat;
    std::function<void()> on_stop_ai_speaking;
    std::function<void(int)> on_set_volume;       // 0 ~ 100
    std::function<int()> on_get_volume;
    std::function<void(int)> on_set_brightness;   // 10 ~ 100
    std::function<int()> on_get_brightness;
    std::function<void()> on_reset_wifi;
    std::function<bool()> on_is_wifi_connected;
    std::function<std::string()> on_get_wifi_ssid;
    std::function<std::string()> on_get_ip_address;
    std::function<int()> on_get_battery_percent;
    std::function<void(int)> on_play_sound;
    // QMI8658 六轴传感器读取 (ax, ay, az 单位 g)
    std::function<void(float&, float&, float&)> on_read_imu_accel;
};

class CubePawOS {
public:
    static CubePawOS& GetInstance();

    void Init(lv_obj_t* root_screen, const HardwareCallbacks& callbacks);
    void OpenApp(AppId app_id, lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT);
    void NavigateBack();

    AppId CurrentApp() const { return current_app_; }
    const HardwareCallbacks& Callbacks() const { return callbacks_; }

    // 供 xiaozhi 语音状态机查询：当前是否允许语音打断（防喇叭回声自激）
    bool IsAntiEchoMuteEnabled() const { return anti_echo_mute_; }
    void SetAntiEchoMuteEnabled(bool enabled) { anti_echo_mute_ = enabled; }

    // 检查 TF 卡是否已挂载
    bool IsSdCardMounted() const { return sd_mounted_; }
    bool TryMountSdCard();

private:
    CubePawOS() = default;
    static void GlobalGestureEventCb(lv_event_t* e);

    lv_obj_t* root_screen_ = nullptr;
    lv_obj_t* active_container_ = nullptr;
    AppId current_app_ = AppId::WATCHFACE;
    HardwareCallbacks callbacks_;
    bool anti_echo_mute_ = true;  // 默认开启“说话时自动闭麦防乱说话”
    bool sd_mounted_ = false;
};

// 各个子应用的构建入口
void BuildWatchfaceApp(lv_obj_t* parent);
void BuildLauncherApp(lv_obj_t* parent);
void BuildAiChatApp(lv_obj_t* parent);
void BuildMusicApp(lv_obj_t* parent);
void BuildGalleryApp(lv_obj_t* parent);
void BuildGamesApp(lv_obj_t* parent);
void BuildSettingsApp(lv_obj_t* parent);

} // namespace cubepaw
