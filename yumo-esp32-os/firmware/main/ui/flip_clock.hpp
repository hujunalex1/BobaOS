#pragma once

#include "lvgl.h"
#include <time.h>

namespace ui {

class FlipClock {
public:
    void show(lv_obj_t *parent);
    void hide();
    void stop();
    void refresh(const tm &time, bool animated);

private:
    void create(lv_obj_t *parent);

    lv_obj_t *area{nullptr};

    // Header labels
    lv_obj_t *labelAmPm{nullptr};
    lv_obj_t *labelDate{nullptr};
    lv_obj_t *labelBrand{nullptr};

    // Flip cards
    lv_obj_t *hourCard{nullptr};
    lv_obj_t *hourLabel{nullptr};
    lv_obj_t *colonDot1{nullptr};
    lv_obj_t *colonDot2{nullptr};
    lv_obj_t *minuteCard{nullptr};
    lv_obj_t *minuteLabel{nullptr};

    // Bottom complications
    lv_obj_t *secBox{nullptr};
    lv_obj_t *secLabel{nullptr};
    lv_obj_t *infoBox{nullptr};
    lv_obj_t *infoLabel{nullptr};

    int lastHour{-1};
    int lastMin{-1};
    int lastSec{-1};
    int lastDay{-1};
};

} // namespace ui
