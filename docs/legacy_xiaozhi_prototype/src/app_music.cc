#include "cubepaw_os.h"
#include <vector>
#include <string>
#include <dirent.h>
#include <cstdio>

namespace cubepaw {

struct MusicTrack {
    std::string title;
    std::string path;
    int duration_sec;
};

struct MusicAppState {
    std::vector<MusicTrack> playlist;
    int current_idx = 0;
    bool is_playing = false;
    int elapsed_sec = 0;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* source_label = nullptr;
    lv_obj_t* time_label = nullptr;
    lv_obj_t* play_btn_label = nullptr;
    lv_obj_t* progress_bar = nullptr;
    lv_timer_t* tick_timer = nullptr;
};

static MusicAppState g_music;

static void ScanMusicLibrary() {
    g_music.playlist.clear();
    if (CubePawOS::GetInstance().TryMountSdCard()) {
        DIR* dir = opendir("/sdcard/music");
        if (dir) {
            struct dirent* ent;
            while ((ent = readdir(dir)) != nullptr) {
                std::string name = ent->d_name;
                if (name.size() > 4 &&
                    (name.substr(name.size() - 4) == ".mp3" ||
                     name.substr(name.size() - 4) == ".wav")) {
                    g_music.playlist.push_back({
                        name.substr(0, name.size() - 4),
                        "/sdcard/music/" + name,
                        150
                    });
                }
            }
            closedir(dir);
        }
    }

    if (g_music.playlist.empty()) {
        g_music.playlist = {
            {"01. 一闪一闪亮晶晶 (八音盒)", "/spiffs/star.mp3", 95},
            {"02. 小玉的欢乐森林 (八音盒)", "/spiffs/forest.mp3", 120},
            {"03. 虫儿飞·睡前伴眠 (八音盒)", "/spiffs/lullaby.mp3", 140},
            {"04. 两只老虎欢乐颂 (八音盒)", "/spiffs/tiger.mp3", 85},
            {"05. 阳光彩虹小白马 (八音盒)", "/spiffs/rainbow.mp3", 160}
        };
    }
}

static void RefreshMusicUI() {
    if (!g_music.title_label || g_music.playlist.empty()) return;
    const auto& track = g_music.playlist[g_music.current_idx];
    lv_label_set_text(g_music.title_label, track.title.c_str());

    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d / %02d:%02d",
             g_music.elapsed_sec / 60, g_music.elapsed_sec % 60,
             track.duration_sec / 60, track.duration_sec % 60);
    lv_label_set_text(g_music.time_label, time_buf);

    int pct = (track.duration_sec > 0) ? (g_music.elapsed_sec * 100 / track.duration_sec) : 0;
    lv_bar_set_value(g_music.progress_bar, pct, LV_ANIM_OFF);
    lv_label_set_text(g_music.play_btn_label, g_music.is_playing ? "暂停" : "播放");
}

static void MusicTimerCb(lv_timer_t* timer) {
    (void)timer;
    if (!g_music.is_playing || g_music.playlist.empty()) return;
    g_music.elapsed_sec++;
    if (g_music.elapsed_sec % 4 == 0) {
        auto& cb = CubePawOS::GetInstance().Callbacks();
        if (cb.on_play_sound) cb.on_play_sound(g_music.current_idx);
    }
    if (g_music.elapsed_sec >= g_music.playlist[g_music.current_idx].duration_sec) {
        g_music.elapsed_sec = 0;
        g_music.current_idx = (g_music.current_idx + 1) % static_cast<int>(g_music.playlist.size());
    }
    RefreshMusicUI();
}

static void OnMusicDeleteCb(lv_event_t* e) {
    (void)e;
    g_music.is_playing = false;
    if (g_music.tick_timer) {
        lv_timer_del(g_music.tick_timer);
        g_music.tick_timer = nullptr;
    }
    g_music.title_label = nullptr;
}

void BuildMusicApp(lv_obj_t* parent) {
    ScanMusicLibrary();
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xE1F5FE), 0);
    lv_obj_add_event_cb(parent, OnMusicDeleteCb, LV_EVENT_DELETE, nullptr);

    // 顶部返回按钮
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 68, 30);
    lv_obj_set_pos(back_btn, 8, 6);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_radius(back_btn, 15, 0);
    lv_obj_set_ext_click_area(back_btn, 10);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        g_music.is_playing = false;
        CubePawOS::GetInstance().OpenApp(AppId::LAUNCHER);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< 桌面");
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_lbl);

    g_music.source_label = lv_label_create(parent);
    bool has_sd = CubePawOS::GetInstance().IsSdCardMounted();
    lv_label_set_text(g_music.source_label, has_sd ? "TF内存卡" : "内置八音盒");
    lv_obj_set_style_text_color(g_music.source_label, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_music.source_label, LV_ALIGN_TOP_RIGHT, -10, 12);

    // 中央黑胶唱片盘面
    lv_obj_t* disc = lv_obj_create(parent);
    lv_obj_set_size(disc, 84, 84);
    lv_obj_align(disc, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_radius(disc, 42, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(0xFFEA79), 0);
    lv_obj_set_style_border_width(disc, 6, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* disc_center = lv_label_create(disc);
    lv_label_set_text(disc_center, "小玉\n音乐盒");
    lv_obj_set_style_text_align(disc_center, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(disc_center, lv_color_hex(0xFFF6B3), 0);
    lv_obj_center(disc_center);

    // 曲目名称
    g_music.title_label = lv_label_create(parent);
    lv_obj_set_width(g_music.title_label, 220);
    lv_label_set_long_mode(g_music.title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(g_music.title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_music.title_label, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_music.title_label, LV_ALIGN_TOP_MID, 0, 130);

    // 进度条与时间
    g_music.progress_bar = lv_bar_create(parent);
    lv_obj_set_size(g_music.progress_bar, 200, 8);
    lv_obj_align(g_music.progress_bar, LV_ALIGN_TOP_MID, 0, 154);
    lv_bar_set_range(g_music.progress_bar, 0, 100);
    lv_obj_clear_flag(g_music.progress_bar, LV_OBJ_FLAG_CLICKABLE);

    g_music.time_label = lv_label_create(parent);
    lv_obj_set_style_text_color(g_music.time_label, lv_color_hex(0x525A8E), 0);
    lv_obj_align(g_music.time_label, LV_ALIGN_TOP_MID, 0, 166);

    // 底部播放控制三键 (上一首 / 播放暂停 / 下一首)
    lv_obj_t* prev_btn = lv_btn_create(parent);
    lv_obj_set_size(prev_btn, 64, 38);
    lv_obj_align(prev_btn, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x4FC3F7), 0);
    lv_obj_set_ext_click_area(prev_btn, 8);
    lv_obj_add_event_cb(prev_btn, [](lv_event_t*) {
        if (g_music.playlist.empty()) return;
        g_music.current_idx = (g_music.current_idx - 1 + static_cast<int>(g_music.playlist.size())) %
                              static_cast<int>(g_music.playlist.size());
        g_music.elapsed_sec = 0;
        g_music.is_playing = true;
        auto& cb = CubePawOS::GetInstance().Callbacks();
        if (cb.on_play_sound) cb.on_play_sound(g_music.current_idx);
        RefreshMusicUI();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* prev_lbl = lv_label_create(prev_btn);
    lv_label_set_text(prev_lbl, "上一首");
    lv_obj_set_style_text_color(prev_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_center(prev_lbl);

    lv_obj_t* play_btn = lv_btn_create(parent);
    lv_obj_set_size(play_btn, 76, 40);
    lv_obj_align(play_btn, LV_ALIGN_BOTTOM_MID, 0, -7);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0xFF8A80), 0);
    lv_obj_set_ext_click_area(play_btn, 8);
    lv_obj_add_event_cb(play_btn, [](lv_event_t*) {
        g_music.is_playing = !g_music.is_playing;
        if (g_music.is_playing) {
            auto& cb = CubePawOS::GetInstance().Callbacks();
            if (cb.on_play_sound) cb.on_play_sound(g_music.current_idx);
        }
        RefreshMusicUI();
    }, LV_EVENT_CLICKED, nullptr);
    g_music.play_btn_label = lv_label_create(play_btn);
    lv_obj_set_style_text_color(g_music.play_btn_label, lv_color_hex(0x262D5C), 0);
    lv_obj_center(g_music.play_btn_label);

    lv_obj_t* next_btn = lv_btn_create(parent);
    lv_obj_set_size(next_btn, 64, 38);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x4FC3F7), 0);
    lv_obj_set_ext_click_area(next_btn, 8);
    lv_obj_add_event_cb(next_btn, [](lv_event_t*) {
        if (g_music.playlist.empty()) return;
        g_music.current_idx = (g_music.current_idx + 1) % static_cast<int>(g_music.playlist.size());
        g_music.elapsed_sec = 0;
        g_music.is_playing = true;
        auto& cb = CubePawOS::GetInstance().Callbacks();
        if (cb.on_play_sound) cb.on_play_sound(g_music.current_idx);
        RefreshMusicUI();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* next_lbl = lv_label_create(next_btn);
    lv_label_set_text(next_lbl, "下一首");
    lv_obj_set_style_text_color(next_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_center(next_lbl);

    RefreshMusicUI();
    g_music.tick_timer = lv_timer_create(MusicTimerCb, 1000, nullptr);
}

} // namespace cubepaw
