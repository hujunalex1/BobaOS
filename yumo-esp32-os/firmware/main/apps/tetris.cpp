#include "tetris.hpp"
#include "ui/compact_ui.hpp"
#include "ui/theme.hpp"
#include "ui/window_mgr.hpp"
#include "core/button_mgr.hpp"
#include "core/event_bus.hpp"
#include "core/power_mgr.hpp"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace apps {

namespace c = ui::compact;
namespace t = ui::theme;

namespace {

uint32_t nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

struct BlockOffset { int8_t x, y; };

// 7 pieces x 4 rotations x 4 block coordinates
static constexpr BlockOffset kShapes[7][4][4] = {
    // 0: I (Cyan)
    {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}}
    },
    // 1: J (Blue)
    {
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {0, 2}, {1, 2}}
    },
    // 2: L (Orange)
    {
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}}
    },
    // 3: O (Yellow)
    {
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}}
    },
    // 4: S (Green)
    {
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}}
    },
    // 5: T (Purple)
    {
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {1, 2}}
    },
    // 6: Z (Red)
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {0, 2}}
    }
};

struct BlockPalette {
    uint16_t base;
    uint16_t light;
    uint16_t dark;
};

static constexpr uint16_t ColorBg = 0x10A3;       // #11141A Deep dark space
static constexpr uint16_t ColorGrid = 0x1926;     // #1A212E Subtle grid
static constexpr uint16_t ColorFlash = 0xFFFF;    // Pure white line flash

static constexpr BlockPalette kPalettes[8] = {
    {ColorBg, ColorBg, ColorBg}, // None
    {0x069A, 0x57FD, 0x0514},   // I: Cyan
    {0x2C3B, 0x551F, 0x0C3C},   // J: Blue
    {0xFCF8, 0xFE5A, 0xEA8A},   // L: Orange
    {0xFE85, 0xFF8E, 0xFD40},   // O: Yellow
    {0x1570, 0x1E94, 0x030C},   // S: Green
    {0xA2FD, 0xC37E, 0x8AA9},   // T: Purple
    {0xFAAA, 0xFBAC, 0xD186}    // Z: Red
};

} // namespace

void Tetris::onCreate(lv_obj_t *parent) {
    loadHighScore();

    // Allocate pixel buffers in PSRAM
    boardPixels = static_cast<uint16_t *>(heap_caps_calloc(BoardW * BoardH, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    previewPixels = static_cast<uint16_t *>(heap_caps_calloc(PreviewW * PreviewH, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    boardDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    boardDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    boardDsc.header.w = BoardW;
    boardDsc.header.h = BoardH;
    boardDsc.header.stride = BoardW * sizeof(uint16_t);
    boardDsc.data_size = BoardW * BoardH * sizeof(uint16_t);
    boardDsc.data = reinterpret_cast<const uint8_t *>(boardPixels);

    previewDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    previewDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    previewDsc.header.w = PreviewW;
    previewDsc.header.h = PreviewH;
    previewDsc.header.stride = PreviewW * sizeof(uint16_t);
    previewDsc.data_size = PreviewW * PreviewH * sizeof(uint16_t);
    previewDsc.data = reinterpret_cast<const uint8_t *>(previewPixels);

    // Root screen (240x240 dark arcade background)
    screen = c::panel(parent, 240, 240);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0C0D12), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    // Top header bar (x = 0, y = 0, w = 240, h = 34)
    createHud(screen);

    // Center game board container (x = 68, y = 36, w = 104, h = 202)
    auto boardBox = c::panel(screen, BoardW + 4, BoardH + 2);
    lv_obj_set_pos(boardBox, 68, 36);
    lv_obj_set_style_pad_all(boardBox, 1, 0);
    lv_obj_set_style_radius(boardBox, 4, 0);
    lv_obj_set_style_bg_color(boardBox, lv_color_hex(0x0E1118), 0);
    lv_obj_set_style_border_width(boardBox, 1, 0);
    lv_obj_set_style_border_color(boardBox, lv_color_hex(0x384157), 0);

    boardImg = lv_image_create(boardBox);
    lv_image_set_src(boardImg, &boardDsc);
    lv_obj_align(boardImg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_clickable(boardImg, true);

    // Touch gesture handler on center board
    lv_obj_add_event_cb(boardImg, [](lv_event_t *e) {
        auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
        if (self->paused || self->gameOver) return;

        lv_event_code_t code = lv_event_get_code(e);
        lv_indev_t *indev = lv_indev_active();
        if (!indev) return;

        lv_point_t p;
        lv_indev_get_point(indev, &p);

        if (code == LV_EVENT_PRESSED) {
            self->touchStart = p;
            self->lastDrag = p;
            self->touchDownTick = nowMs();
            self->isDragging = false;
        } else if (code == LV_EVENT_PRESSING) {
            int dx = p.x - self->lastDrag.x;
            int dy = p.y - self->lastDrag.y;
            if (std::abs(p.x - self->touchStart.x) > 6 || std::abs(p.y - self->touchStart.y) > 6) {
                self->isDragging = true;
            }

            // Continuous horizontal drag with step accumulation
            while (dx <= -CellSize) {
                self->moveLeft();
                self->lastDrag.x -= CellSize;
                dx += CellSize;
            }
            while (dx >= CellSize) {
                self->moveRight();
                self->lastDrag.x += CellSize;
                dx -= CellSize;
            }

            // Continuous soft drop drag
            while (dy >= CellSize * 2) {
                self->score += 1;
                self->stepDrop();
                self->lastDrag.y += CellSize * 2;
                dy -= CellSize * 2;
            }
        } else if (code == LV_EVENT_RELEASED) {
            uint32_t dt = nowMs() - self->touchDownTick;
            int totalDx = std::abs(p.x - self->touchStart.x);
            int totalDy = p.y - self->touchStart.y;

            if (!self->isDragging && dt < 280 && totalDx < 12 && std::abs(totalDy) < 12) {
                // Short tap -> rotate!
                self->rotatePiece(1);
            } else if (totalDy > CellSize * 3 && dt < 350) {
                // Fast swipe down -> hard drop!
                self->hardDrop();
            }
            self->isDragging = false;
        }
    }, LV_EVENT_ALL, this);

    // Left and right wing arcade controls
    createControls(screen);

    // Pause / Game over dialog overlay
    createMenuOverlay(screen);

    // Timer loop (30 FPS)
    gameTimer = lv_timer_create([](lv_timer_t *t) {
        auto self = static_cast<Tetris *>(lv_timer_get_user_data(t));
        if (!self->running || self->paused || self->gameOver) return;

        uint32_t now = nowMs();

        // Check hold repeat (DAS: Delayed Auto Shift)
        if (self->heldDir != HoldDir::None && now >= self->nextRepeatTick) {
            self->nextRepeatTick = now + 75; // 75ms repeat (~13 shifts/sec)
            if (self->heldDir == HoldDir::Left) {
                self->moveLeft();
            } else if (self->heldDir == HoldDir::Right) {
                self->moveRight();
            } else if (self->heldDir == HoldDir::Down) {
                self->score += 1;
                self->stepDrop();
            }
        }

        // Check if clearing rows flash animation is active
        if (!self->clearingRows.empty()) {
            if (now >= self->clearFlashUntil) {
                // Complete row clearing collapse
                self->clearFullLines();
                self->clearingRows.clear();
                self->spawnPiece();
                self->lastDropTick = now;
            }
            self->renderBoard();
            lv_obj_invalidate(self->boardImg);
            return;
        }

        // Automatic gravity drop
        if (now - self->lastDropTick >= self->dropIntervalMs) {
            self->lastDropTick = now;
            if (!self->stepDrop()) {
                self->lockPiece();
            }
        }

        self->renderBoard();
        lv_obj_invalidate(self->boardImg);
    }, 33, this);

    resetGame();
    showMenu(false);
}

void Tetris::createHud(lv_obj_t *parent) {
    // Top header bar (x = 0, y = 0, w = 240, h = 34)
    auto header = c::panel(parent, 240, 34);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x232938), 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);

    // 1. Next piece preview box (x = 4, y = 2, w = 56, h = 30)
    auto nextBox = c::panel(header, 56, 30);
    lv_obj_set_pos(nextBox, 4, 2);
    lv_obj_set_style_pad_all(nextBox, 1, 0);
    lv_obj_set_style_radius(nextBox, 6, 0);
    lv_obj_set_style_bg_color(nextBox, lv_color_hex(0x1C2230), 0);
    lv_obj_set_style_border_width(nextBox, 1, 0);
    lv_obj_set_style_border_color(nextBox, lv_color_hex(0x2E384D), 0);

    previewImg = lv_image_create(nextBox);
    lv_image_set_src(previewImg, &previewDsc);
    lv_obj_center(previewImg);

    // 2. Score & Lines in center (x = 64, y = 1, w = 112, h = 32)
    auto hudCenter = lv_obj_create(header);
    lv_obj_remove_style_all(hudCenter);
    lv_obj_set_pos(hudCenter, 64, 1);
    lv_obj_set_size(hudCenter, 112, 32);

    scoreVal = c::label(hudCenter, "0", 112);
    lv_obj_set_pos(scoreVal, 0, 0);
    lv_obj_set_style_text_align(scoreVal, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(scoreVal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scoreVal, lv_color_hex(0xFADB14), 0);

    linesVal = c::label(hudCenter, "行数: 0", 112);
    lv_obj_set_pos(linesVal, 0, 16);
    lv_obj_set_style_text_align(linesVal, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(linesVal, t::fontSmall(), 0);
    lv_obj_set_style_text_color(linesVal, lv_color_hex(0x52C41A), 0);

    // 3. Pause button (x = 180, y = 2, w = 56, h = 30)
    auto pauseBtn = lv_button_create(header);
    lv_obj_set_pos(pauseBtn, 180, 2);
    lv_obj_set_size(pauseBtn, 56, 30);
    lv_obj_set_style_radius(pauseBtn, 6, 0);
    lv_obj_set_style_bg_color(pauseBtn, lv_color_hex(0x232733), 0);
    lv_obj_set_style_border_width(pauseBtn, 1, 0);
    lv_obj_set_style_border_color(pauseBtn, lv_color_hex(0x424B60), 0);
    lv_obj_set_style_pad_all(pauseBtn, 0, 0);
    lv_obj_set_ext_click_area(pauseBtn, 4);

    auto pauseLbl = lv_label_create(pauseBtn);
    lv_obj_set_style_text_font(pauseLbl, t::fontSmall(), 0);
    lv_obj_set_style_text_color(pauseLbl, lv_color_hex(0xE6E8ED), 0);
    lv_label_set_text(pauseLbl, LV_SYMBOL_PAUSE " 暂停");
    lv_obj_center(pauseLbl);
    lv_obj_clear_flag(pauseLbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(pauseBtn, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED || lv_event_get_code(e) == LV_EVENT_PRESSED) {
            auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
            self->showMenu(false);
        }
    }, LV_EVENT_ALL, this);
}

void Tetris::createControls(lv_obj_t *parent) {
    auto makeWingBtn = [this](lv_obj_t *parent, const char *symbol, const char *text,
                              int x, int y, int w, int h, lv_color_t color, lv_color_t border,
                              lv_event_cb_t cb) {
        auto btn = lv_button_create(parent);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, w, h);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn, color, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, border, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_ext_click_area(btn, 4);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        auto lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, t::fontSmall(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s\n%s", symbol, text);
        lv_label_set_text(lbl, buf);
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, this);
        return btn;
    };

    // --- Left Wing: Left Thumb Area ---
    // 1. Move Left Button (x = 4, y = 38, w = 60, h = 96)
    makeWingBtn(parent, LV_SYMBOL_LEFT, "左移", 4, 38, 60, 96,
                lv_color_hex(0x1D39C4), lv_color_hex(0x2F54EB),
                [](lv_event_t *e) {
                    auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
                    lv_event_code_t code = lv_event_get_code(e);
                    if (code == LV_EVENT_PRESSED) {
                        if (!self->paused && !self->gameOver) {
                            self->moveLeft();
                            self->heldDir = HoldDir::Left;
                            self->holdStartTick = nowMs();
                            self->nextRepeatTick = self->holdStartTick + 180;
                        }
                    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
                        if (self->heldDir == HoldDir::Left) self->heldDir = HoldDir::None;
                    }
                });

    // 2. Move Right Button (x = 4, y = 140, w = 60, h = 96)
    makeWingBtn(parent, LV_SYMBOL_RIGHT, "右移", 4, 140, 60, 96,
                lv_color_hex(0x1D39C4), lv_color_hex(0x2F54EB),
                [](lv_event_t *e) {
                    auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
                    lv_event_code_t code = lv_event_get_code(e);
                    if (code == LV_EVENT_PRESSED) {
                        if (!self->paused && !self->gameOver) {
                            self->moveRight();
                            self->heldDir = HoldDir::Right;
                            self->holdStartTick = nowMs();
                            self->nextRepeatTick = self->holdStartTick + 180;
                        }
                    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
                        if (self->heldDir == HoldDir::Right) self->heldDir = HoldDir::None;
                    }
                });

    // --- Right Wing: Right Thumb Area ---
    // 3. Rotate Button (x = 176, y = 38, w = 60, h = 96)
    makeWingBtn(parent, LV_SYMBOL_REFRESH, "旋转", 176, 38, 60, 96,
                lv_color_hex(0x722ED1), lv_color_hex(0x9254DE),
                [](lv_event_t *e) {
                    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
                        auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
                        if (!self->paused && !self->gameOver) self->rotatePiece(1);
                    }
                });

    // 4. Drop Button (x = 176, y = 140, w = 60, h = 96)
    // Tap: Soft drop 1 step; Hold: Fast soft drop; Double-Tap: Instant Hard Drop!
    makeWingBtn(parent, LV_SYMBOL_DOWN, "下落", 176, 140, 60, 96,
                lv_color_hex(0x006D75), lv_color_hex(0x13C2C2),
                [](lv_event_t *e) {
                    auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
                    lv_event_code_t code = lv_event_get_code(e);
                    if (code == LV_EVENT_PRESSED) {
                        if (!self->paused && !self->gameOver) {
                            uint32_t now = nowMs();
                            if (now - self->lastDropBtnTick < 280) {
                                // Fast double-tap -> Hard Drop!
                                self->hardDrop();
                                self->lastDropBtnTick = 0;
                                self->heldDir = HoldDir::None;
                            } else {
                                self->lastDropBtnTick = now;
                                self->score += 1;
                                self->stepDrop();
                                self->heldDir = HoldDir::Down;
                                self->holdStartTick = now;
                                self->nextRepeatTick = now + 150;
                            }
                        }
                    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
                        if (self->heldDir == HoldDir::Down) self->heldDir = HoldDir::None;
                    }
                });
}

void Tetris::createMenuOverlay(lv_obj_t *parent) {
    menuOverlay = c::panel(parent, 240, 240);
    lv_obj_set_pos(menuOverlay, 0, 0);
    lv_obj_set_style_bg_color(menuOverlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(menuOverlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(menuOverlay, 0, 0);

    auto card = c::panel(menuOverlay, 200, 180);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1F2430), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x722ED1), 0);

    menuTitle = c::label(card, "俄罗斯方块", 180);
    lv_obj_align(menuTitle, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(menuTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(menuTitle, t::fontSmall(), 0);
    lv_obj_set_style_text_color(menuTitle, lv_color_hex(0xFFFFFF), 0);

    menuScore = c::label(card, "得分: 0  行数: 0", 180);
    lv_obj_align(menuScore, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_text_align(menuScore, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(menuScore, t::fontSmall(), 0);
    lv_obj_set_style_text_color(menuScore, lv_color_hex(0xFADB14), 0);

    menuBest = c::label(card, "历史最高: 0", 180);
    lv_obj_align(menuBest, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_text_align(menuBest, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(menuBest, t::fontSmall(), 0);
    lv_obj_set_style_text_color(menuBest, lv_color_hex(0x52C41A), 0);

    // Resume / Start Button
    menuResumeBtn = c::button(card, "继续", [](lv_event_t *e) {
        auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
        if (self->gameOver) {
            self->resetGame();
        }
        self->hideMenu();
    }, this, 160, 36);
    lv_obj_align(menuResumeBtn, LV_ALIGN_TOP_MID, 0, 84);
    lv_obj_set_style_radius(menuResumeBtn, 10, 0);
    lv_obj_set_style_bg_color(menuResumeBtn, lv_color_hex(0x722ED1), 0);

    // Restart / New Game Button
    auto newGameBtn = c::button(card, "新游戏", [](lv_event_t *e) {
        auto self = static_cast<Tetris *>(lv_event_get_user_data(e));
        self->resetGame();
        self->hideMenu();
    }, this, 76, 32);
    lv_obj_align(newGameBtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_radius(newGameBtn, 8, 0);
    lv_obj_set_style_bg_color(newGameBtn, lv_color_hex(0x2F54EB), 0);

    // Exit Button
    auto exitBtn = c::button(card, "退出", [](lv_event_t *) {
        ui::WindowManager::instance().goHome();
    }, this, 76, 32);
    lv_obj_align(exitBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_radius(exitBtn, 8, 0);
    lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0x434343), 0);
}

void Tetris::showMenu(bool isGameOver) {
    paused = true;
    gameOver = isGameOver;
    heldDir = HoldDir::None;

    char scoreBuf[48];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "得分: %lu   行数: %lu", static_cast<unsigned long>(score), static_cast<unsigned long>(lines));
    c::setText(menuScore, scoreBuf);

    char bestBuf[36];
    std::snprintf(bestBuf, sizeof(bestBuf), "历史最高: %lu", static_cast<unsigned long>(highScore));
    c::setText(menuBest, bestBuf);

    if (isGameOver) {
        c::setText(menuTitle, "游戏结束");
        c::setText(lv_obj_get_child(menuResumeBtn, 0), "再来一局");
        soundEffect(core::game_audio::Lost);
    } else {
        c::setText(menuTitle, running ? "游戏暂停" : "俄罗斯方块");
        c::setText(lv_obj_get_child(menuResumeBtn, 0), running ? "继续游戏" : "开始游戏");
    }

    lv_obj_set_hidden(menuOverlay, false);
}

void Tetris::hideMenu() {
    paused = false;
    running = true;
    heldDir = HoldDir::None;
    lastDropTick = nowMs();
    lv_obj_set_hidden(menuOverlay, true);
}

void Tetris::soundEffect(unsigned effect) {
    if (!audio) audio = core::game_audio::start();
    if (audio) audio->trigger(effect);
}

void Tetris::shuffleBag() {
    bag = {PieceType::I, PieceType::J, PieceType::L, PieceType::O, PieceType::S, PieceType::T, PieceType::Z};
    for (size_t i = bag.size() - 1; i > 0; --i) {
        size_t j = esp_random() % (i + 1);
        std::swap(bag[i], bag[j]);
    }
}

Tetris::PieceType Tetris::nextBagPiece() {
    if (bag.empty()) {
        shuffleBag();
    }
    PieceType p = bag.back();
    bag.pop_back();
    return p;
}

void Tetris::spawnPiece() {
    currentPiece.type = (nextPiece == PieceType::None) ? nextBagPiece() : nextPiece;
    currentPiece.rotation = 0;
    currentPiece.x = 3;
    currentPiece.y = 0;

    nextPiece = nextBagPiece();

    if (!isValidPosition(currentPiece)) {
        // Spawn collision -> Game Over!
        gameOver = true;
        saveHighScore();
        showMenu(true);
    }

    renderPreview();
    lv_obj_invalidate(previewImg);
}

void Tetris::resetGame() {
    for (auto &row : board) {
        row.fill(PieceType::None);
    }
    score = 0;
    lines = 0;
    level = 1;
    clearingRows.clear();
    gameOver = false;
    updateDropSpeed();

    shuffleBag();
    nextPiece = PieceType::None;
    spawnPiece();

    updateHud();
    renderBoard();
    renderPreview();
    lv_obj_invalidate(boardImg);
    lv_obj_invalidate(previewImg);
}

bool Tetris::isValidPosition(const Piece &p) const {
    if (p.type == PieceType::None) return false;
    int typeIdx = static_cast<int>(p.type) - 1;
    int rot = p.rotation % 4;

    for (int i = 0; i < 4; ++i) {
        int x = p.x + kShapes[typeIdx][rot][i].x;
        int y = p.y + kShapes[typeIdx][rot][i].y;

        if (x < 0 || x >= BoardCols || y >= BoardRows) {
            return false;
        }
        if (y >= 0 && board[y][x] != PieceType::None) {
            return false;
        }
    }
    return true;
}

bool Tetris::moveLeft() {
    Piece test = currentPiece;
    test.x -= 1;
    if (isValidPosition(test)) {
        currentPiece = test;
        soundEffect(core::game_audio::Impact);
        renderBoard();
        lv_obj_invalidate(boardImg);
        return true;
    }
    return false;
}

bool Tetris::moveRight() {
    Piece test = currentPiece;
    test.x += 1;
    if (isValidPosition(test)) {
        currentPiece = test;
        soundEffect(core::game_audio::Impact);
        renderBoard();
        lv_obj_invalidate(boardImg);
        return true;
    }
    return false;
}

bool Tetris::rotatePiece(int dir) {
    Piece test = currentPiece;
    test.rotation = (test.rotation + dir + 4) % 4;

    // Standard wall kick offsets: test direct, left 1, right 1, up 1, left 2, right 2
    static constexpr int kKicks[5] = {0, -1, 1, -2, 2};
    for (int kick : kKicks) {
        test.x = currentPiece.x + kick;
        if (isValidPosition(test)) {
            currentPiece = test;
            soundEffect(core::game_audio::Impact);
            renderBoard();
            lv_obj_invalidate(boardImg);
            return true;
        }
    }
    return false;
}

bool Tetris::stepDrop() {
    Piece test = currentPiece;
    test.y += 1;
    if (isValidPosition(test)) {
        currentPiece = test;
        return true;
    }
    return false;
}

void Tetris::hardDrop() {
    int dropCount = 0;
    while (stepDrop()) {
        dropCount++;
    }
    score += dropCount * 2;
    soundEffect(core::game_audio::Fall);
    lockPiece();
    renderBoard();
    lv_obj_invalidate(boardImg);
}

int Tetris::calculateGhostY() const {
    Piece test = currentPiece;
    while (isValidPosition(test)) {
        test.y += 1;
    }
    return test.y - 1;
}

void Tetris::lockPiece() {
    if (currentPiece.type == PieceType::None) return;

    int typeIdx = static_cast<int>(currentPiece.type) - 1;
    int rot = currentPiece.rotation % 4;

    for (int i = 0; i < 4; ++i) {
        int x = currentPiece.x + kShapes[typeIdx][rot][i].x;
        int y = currentPiece.y + kShapes[typeIdx][rot][i].y;
        if (y >= 0 && y < BoardRows && x >= 0 && x < BoardCols) {
            board[y][x] = currentPiece.type;
        }
    }

    currentPiece.type = PieceType::None;

    // Check for full lines
    clearingRows.clear();
    for (int r = 0; r < BoardRows; ++r) {
        bool full = true;
        for (int c = 0; c < BoardCols; ++c) {
            if (board[r][c] == PieceType::None) {
                full = false;
                break;
            }
        }
        if (full) {
            clearingRows.push_back(r);
        }
    }

    if (!clearingRows.empty()) {
        clearFlashUntil = nowMs() + 120; // 120ms white flash animation
        soundEffect(core::game_audio::Bonus);
    } else {
        spawnPiece();
    }

    updateHud();
}

int Tetris::clearFullLines() {
    int count = static_cast<int>(clearingRows.size());
    if (count == 0) return 0;

    for (int fullRow : clearingRows) {
        for (int r = fullRow; r > 0; --r) {
            board[r] = board[r - 1];
        }
        board[0].fill(PieceType::None);
    }

    lines += count;
    level = 1 + lines / 10;
    updateDropSpeed();

    static constexpr uint32_t kLineScores[5] = {0, 100, 300, 500, 800};
    uint32_t earned = (count <= 4 ? kLineScores[count] : 800) * level;
    score += earned;

    saveHighScore();
    updateHud();
    return count;
}

void Tetris::updateDropSpeed() {
    // Drops faster with each level
    if (level <= 1) dropIntervalMs = 800;
    else if (level <= 3) dropIntervalMs = 650;
    else if (level <= 5) dropIntervalMs = 500;
    else if (level <= 8) dropIntervalMs = 380;
    else if (level <= 10) dropIntervalMs = 280;
    else dropIntervalMs = 200;
}

void Tetris::updateHud() {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(score));
    c::setText(scoreVal, buf);

    std::snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(lines));
    c::setText(linesVal, buf);
}

void Tetris::drawBlock(uint16_t *buf, int stride, int bx, int by, PieceType type, bool ghost) {
    if (type == PieceType::None) return;
    const auto &pal = kPalettes[static_cast<size_t>(type)];

    int px = bx * CellSize;
    int py = by * CellSize;

    if (ghost) {
        // Subtle dotted/dashed outline for ghost projection (虚线向导投影)
        uint16_t color = pal.dark;
        for (int y = 0; y < CellSize - 1; ++y) {
            for (int x = 0; x < CellSize - 1; ++x) {
                bool isBorder = (x == 0 || x == CellSize - 2 || y == 0 || y == CellSize - 2);
                if (isBorder && ((x + y) % 2 == 0)) {
                    buf[(py + y) * stride + (px + x)] = color;
                }
            }
        }
        return;
    }

    // 3D beveled block
    for (int y = 0; y < CellSize - 1; ++y) {
        for (int x = 0; x < CellSize - 1; ++x) {
            uint16_t color = pal.base;
            if (y == 0 || x == 0) {
                color = pal.light;
            } else if (y == CellSize - 2 || x == CellSize - 2) {
                color = pal.dark;
            }
            buf[(py + y) * stride + (px + x)] = color;
        }
    }
}

void Tetris::clearBuffer(uint16_t *buf, int w, int h, uint16_t color) {
    std::fill(buf, buf + (w * h), color);
}

void Tetris::renderBoard() {
    if (!boardPixels) return;

    // Fill background
    clearBuffer(boardPixels, BoardW, BoardH, ColorBg);

    // Subtle grid dots/lines
    for (int r = 0; r < BoardRows; ++r) {
        for (int c = 0; c < BoardCols; ++c) {
            boardPixels[(r * CellSize + CellSize - 1) * BoardW + (c * CellSize + CellSize - 1)] = ColorGrid;
        }
    }

    // Draw locked blocks
    for (int r = 0; r < BoardRows; ++r) {
        bool isFlashing = std::find(clearingRows.begin(), clearingRows.end(), r) != clearingRows.end();
        if (isFlashing) {
            // White flash effect for cleared lines
            for (int y = r * CellSize; y < (r + 1) * CellSize - 1; ++y) {
                for (int x = 0; x < BoardW; ++x) {
                    boardPixels[y * BoardW + x] = ColorFlash;
                }
            }
        } else {
            for (int c = 0; c < BoardCols; ++c) {
                if (board[r][c] != PieceType::None) {
                    drawBlock(boardPixels, BoardW, c, r, board[r][c], false);
                }
            }
        }
    }

    // Draw ghost piece (dashed bottom guide)
    if (currentPiece.type != PieceType::None && clearingRows.empty()) {
        int ghostY = calculateGhostY();
        int typeIdx = static_cast<int>(currentPiece.type) - 1;
        int rot = currentPiece.rotation % 4;
        for (int i = 0; i < 4; ++i) {
            int gx = currentPiece.x + kShapes[typeIdx][rot][i].x;
            int gy = ghostY + kShapes[typeIdx][rot][i].y;
            if (gy >= 0 && gy < BoardRows && gx >= 0 && gx < BoardCols) {
                drawBlock(boardPixels, BoardW, gx, gy, currentPiece.type, true);
            }
        }

        // Draw active falling piece
        for (int i = 0; i < 4; ++i) {
            int px = currentPiece.x + kShapes[typeIdx][rot][i].x;
            int py = currentPiece.y + kShapes[typeIdx][rot][i].y;
            if (py >= 0 && py < BoardRows && px >= 0 && px < BoardCols) {
                drawBlock(boardPixels, BoardW, px, py, currentPiece.type, false);
            }
        }
    }
}

void Tetris::renderPreview() {
    if (!previewPixels) return;

    clearBuffer(previewPixels, PreviewW, PreviewH, ColorBg);

    if (nextPiece == PieceType::None) return;

    int typeIdx = static_cast<int>(nextPiece) - 1;
    int minX = 4, maxX = 0, minY = 4, maxY = 0;
    for (int i = 0; i < 4; ++i) {
        int x = kShapes[typeIdx][0][i].x;
        int y = kShapes[typeIdx][0][i].y;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    constexpr int pCell = 6;
    int pw = (maxX - minX + 1) * pCell;
    int ph = (maxY - minY + 1) * pCell;
    int offsetX = (PreviewW - pw) / 2 - minX * pCell;
    int offsetY = (PreviewH - ph) / 2 - minY * pCell;

    const auto &pal = kPalettes[static_cast<size_t>(nextPiece)];
    for (int i = 0; i < 4; ++i) {
        int bx = kShapes[typeIdx][0][i].x * pCell + offsetX;
        int by = kShapes[typeIdx][0][i].y * pCell + offsetY;

        for (int y = 0; y < pCell - 1; ++y) {
            for (int x = 0; x < pCell - 1; ++x) {
                uint16_t color = pal.base;
                if (y == 0 || x == 0) color = pal.light;
                else if (y == pCell - 2 || x == pCell - 2) color = pal.dark;
                if (by + y >= 0 && by + y < PreviewH && bx + x >= 0 && bx + x < PreviewW) {
                    previewPixels[(by + y) * PreviewW + (bx + x)] = color;
                }
            }
        }
    }
}

void Tetris::loadHighScore() {
    highScore = 0;
    nvs_handle_t h;
    if (nvs_open("tetris", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, "high_score", &highScore);
        nvs_close(h);
    }
}

void Tetris::saveHighScore() {
    if (score > highScore) {
        highScore = score;
        nvs_handle_t h;
        if (nvs_open("tetris", NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_u32(h, "high_score", highScore);
            nvs_commit(h);
            nvs_close(h);
        }
    }
}

void Tetris::onShow() {
    if (running && !gameOver) {
        hideMenu();
    }
}

void Tetris::onHide() {
    paused = true;
    saveHighScore();
}

void Tetris::onDestroy() {
    if (gameTimer) {
        lv_timer_delete(gameTimer);
        gameTimer = nullptr;
    }
    if (boardPixels) {
        heap_caps_free(boardPixels);
        boardPixels = nullptr;
    }
    if (previewPixels) {
        heap_caps_free(previewPixels);
        previewPixels = nullptr;
    }
    if (audio) {
        audio->pause();
        audio.reset();
    }
}

bool Tetris::onBack() {
    if (running && !paused && !gameOver) {
        showMenu(false);
        return true;
    }
    return false; // Exit to home
}

} // namespace apps
