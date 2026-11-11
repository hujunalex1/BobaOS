#pragma once
#include "app.hpp"
#include "ui/app_icons.hpp"
#include "core/game_audio.hpp"
#include "esp_timer.h"
#include <memory>
#include <array>
#include <vector>

namespace apps {

class Tetris : public ui::App {
public:
    const char *name() const override { return "俄罗斯方块"; }
    const char *icon() const override { return ui::icons::Patterns; }
    bool handlesHorizontalGesture() const override { return !paused; }
    bool allowsQuickPanel() const override { return paused; }
    uint32_t powerHints() const override {
        return running && !paused ? (sys::power::StayLit | sys::power::KeepSession) : sys::power::None;
    }

    void onCreate(lv_obj_t *parent) override;
    void onShow() override;
    void onHide() override;
    void onDestroy() override;
    bool onBack() override;

private:
    static constexpr int BoardCols = 10;
    static constexpr int BoardRows = 20;
    static constexpr int CellSize = 10;
    static constexpr int BoardW = BoardCols * CellSize; // 100 px
    static constexpr int BoardH = BoardRows * CellSize; // 200 px
    static constexpr int PreviewW = 32;
    static constexpr int PreviewH = 32;

    enum class PieceType : uint8_t { None = 0, I = 1, J = 2, L = 3, O = 4, S = 5, T = 6, Z = 7 };

    struct Piece {
        PieceType type{PieceType::None};
        int rotation{0}; // 0, 1, 2, 3
        int x{3};
        int y{0};
    };

    void initGame();
    void resetGame();
    void spawnPiece();
    PieceType nextBagPiece();
    void shuffleBag();

    bool isValidPosition(const Piece &p) const;
    bool moveLeft();
    bool moveRight();
    bool rotatePiece(int dir = 1);
    bool stepDrop();
    void hardDrop();
    void lockPiece();
    int clearFullLines();
    int calculateGhostY() const;

    void updateDropSpeed();
    void soundEffect(unsigned effect);

    // Rendering
    void renderBoard();
    void renderPreview();
    void updateHud();

    // Pixel drawing helpers
    void drawBlock(uint16_t *buf, int stride, int bx, int by, PieceType type, bool ghost = false);
    void clearBuffer(uint16_t *buf, int w, int h, uint16_t color);

    // UI creation
    void createHud(lv_obj_t *parent);
    void createControls(lv_obj_t *parent);
    void createMenuOverlay(lv_obj_t *parent);
    void showMenu(bool isGameOver);
    void hideMenu();

    // High score
    void loadHighScore();
    void saveHighScore();

    // UI references
    lv_obj_t *screen{nullptr};
    lv_obj_t *boardImg{nullptr};
    lv_obj_t *previewImg{nullptr};
    lv_obj_t *scoreVal{nullptr};
    lv_obj_t *linesVal{nullptr};
    lv_obj_t *levelVal{nullptr};
    lv_obj_t *menuOverlay{nullptr};
    lv_obj_t *menuTitle{nullptr};
    lv_obj_t *menuScore{nullptr};
    lv_obj_t *menuBest{nullptr};
    lv_obj_t *menuResumeBtn{nullptr};
    lv_timer_t *gameTimer{nullptr};

    // Board image descriptors & framebuffers
    lv_image_dsc_t boardDsc{};
    lv_image_dsc_t previewDsc{};
    uint16_t *boardPixels{nullptr};
    uint16_t *previewPixels{nullptr};

    // Game state
    std::array<std::array<PieceType, BoardCols>, BoardRows> board{};
    Piece currentPiece{};
    PieceType nextPiece{PieceType::None};
    std::vector<PieceType> bag{};

    uint32_t score{0};
    uint32_t lines{0};
    uint32_t level{1};
    uint32_t highScore{0};
    uint32_t dropIntervalMs{800};
    uint32_t lastDropTick{0};

    // Line clear flash animation state
    std::vector<int> clearingRows{};
    uint32_t clearFlashUntil{0};

    bool running{false};
    bool paused{true};
    bool gameOver{false};

    // Touch gesture tracking on board
    lv_point_t touchStart{};
    lv_point_t lastDrag{};
    uint32_t touchDownTick{0};
    bool isDragging{false};

    // Auto-repeat (DAS: Delayed Auto Shift) for hold keys
    enum class HoldDir : uint8_t { None = 0, Left, Right, Down };
    HoldDir heldDir{HoldDir::None};
    uint32_t holdStartTick{0};
    uint32_t nextRepeatTick{0};
    uint32_t lastDropBtnTick{0};

    std::shared_ptr<core::game_audio::Session> audio;
};

} // namespace apps
