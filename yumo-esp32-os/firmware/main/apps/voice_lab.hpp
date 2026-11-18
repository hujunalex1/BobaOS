/* Voice Lab: record a short clip, play it through cartoon effects, and a
 * generated soundboard. Recording and clip playback reuse core::recorder;
 * the soundboard reuses the single game-audio synthesizer worker. */
#pragma once

#include "core/audio_recorder.hpp"
#include "core/game_audio.hpp"
#include "ui/app.hpp"

#include <array>
#include <memory>
#include <string>

namespace apps {

class VoiceLab final : public ui::App {
public:
    const char *name() const override { return "声音泡泡"; }
    const char *icon() const override { return LV_SYMBOL_AUDIO; }
    bool allowsScreenProtection() const override { return false; }
    uint32_t powerHints() const override { return sys::power::StayLit | sys::power::LowLoad; }

    void onCreate(lv_obj_t *parent) override;
    void onShow() override;
    void onHide() override;
    void onDestroy() override;
    bool onBack() override;

private:
    enum class Page : uint8_t { Record, Sounds };
    enum class Mode : uint8_t { Idle, Recording, Playing };
    enum class Pending : uint8_t { None, Record, Play, Sound };

    void buildHeader(void);
    void buildRecordPage(void);
    void buildSoundPage(void);
    void showPage(Page page);
    void startRecording(void);
    void playClip(core::recorder::Effect effect);
    void triggerSound(unsigned index);
    void processPending(void);
    void poll(void);
    void refreshUi(void);
    void setStatus(const char *text);
    void notice(const char *text);
    void stopActivity(void);

    static void tick(lv_timer_t *timer);
    static void bigClick(lv_event_t *event);
    static void effectClick(lv_event_t *event);
    static void soundClick(lv_event_t *event);
    static void pageClick(lv_event_t *event);
    static void backClick(lv_event_t *event);
    static void rerecordClick(lv_event_t *event);

    lv_obj_t *root = nullptr;
    lv_obj_t *header = nullptr;
    lv_obj_t *back = nullptr;
    lv_obj_t *title = nullptr;
    lv_obj_t *pageButton = nullptr;
    lv_obj_t *recordPage = nullptr;
    lv_obj_t *soundPage = nullptr;
    lv_obj_t *big = nullptr;
    lv_obj_t *bigIcon = nullptr;
    lv_obj_t *bigLabel = nullptr;
    lv_obj_t *status = nullptr;
    lv_obj_t *meter = nullptr;
    lv_obj_t *rerecord = nullptr;
    lv_obj_t *soundTab = nullptr;
    std::array<lv_obj_t *, 5> effectButtons{};
    std::array<lv_obj_t *, 8> soundButtons{};
    lv_timer_t *timer = nullptr;

    Page page = Page::Record;
    Mode mode = Mode::Idle;
    Pending pending = Pending::None;
    core::recorder::Effect pendingEffect = core::recorder::Effect::None;
    unsigned pendingSound = 0;
    std::string clipPath;
    std::string noticeText;
    uint32_t noticeUntil = 0;
    uint32_t recordStarted = 0;
    uint32_t uiSignature = 0xFFFFFFFFu;
    bool visible = false;
    std::shared_ptr<core::recorder::Job> job;
    std::shared_ptr<core::game_audio::Session> soundSession;
};

} // namespace apps
