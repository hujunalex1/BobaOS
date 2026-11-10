#include "cubepaw_os.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace cubepaw {

enum class GameMode {
    MENU = 0,
    STAR_CATCHER,   // 1. 小玉接星星
    PUZZLE_2048,    // 2. 2048 益智数字块
    BREAKOUT,       // 3. 经典打砖块
    IMU_MAZE_DICE   // 4. 体感/触控滚球迷宫 & 摇骰子
};

struct GamesState {
    lv_obj_t* root = nullptr;
    lv_obj_t* stage = nullptr;
    lv_timer_t* loop_timer = nullptr;
    GameMode mode = GameMode::MENU;

    // 游戏 1: 小玉接星星
    int player_x = 95;
    int star_x = 120;
    int star_y = 38;
    bool is_bomb = false;
    int score = 0;
    lv_obj_t* player_obj = nullptr;
    lv_obj_t* star_obj = nullptr;
    lv_obj_t* score_lbl = nullptr;

    // 游戏 2: 2048
    int board_2048[4][4];
    lv_obj_t* cell_lbls[4][4];

    // 游戏 3: 打砖块
    float ball_x = 120.0f, ball_y = 160.0f;
    float ball_vx = 2.8f, ball_vy = -3.2f;
    int paddle_x = 88;
    bool bricks[3][6];
    lv_obj_t* brick_objs[3][6];
    lv_obj_t* ball_obj = nullptr;
    lv_obj_t* paddle_obj = nullptr;

    // 游戏 4: 滚球迷宫 & 摇骰子
    float maze_bx = 26.0f, maze_by = 46.0f;
    int dice_val = 6;
    lv_obj_t* maze_ball_obj = nullptr;
    lv_obj_t* dice_lbl = nullptr;
};

static GamesState g_gm;

static void StopGameTimer() {
    if (g_gm.loop_timer) {
        lv_timer_del(g_gm.loop_timer);
        g_gm.loop_timer = nullptr;
    }
}

static lv_obj_t* ResetGameStage() {
    StopGameTimer();
    if (g_gm.stage != nullptr) {
        lv_obj_t* old = g_gm.stage;
        g_gm.stage = nullptr;
        lv_obj_add_flag(old, LV_OBJ_FLAG_HIDDEN);
        lv_obj_delete_async(old);
    }
    g_gm.stage = lv_obj_create(g_gm.root);
    lv_obj_set_size(g_gm.stage, 240, 240);
    lv_obj_set_pos(g_gm.stage, 0, 0);
    lv_obj_set_style_pad_all(g_gm.stage, 0, 0);
    lv_obj_set_style_border_width(g_gm.stage, 0, 0);
    lv_obj_set_style_radius(g_gm.stage, 0, 0);
    lv_obj_set_style_bg_color(g_gm.stage, lv_color_hex(0xFFF6B3), 0);
    lv_obj_clear_flag(g_gm.stage, LV_OBJ_FLAG_SCROLLABLE);
    return g_gm.stage;
}

static void RenderGamesMenu();

static void CreateGameTopBar(lv_obj_t* parent, const char* title) {
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 64, 28);
    lv_obj_set_pos(back_btn, 6, 4);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_radius(back_btn, 14, 0);
    lv_obj_set_ext_click_area(back_btn, 10);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        RenderGamesMenu();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(back_btn);
    lv_label_set_text(l, "< 菜单");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l);

    g_gm.score_lbl = lv_label_create(parent);
    lv_label_set_text(g_gm.score_lbl, title);
    lv_obj_set_style_text_color(g_gm.score_lbl, lv_color_hex(0x262D5C), 0);
    lv_obj_align(g_gm.score_lbl, LV_ALIGN_TOP_RIGHT, -10, 8);
}

// ============================================================================
// 游戏 1：小玉接星星 (Star Catcher)
// ============================================================================
static void StarCatcherLoopCb(lv_timer_t*) {
    g_gm.star_y += 5;
    if (g_gm.star_y >= 190 && g_gm.star_y <= 218) {
        if (std::abs(g_gm.star_x - (g_gm.player_x + 28)) < 34) {
            if (g_gm.is_bomb) {
                g_gm.score = (g_gm.score >= 15) ? (g_gm.score - 15) : 0;
            } else {
                g_gm.score += 10;
            }
            g_gm.star_y = 36;
            g_gm.star_x = 20 + (std::rand() % 190);
            g_gm.is_bomb = (std::rand() % 5 == 0);
        }
    }
    if (g_gm.star_y > 232) {
        g_gm.star_y = 36;
        g_gm.star_x = 20 + (std::rand() % 190);
        g_gm.is_bomb = (std::rand() % 5 == 0);
    }

    lv_obj_set_pos(g_gm.star_obj, g_gm.star_x - 12, g_gm.star_y);
    lv_obj_set_style_bg_color(g_gm.star_obj,
        lv_color_hex(g_gm.is_bomb ? 0x424242 : 0xFFD600), 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "得分: %d", g_gm.score);
    lv_label_set_text(g_gm.score_lbl, buf);
}

static void StartStarCatcher() {
    lv_obj_t* st = ResetGameStage();
    g_gm.mode = GameMode::STAR_CATCHER;
    g_gm.score = 0;
    g_gm.player_x = 90;
    g_gm.star_x = 120;
    g_gm.star_y = 38;
    g_gm.is_bomb = false;

    CreateGameTopBar(st, "得分: 0");

    g_gm.star_obj = lv_obj_create(st);
    lv_obj_set_size(g_gm.star_obj, 24, 24);
    lv_obj_set_style_radius(g_gm.star_obj, 12, 0);
    lv_obj_set_style_bg_color(g_gm.star_obj, lv_color_hex(0xFFD600), 0);
    lv_obj_clear_flag(g_gm.star_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_gm.star_obj, LV_OBJ_FLAG_CLICKABLE);

    g_gm.player_obj = lv_obj_create(st);
    lv_obj_set_size(g_gm.player_obj, 56, 22);
    lv_obj_set_pos(g_gm.player_obj, g_gm.player_x, 206);
    lv_obj_set_style_radius(g_gm.player_obj, 10, 0);
    lv_obj_set_style_bg_color(g_gm.player_obj, lv_color_hex(0xFF8A80), 0);
    lv_obj_clear_flag(g_gm.player_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_gm.player_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* pl = lv_label_create(g_gm.player_obj);
    lv_label_set_text(pl, "小玉");
    lv_obj_center(pl);

    // 触屏点击或拖动均可控制小玉位置
    lv_obj_add_event_cb(st, [](lv_event_t*) {
        if (g_gm.mode != GameMode::STAR_CATCHER) return;
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (p.y > 34) {
            int nx = p.x - 28;
            if (nx < 4) nx = 4;
            if (nx > 180) nx = 180;
            g_gm.player_x = nx;
            lv_obj_set_pos(g_gm.player_obj, g_gm.player_x, 206);
        }
    }, LV_EVENT_PRESSING, nullptr);

    g_gm.loop_timer = lv_timer_create(StarCatcherLoopCb, 35, nullptr);
}

// ============================================================================
// 游戏 2：2048 益智数字块
// ============================================================================
static void Spawn2048Tile() {
    int empty_r[16], empty_c[16], count = 0;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (g_gm.board_2048[r][c] == 0) {
                empty_r[count] = r;
                empty_c[count] = c;
                count++;
            }
        }
    }
    if (count > 0) {
        int pick = std::rand() % count;
        g_gm.board_2048[empty_r[pick]][empty_c[pick]] = (std::rand() % 5 == 0) ? 4 : 2;
    }
}

static void Refresh2048UI() {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int v = g_gm.board_2048[r][c];
            char buf[12];
            if (v == 0) {
                lv_label_set_text(g_gm.cell_lbls[r][c], "");
            } else {
                snprintf(buf, sizeof(buf), "%d", v);
                lv_label_set_text(g_gm.cell_lbls[r][c], buf);
            }
        }
    }
}

static void Slide2048Line(int line[4], bool& moved) {
    int tmp[4] = {0, 0, 0, 0};
    int pos = 0;
    for (int i = 0; i < 4; ++i) {
        if (line[i] != 0) tmp[pos++] = line[i];
    }
    for (int i = 0; i < 3; ++i) {
        if (tmp[i] != 0 && tmp[i] == tmp[i + 1]) {
            tmp[i] *= 2;
            g_gm.score += tmp[i];
            for (int j = i + 1; j < 3; ++j) tmp[j] = tmp[j + 1];
            tmp[3] = 0;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (line[i] != tmp[i]) moved = true;
        line[i] = tmp[i];
    }
}

static void Move2048(lv_dir_t dir) {
    bool moved = false;
    int line[4];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (dir == LV_DIR_LEFT) line[j] = g_gm.board_2048[i][j];
            else if (dir == LV_DIR_RIGHT) line[j] = g_gm.board_2048[i][3 - j];
            else if (dir == LV_DIR_TOP) line[j] = g_gm.board_2048[j][i];
            else if (dir == LV_DIR_BOTTOM) line[j] = g_gm.board_2048[3 - j][i];
        }
        Slide2048Line(line, moved);
        for (int j = 0; j < 4; ++j) {
            if (dir == LV_DIR_LEFT) g_gm.board_2048[i][j] = line[j];
            else if (dir == LV_DIR_RIGHT) g_gm.board_2048[i][3 - j] = line[j];
            else if (dir == LV_DIR_TOP) g_gm.board_2048[j][i] = line[j];
            else if (dir == LV_DIR_BOTTOM) g_gm.board_2048[3 - j][i] = line[j];
        }
    }
    if (moved) {
        Spawn2048Tile();
        Refresh2048UI();
        char buf[32];
        snprintf(buf, sizeof(buf), "2048: %d", g_gm.score);
        lv_label_set_text(g_gm.score_lbl, buf);
    }
}

static void Start2048Game() {
    lv_obj_t* st = ResetGameStage();
    g_gm.mode = GameMode::PUZZLE_2048;
    g_gm.score = 0;
    std::memset(g_gm.board_2048, 0, sizeof(g_gm.board_2048));

    CreateGameTopBar(st, "2048: 0");

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            lv_obj_t* cell = lv_obj_create(st);
            lv_obj_set_size(cell, 46, 36);
            lv_obj_set_pos(cell, 16 + c * 52, 34 + r * 40);
            lv_obj_set_style_bg_color(cell, lv_color_hex(0xFFEA79), 0);
            lv_obj_set_style_border_color(cell, lv_color_hex(0x262D5C), 0);
            lv_obj_set_style_border_width(cell, 2, 0);
            lv_obj_set_style_radius(cell, 8, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_CLICKABLE);

            g_gm.cell_lbls[r][c] = lv_label_create(cell);
            lv_obj_set_style_text_color(g_gm.cell_lbls[r][c], lv_color_hex(0x262D5C), 0);
            lv_obj_center(g_gm.cell_lbls[r][c]);
        }
    }

    // 底部四个方向按键（支持滑动 + 按键双操作）
    struct DirBtn { const char* txt; lv_dir_t dir; };
    static const DirBtn kDirs[4] = {
        {"左", LV_DIR_LEFT}, {"上", LV_DIR_TOP}, {"下", LV_DIR_BOTTOM}, {"右", LV_DIR_RIGHT}
    };
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* db = lv_btn_create(st);
        lv_obj_set_size(db, 48, 34);
        lv_obj_set_pos(db, 14 + i * 54, 198);
        lv_obj_set_style_bg_color(db, lv_color_hex(0x262D5C), 0);
        lv_obj_set_style_radius(db, 10, 0);
        lv_obj_set_ext_click_area(db, 6);
        lv_obj_add_event_cb(db, [](lv_event_t* e) {
            lv_dir_t d = static_cast<lv_dir_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
            Move2048(d);
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(kDirs[i].dir)));
        lv_obj_t* dl = lv_label_create(db);
        lv_label_set_text(dl, kDirs[i].txt);
        lv_obj_set_style_text_color(dl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(dl);
    }

    Spawn2048Tile();
    Spawn2048Tile();
    Refresh2048UI();

    lv_obj_add_event_cb(st, [](lv_event_t*) {
        if (g_gm.mode != GameMode::PUZZLE_2048) return;
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir != LV_DIR_NONE) Move2048(dir);
    }, LV_EVENT_GESTURE, nullptr);
}

// ============================================================================
// 游戏 3：经典打砖块 (Breakout)
// ============================================================================
static void BreakoutLoopCb(lv_timer_t*) {
    g_gm.ball_x += g_gm.ball_vx;
    g_gm.ball_y += g_gm.ball_vy;

    if (g_gm.ball_x < 6 || g_gm.ball_x > 224) g_gm.ball_vx = -g_gm.ball_vx;
    if (g_gm.ball_y < 34) g_gm.ball_vy = -g_gm.ball_vy;

    // 挡板碰撞
    if (g_gm.ball_y >= 200 && g_gm.ball_y <= 218 &&
        g_gm.ball_x >= g_gm.paddle_x - 8 && g_gm.ball_x <= g_gm.paddle_x + 64) {
        g_gm.ball_vy = -std::fabs(g_gm.ball_vy);
    }

    // 砖块碰撞
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 6; ++c) {
            if (!g_gm.bricks[r][c]) continue;
            int bx = 12 + c * 36;
            int by = 40 + r * 20;
            if (g_gm.ball_x >= bx && g_gm.ball_x <= bx + 32 &&
                g_gm.ball_y >= by && g_gm.ball_y <= by + 14) {
                g_gm.bricks[r][c] = false;
                lv_obj_add_flag(g_gm.brick_objs[r][c], LV_OBJ_FLAG_HIDDEN);
                g_gm.ball_vy = -g_gm.ball_vy;
                g_gm.score += 10;
                char buf[32];
                snprintf(buf, sizeof(buf), "砖块: %d", g_gm.score);
                lv_label_set_text(g_gm.score_lbl, buf);
            }
        }
    }

    if (g_gm.ball_y > 234) {
        g_gm.ball_x = 120.0f;
        g_gm.ball_y = 160.0f;
        g_gm.ball_vy = -3.2f;
    }

    lv_obj_set_pos(g_gm.ball_obj, static_cast<int>(g_gm.ball_x), static_cast<int>(g_gm.ball_y));
}

static void StartBreakoutGame() {
    lv_obj_t* st = ResetGameStage();
    g_gm.mode = GameMode::BREAKOUT;
    g_gm.score = 0;
    g_gm.ball_x = 120.0f;
    g_gm.ball_y = 160.0f;
    g_gm.ball_vx = 2.8f;
    g_gm.ball_vy = -3.2f;
    g_gm.paddle_x = 90;

    CreateGameTopBar(st, "砖块: 0");

    static const uint32_t kRowColors[3] = {0xFF8A80, 0xFFD180, 0x80D8FF};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 6; ++c) {
            g_gm.bricks[r][c] = true;
            lv_obj_t* b = lv_obj_create(st);
            lv_obj_set_size(b, 32, 14);
            lv_obj_set_pos(b, 12 + c * 36, 40 + r * 20);
            lv_obj_set_style_bg_color(b, lv_color_hex(kRowColors[r]), 0);
            lv_obj_set_style_radius(b, 4, 0);
            lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
            g_gm.brick_objs[r][c] = b;
        }
    }

    g_gm.ball_obj = lv_obj_create(st);
    lv_obj_set_size(g_gm.ball_obj, 12, 12);
    lv_obj_set_style_radius(g_gm.ball_obj, 6, 0);
    lv_obj_set_style_bg_color(g_gm.ball_obj, lv_color_hex(0x262D5C), 0);
    lv_obj_clear_flag(g_gm.ball_obj, LV_OBJ_FLAG_CLICKABLE);

    g_gm.paddle_obj = lv_obj_create(st);
    lv_obj_set_size(g_gm.paddle_obj, 58, 12);
    lv_obj_set_pos(g_gm.paddle_obj, g_gm.paddle_x, 212);
    lv_obj_set_style_radius(g_gm.paddle_obj, 6, 0);
    lv_obj_set_style_bg_color(g_gm.paddle_obj, lv_color_hex(0x262D5C), 0);
    lv_obj_clear_flag(g_gm.paddle_obj, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(st, [](lv_event_t*) {
        if (g_gm.mode != GameMode::BREAKOUT) return;
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (p.y > 34) {
            int nx = p.x - 29;
            if (nx < 4) nx = 4;
            if (nx > 178) nx = 178;
            g_gm.paddle_x = nx;
            lv_obj_set_pos(g_gm.paddle_obj, g_gm.paddle_x, 212);
        }
    }, LV_EVENT_PRESSING, nullptr);

    g_gm.loop_timer = lv_timer_create(BreakoutLoopCb, 30, nullptr);
}

// ============================================================================
// 游戏 4：滚球迷宫 & 摇骰子
// ============================================================================
static void ImuMazeLoopCb(lv_timer_t*) {
    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    auto& cb = CubePawOS::GetInstance().Callbacks();
    if (cb.on_read_imu_accel) {
        cb.on_read_imu_accel(ax, ay, az);
    }

    float total_g = std::fabs(ax) + std::fabs(ay) + std::fabs(az);
    if (total_g > 2.2f && g_gm.dice_lbl) {
        g_gm.dice_val = 1 + (std::rand() % 6);
        char dbuf[64];
        snprintf(dbuf, sizeof(dbuf), "摇骰子: [%d] 点", g_gm.dice_val);
        lv_label_set_text(g_gm.dice_lbl, dbuf);
    }

    g_gm.maze_bx += ax * 4.5f;
    g_gm.maze_by += ay * 4.5f;
    if (g_gm.maze_bx < 16) g_gm.maze_bx = 16;
    if (g_gm.maze_bx > 208) g_gm.maze_bx = 208;
    if (g_gm.maze_by < 40) g_gm.maze_by = 40;
    if (g_gm.maze_by > 176) g_gm.maze_by = 176;

    lv_obj_set_pos(g_gm.maze_ball_obj,
                   static_cast<int>(g_gm.maze_bx),
                   static_cast<int>(g_gm.maze_by));
}

static void StartImuMazeGame() {
    lv_obj_t* st = ResetGameStage();
    g_gm.mode = GameMode::IMU_MAZE_DICE;
    g_gm.maze_bx = 28.0f;
    g_gm.maze_by = 50.0f;

    CreateGameTopBar(st, "触屏滚球/摇骰子");

    // 终点星标
    lv_obj_t* goal = lv_obj_create(st);
    lv_obj_set_size(goal, 28, 28);
    lv_obj_set_pos(goal, 190, 150);
    lv_obj_set_style_bg_color(goal, lv_color_hex(0x69F0AE), 0);
    lv_obj_set_style_radius(goal, 14, 0);
    lv_obj_clear_flag(goal, LV_OBJ_FLAG_CLICKABLE);

    // 滚球
    g_gm.maze_ball_obj = lv_obj_create(st);
    lv_obj_set_size(g_gm.maze_ball_obj, 20, 20);
    lv_obj_set_style_bg_color(g_gm.maze_ball_obj, lv_color_hex(0xFF5252), 0);
    lv_obj_set_style_radius(g_gm.maze_ball_obj, 10, 0);
    lv_obj_clear_flag(g_gm.maze_ball_obj, LV_OBJ_FLAG_CLICKABLE);

    // 触屏引导滚球移动
    lv_obj_add_event_cb(st, [](lv_event_t*) {
        if (g_gm.mode != GameMode::IMU_MAZE_DICE) return;
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (p.y > 36 && p.y < 190) {
            g_gm.maze_bx += (p.x - 10 - g_gm.maze_bx) * 0.25f;
            g_gm.maze_by += (p.y - 10 - g_gm.maze_by) * 0.25f;
        }
    }, LV_EVENT_PRESSING, nullptr);

    // 底部摇骰子按钮
    lv_obj_t* dice_btn = lv_btn_create(st);
    lv_obj_set_size(dice_btn, 196, 36);
    lv_obj_align(dice_btn, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_color(dice_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_ext_click_area(dice_btn, 10);
    lv_obj_add_event_cb(dice_btn, [](lv_event_t*) {
        g_gm.dice_val = 1 + (std::rand() % 6);
        char dbuf[64];
        snprintf(dbuf, sizeof(dbuf), "点此掷骰子: [%d] 点", g_gm.dice_val);
        lv_label_set_text(g_gm.dice_lbl, dbuf);
    }, LV_EVENT_CLICKED, nullptr);
    g_gm.dice_lbl = lv_label_create(dice_btn);
    lv_label_set_text(g_gm.dice_lbl, "点此掷骰子: [6] 点");
    lv_obj_set_style_text_color(g_gm.dice_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(g_gm.dice_lbl);

    g_gm.loop_timer = lv_timer_create(ImuMazeLoopCb, 40, nullptr);
}

static void RenderGamesMenu() {
    lv_obj_t* st = ResetGameStage();
    g_gm.mode = GameMode::MENU;

    lv_obj_t* back_btn = lv_btn_create(st);
    lv_obj_set_size(back_btn, 68, 30);
    lv_obj_set_pos(back_btn, 8, 6);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x262D5C), 0);
    lv_obj_set_style_radius(back_btn, 15, 0);
    lv_obj_set_ext_click_area(back_btn, 10);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        StopGameTimer();
        CubePawOS::GetInstance().OpenApp(AppId::LAUNCHER);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< 桌面");
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(st);
    lv_label_set_text(title, "4合1 掌上游戏厅");
    lv_obj_set_style_text_color(title, lv_color_hex(0x262D5C), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 24, 12);

    struct GameCard {
        const char* label;
        uint32_t color;
        void (*launcher)();
    };
    static const GameCard kGameCards[4] = {
        {"1. 小玉接星星 (触屏滑动)", 0xFFCC80, StartStarCatcher},
        {"2. 2048 益智数字块",      0x80DEEA, Start2048Game},
        {"3. 经典彩色打砖块",       0xF48FB1, StartBreakoutGame},
        {"4. 触屏迷宫 & 欢乐骰子",  0xA5D6A7, StartImuMazeGame},
    };

    for (int i = 0; i < 4; ++i) {
        lv_obj_t* btn = lv_btn_create(st);
        lv_obj_set_size(btn, 212, 40);
        lv_obj_set_pos(btn, 14, 42 + i * 47);
        lv_obj_set_style_bg_color(btn, lv_color_hex(kGameCards[i].color), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x262D5C), 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_ext_click_area(btn, 4);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto fn = reinterpret_cast<void (*)()>(lv_event_get_user_data(e));
            if (fn) fn();
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(kGameCards[i].launcher));

        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text(l, kGameCards[i].label);
        lv_obj_set_style_text_color(l, lv_color_hex(0x262D5C), 0);
        lv_obj_center(l);
    }
}

void BuildGamesApp(lv_obj_t* parent) {
    g_gm.root = parent;
    g_gm.stage = nullptr;
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFF6B3), 0);
    lv_obj_add_event_cb(parent, [](lv_event_t*) {
        StopGameTimer();
        g_gm.stage = nullptr;
        g_gm.root = nullptr;
    }, LV_EVENT_DELETE, nullptr);
    RenderGamesMenu();
}

} // namespace cubepaw
