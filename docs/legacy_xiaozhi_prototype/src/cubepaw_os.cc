#include "cubepaw_os.h"
#include <esp_log.h>
#include <sys/stat.h>

static const char* TAG = "CubePawOS";

namespace cubepaw {

CubePawOS& CubePawOS::GetInstance() {
    static CubePawOS instance;
    return instance;
}

void CubePawOS::Init(lv_obj_t* root_screen, const HardwareCallbacks& callbacks) {
    root_screen_ = root_screen;
    callbacks_ = callbacks;

    lv_obj_clear_flag(root_screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root_screen_, GlobalGestureEventCb, LV_EVENT_GESTURE, this);

    TryMountSdCard();
    OpenApp(AppId::WATCHFACE, LV_SCR_LOAD_ANIM_NONE);
    ESP_LOGI(TAG, "CubePaw OS initialized on 240x240 capacitive touch screen");
}

bool CubePawOS::TryMountSdCard() {
    struct stat st;
    if (stat("/sdcard", &st) == 0 && S_ISDIR(st.st_mode)) {
        sd_mounted_ = true;
        ESP_LOGI(TAG, "MicroSD card detected at /sdcard");
    } else {
        sd_mounted_ = false;
        ESP_LOGI(TAG, "No MicroSD card mounted, using built-in 16MB Flash assets");
    }
    return sd_mounted_;
}

void CubePawOS::OpenApp(AppId app_id, lv_scr_load_anim_t anim) {
    (void)anim;
    if (active_container_ != nullptr) {
        lv_obj_t* old = active_container_;
        active_container_ = nullptr;
        lv_obj_add_flag(old, LV_OBJ_FLAG_HIDDEN);
        lv_obj_delete_async(old);
    }

    current_app_ = app_id;
    active_container_ = lv_obj_create(root_screen_);
    lv_obj_set_size(active_container_, 240, 240);
    lv_obj_set_pos(active_container_, 0, 0);
    lv_obj_set_style_pad_all(active_container_, 0, 0);
    lv_obj_set_style_border_width(active_container_, 0, 0);
    lv_obj_set_style_radius(active_container_, 0, 0);
    lv_obj_clear_flag(active_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(active_container_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_move_foreground(active_container_);

    switch (app_id) {
        case AppId::WATCHFACE:
            BuildWatchfaceApp(active_container_);
            break;
        case AppId::LAUNCHER:
            BuildLauncherApp(active_container_);
            break;
        case AppId::AI_CHAT:
            BuildAiChatApp(active_container_);
            if (callbacks_.on_enter_ai_chat) {
                callbacks_.on_enter_ai_chat();
            }
            break;
        case AppId::MUSIC_PLAYER:
            if (callbacks_.on_stop_ai_speaking) callbacks_.on_stop_ai_speaking();
            BuildMusicApp(active_container_);
            break;
        case AppId::PHOTO_GALLERY:
            if (callbacks_.on_stop_ai_speaking) callbacks_.on_stop_ai_speaking();
            BuildGalleryApp(active_container_);
            break;
        case AppId::MINI_GAMES:
            if (callbacks_.on_stop_ai_speaking) callbacks_.on_stop_ai_speaking();
            BuildGamesApp(active_container_);
            break;
        case AppId::SETTINGS:
            BuildSettingsApp(active_container_);
            break;
    }
}

void CubePawOS::NavigateBack() {
    if (current_app_ == AppId::LAUNCHER) {
        OpenApp(AppId::WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    } else if (current_app_ != AppId::WATCHFACE) {
        OpenApp(AppId::LAUNCHER, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

void CubePawOS::GlobalGestureEventCb(lv_event_t* e) {
    auto* os = static_cast<CubePawOS*>(lv_event_get_user_data(e));
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

    if (os->current_app_ == AppId::WATCHFACE && dir == LV_DIR_LEFT) {
        os->OpenApp(AppId::LAUNCHER, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    } else if (os->current_app_ == AppId::LAUNCHER && dir == LV_DIR_RIGHT) {
        os->OpenApp(AppId::WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    } else if (os->current_app_ != AppId::WATCHFACE &&
               os->current_app_ != AppId::MINI_GAMES &&
               dir == LV_DIR_RIGHT) {
        os->NavigateBack();
    }
}

} // namespace cubepaw
