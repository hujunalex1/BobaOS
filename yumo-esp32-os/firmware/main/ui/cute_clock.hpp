#pragma once

#include "lvgl.h"
#include <time.h>

namespace ui {

class CuteClock {
public:
    void show(lv_obj_t *parent);
    void hide();
    void stop();
    void refresh(const tm &time, bool animated);

private:
    void create(lv_obj_t *parent);

    lv_obj_t *area{nullptr};

    // Header labels
    lv_obj_t *labelDatePill{nullptr};
    lv_obj_t *labelBatPill{nullptr};

    // Cute Bunny character elements
    lv_obj_t *bunnyBox{nullptr};
    lv_obj_t *leftEye{nullptr};
    lv_obj_t *rightEye{nullptr};

    // Time Card elements
    lv_obj_t *timeCard{nullptr};
    lv_obj_t *labelTime{nullptr};
    lv_obj_t *labelSec{nullptr};
    lv_obj_t *labelFoot{nullptr};

    int lastMin{-1};
    int lastSec{-1};
    int lastDay{-1};
    bool eyeBlinked{false};
};

} // namespace ui
