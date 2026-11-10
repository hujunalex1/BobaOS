#include "cubepaw_os.h"
#include "lvgl_theme.h"
#include <vector>
#include <string>
#include <cstdio>

namespace cubepaw {

struct PhotoEntry {
    const char* name;
    const char* emotion_key;
};

static const PhotoEntry kBuiltinPhotos[] = {
    {"01. 小玉·默认待机", "neutral"},
    {"02. 小玉·阳光开心", "happy"},
    {"03. 小玉·开怀大笑", "laughing"},
    {"04. 小玉·古灵精怪", "funny"},
    {"05. 小玉·满眼爱心", "loving"},
    {"06. 小玉·潮流墨镜", "cool"},
    {"07. 小玉·俏皮眨眼", "winking"},
    {"08. 小玉·认真思考", "thinking"},
    {"09. 小玉·惬意听歌", "relaxed"},
    {"10. 小玉·甜美飞吻", "kissy"},
    {"11. 小玉·美味舔舌", "delicious"},
    {"12. 小玉·自信满满", "confident"},
    {"13. 小玉·安静睡觉", "sleepy"},
    {"14. 小玉·吐舌调皮", "silly"},
};

struct GalleryState {
    int idx = 0;
    bool auto_slideshow = false;
    lv_obj_t* photo_img = nullptr;
    lv_obj_t* caption_lbl = nullptr;
    lv_obj_t* slide_btn_lbl = nullptr;
    lv_timer_t* slide_timer = nullptr;
};

static GalleryState g_gal;
static constexpr int kPhotoCount = sizeof(kBuiltinPhotos) / sizeof(kBuiltinPhotos[0]);

static void RefreshGalleryView() {
    if (!g_gal.caption_lbl) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s (%d/%d)",
             kBuiltinPhotos[g_gal.idx].name, g_gal.idx + 1, kPhotoCount);
    lv_label_set_text(g_gal.caption_lbl, buf);
    if (g_gal.slide_btn_lbl) {
        lv_label_set_text(g_gal.slide_btn_lbl, g_gal.auto_slideshow ? "轮播: 开" : "轮播: 关");
    }
    if (g_gal.photo_img) {
        auto* theme = LvglThemeManager::GetInstance().GetTheme("light");
        if (theme && theme->emoji_collection()) {
            auto* img = theme->emoji_collection()->GetEmojiImage(kBuiltinPhotos[g_gal.idx].emotion_key);
            if (img && img->image_dsc()) {
                lv_image_set_src(g_gal.photo_img, img->image_dsc());
            }
        }
    }
}

static void SlideshowTimerCb(lv_timer_t* timer) {
    (void)timer;
    if (!g_gal.auto_slideshow) return;
    g_gal.idx = (g_gal.idx + 1) % kPhotoCount;
    RefreshGalleryView();
}

static void OnGalleryDeleteCb(lv_event_t* e) {
    (void)e;
    if (g_gal.slide_timer) {
        lv_timer_del(g_gal.slide_timer);
        g_gal.slide_timer = nullptr;
    }
    g_gal.photo_img = nullptr;
    g_gal.caption_lbl = nullptr;
    g_gal.slide_btn_lbl = nullptr;
}

void BuildGalleryApp(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFF8E1), 0);
    lv_obj_add_event_cb(parent, OnGalleryDeleteCb, LV_EVENT_DELETE, nullptr);

    // 顶栏返回 + 自动轮播开关
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

    lv_obj_t* slide_btn = lv_btn_create(parent);
    lv_obj_set_size(slide_btn, 84, 30);
    lv_obj_align(slide_btn, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_obj_set_style_bg_color(slide_btn, lv_color_hex(0xFFB74D), 0);
    lv_obj_set_style_radius(slide_btn, 15, 0);
    lv_obj_set_ext_click_area(slide_btn, 10);
    lv_obj_add_event_cb(slide_btn, [](lv_event_t*) {
        g_gal.auto_slideshow = !g_gal.auto_slideshow;
        RefreshGalleryView();
    }, LV_EVENT_CLICKED, nullptr);
    g_gal.slide_btn_lbl = lv_label_create(slide_btn);
    lv_obj_set_style_text_color(g_gal.slide_btn_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_center(g_gal.slide_btn_lbl);

    // 相框画布区 (点击相框直接切换下一张)
    lv_obj_t* frame = lv_btn_create(parent);
    lv_obj_set_size(frame, 150, 120);
    lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_bg_color(frame, lv_color_hex(0xFFEA79), 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_border_width(frame, 3, 0);
    lv_obj_set_style_radius(frame, 16, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(frame, [](lv_event_t*) {
        g_gal.idx = (g_gal.idx + 1) % kPhotoCount;
        RefreshGalleryView();
    }, LV_EVENT_CLICKED, nullptr);

    g_gal.photo_img = lv_image_create(frame);
    lv_obj_center(g_gal.photo_img);
    lv_obj_clear_flag(g_gal.photo_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_gal.photo_img, LV_OBJ_FLAG_EVENT_BUBBLE);

    g_gal.caption_lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(g_gal.caption_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_gal.caption_lbl, LV_ALIGN_TOP_MID, 0, 168);

    // 左右翻页按钮
    lv_obj_t* prev_btn = lv_btn_create(parent);
    lv_obj_set_size(prev_btn, 90, 36);
    lv_obj_align(prev_btn, LV_ALIGN_BOTTOM_LEFT, 16, -6);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_ext_click_area(prev_btn, 10);
    lv_obj_add_event_cb(prev_btn, [](lv_event_t*) {
        g_gal.idx = (g_gal.idx - 1 + kPhotoCount) % kPhotoCount;
        RefreshGalleryView();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* prev_lbl = lv_label_create(prev_btn);
    lv_label_set_text(prev_lbl, "< 上一张");
    lv_obj_set_style_text_color(prev_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(prev_lbl);

    lv_obj_t* next_btn = lv_btn_create(parent);
    lv_obj_set_size(next_btn, 90, 36);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_RIGHT, -16, -6);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_ext_click_area(next_btn, 10);
    lv_obj_add_event_cb(next_btn, [](lv_event_t*) {
        g_gal.idx = (g_gal.idx + 1) % kPhotoCount;
        RefreshGalleryView();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* next_lbl = lv_label_create(next_btn);
    lv_label_set_text(next_lbl, "下一张 >");
    lv_obj_set_style_text_color(next_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_lbl);

    RefreshGalleryView();
    g_gal.slide_timer = lv_timer_create(SlideshowTimerCb, 3000, nullptr);
}

} // namespace cubepaw
