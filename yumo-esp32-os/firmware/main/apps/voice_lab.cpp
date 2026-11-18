#include "voice_lab.hpp"

#include "core/power_mgr.hpp"
#include "ui/compact_ui.hpp"
#include "ui/theme.hpp"
#include "ui/window_mgr.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace apps {
namespace {

namespace c = ui::compact;
namespace t = ui::theme;

constexpr unsigned kMaxSeconds = 10;
constexpr core::recorder::Effect kEffects[5] = {
    core::recorder::Effect::Chipmunk,
    core::recorder::Effect::Monster,
    core::recorder::Effect::Robot,
    core::recorder::Effect::Echo,
    core::recorder::Effect::Slow,
};
// Index 0 is the untouched clip; the five effect buttons use 1..5.
constexpr const char *kEffectNames[6] = {"原声", "尖声", "低沉", "机械", "回声", "慢放"};
constexpr unsigned kSoundBits[8] = {
    core::game_audio::Applause,
    core::game_audio::Laugh,
    core::game_audio::Meow,
    core::game_audio::Magic,
    core::game_audio::Boing,
    core::game_audio::Honk,
    core::game_audio::Twinkle,
    core::game_audio::Drum,
};
constexpr const char *kSoundNames[8] = {"掌声", "笑声", "喵", "魔法", "弹簧", "喇叭", "星星", "鼓"};
constexpr uint32_t kSoundColors[8] = {
    0xFFD6E0, 0xD8F3E6, 0xFFF3C4, 0xE6E0FF,
    0xFFE0C2, 0xD6ECFF, 0xFFE6F5, 0xE2F0D9,
};

void setButtonLabelFont(lv_obj_t *button, const lv_font_t *font) {
    auto label = lv_obj_get_child(button, 0);
    if (label) lv_obj_set_style_text_font(label, font, 0);
}

} // namespace

void VoiceLab::onCreate(lv_obj_t *parent) {
    root = parent;
    lv_obj_set_scrollable(parent, false);
    buildHeader();
    buildRecordPage();
    buildSoundPage();
    timer = lv_timer_create(tick, 50, this);
    showPage(Page::Record);
    refreshUi();
}

void VoiceLab::buildHeader(void) {
    header = c::panel(root, 240, 36);
    lv_obj_set_pos(header, 0, 0);
    back = c::button(header, LV_SYMBOL_LEFT, backClick, this, 36, 36);
    lv_obj_set_pos(back, 0, 0);
    setButtonLabelFont(back, &lv_font_montserrat_24);
    title = c::label(header, "声音泡泡", 120);
    lv_obj_set_pos(title, 40, 8);
    lv_obj_set_style_text_font(title, t::fontSmall(), 0);
    pageButton = c::button(header, "音效板", pageClick, this, 70, 36);
    lv_obj_set_pos(pageButton, 170, 0);
}

void VoiceLab::buildRecordPage(void) {
    recordPage = c::panel(root, 240, 204);
    lv_obj_set_pos(recordPage, 0, 36);
    lv_obj_set_style_bg_color(recordPage, t::bg(), 0);

    big = lv_button_create(recordPage);
    lv_obj_set_size(big, 80, 80);
    lv_obj_set_pos(big, 80, 2);
    lv_obj_set_style_radius(big, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(big, 0, 0);
    lv_obj_set_style_shadow_width(big, 0, 0);
    lv_obj_set_style_bg_opa(big, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(big, bigClick, LV_EVENT_CLICKED, this);

    bigIcon = lv_label_create(big);
    lv_label_set_text(bigIcon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(bigIcon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(bigIcon, lv_color_white(), 0);
    lv_obj_align(bigIcon, LV_ALIGN_TOP_MID, 0, 10);

    bigLabel = lv_label_create(big);
    lv_label_set_text(bigLabel, "录音");
    lv_obj_set_style_text_font(bigLabel, t::fontSmall(), 0);
    lv_obj_set_style_text_color(bigLabel, lv_color_white(), 0);
    lv_obj_align(bigLabel, LV_ALIGN_BOTTOM_MID, 0, -10);

    status = c::label(recordPage, "", 230);
    lv_obj_set_pos(status, 5, 88);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);

    meter = lv_bar_create(recordPage);
    lv_obj_set_size(meter, 210, 8);
    lv_obj_set_pos(meter, 15, 110);
    lv_bar_set_range(meter, 0, 1000);
    lv_bar_set_value(meter, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(meter, t::surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(meter, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(meter, t::accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(meter, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(meter, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(meter, 4, LV_PART_INDICATOR);

    for (unsigned i = 0; i < 5; ++i) {
        auto button = c::button(recordPage, kEffectNames[i + 1], effectClick,
                                reinterpret_cast<void *>(static_cast<uintptr_t>(i + 1)), 44, 34);
        lv_obj_set_user_data(button, this);
        lv_obj_set_pos(button, 6 + int(i) * 46, 124);
        setButtonLabelFont(button, t::fontSmall());
        effectButtons[i] = button;
    }

    rerecord = c::button(recordPage, "重录", rerecordClick, this, 112, 36);
    lv_obj_set_pos(rerecord, 6, 164);
    soundTab = c::button(recordPage, "音效板", pageClick, this, 112, 36);
    lv_obj_set_pos(soundTab, 122, 164);
}

void VoiceLab::buildSoundPage(void) {
    soundPage = c::panel(root, 240, 204);
    lv_obj_set_pos(soundPage, 0, 36);
    lv_obj_set_style_bg_color(soundPage, t::bg(), 0);
    lv_obj_set_hidden(soundPage, true);
    for (unsigned i = 0; i < 8; ++i) {
        auto button = c::button(soundPage, kSoundNames[i], soundClick,
                                reinterpret_cast<void *>(static_cast<uintptr_t>(i)), 56, 64);
        lv_obj_set_user_data(button, this);
        lv_obj_set_pos(button, 2 + int(i % 4) * 60, 12 + int(i / 4) * 68);
        lv_obj_set_style_bg_color(button, lv_color_hex(kSoundColors[i]), 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(button, 0, 0);
        auto label = lv_obj_get_child(button, 0);
        if (label) {
            lv_obj_set_style_text_font(label, t::fontSmall(), 0);
            lv_obj_set_style_text_color(label, lv_color_hex(0x3A2E2A), 0);
        }
        soundButtons[i] = button;
    }
    auto hint = c::label(soundPage, "点一下就会响", 230);
    lv_obj_set_pos(hint, 5, 156);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hint, t::muted(), 0);
}

void VoiceLab::showPage(Page target) {
    if (page == target) return;
    if (target == Page::Sounds && mode == Mode::Recording) {
        notice("先结束录音");
        return;
    }
    if (target == Page::Record && soundSession) {
        soundSession->requestStop();
        soundSession.reset();
    }
    if (target == Page::Sounds && job && mode == Mode::Playing) job->requestStop();
    pending = Pending::None;
    page = target;
    lv_obj_set_hidden(recordPage, page != Page::Record);
    lv_obj_set_hidden(soundPage, page != Page::Sounds);
    auto pageLabel = lv_obj_get_child(pageButton, 0);
    c::setText(pageLabel, page == Page::Record ? "音效板" : "变声");
    refreshUi();
}

void VoiceLab::startRecording(void) {
    noticeUntil = 0;
    noticeText.clear();
    if (mode == Mode::Recording) {
        if (job) job->requestStop();
        return;
    }
    if (job) {
        job->requestStop();
        pending = Pending::Record;
        setStatus("正在准备音频…");
        return;
    }
    if (soundSession) {
        soundSession->requestStop();
        soundSession.reset();
    }
    if (!core::recorder::idle() || !core::game_audio::idle()) {
        pending = Pending::Record;
        setStatus("正在准备音频…");
        return;
    }
    auto next = core::recorder::start(true);
    if (!next) {
        notice("音频忙，请稍后再试");
        return;
    }
    job = next;
    mode = Mode::Recording;
    recordStarted = lv_tick_get();
    pending = Pending::None;
    setStatus("正在录音…");
}

void VoiceLab::playClip(core::recorder::Effect effect) {
    noticeUntil = 0;
    noticeText.clear();
    if (clipPath.empty()) {
        notice("先录一段再变声");
        return;
    }
    if (mode == Mode::Recording) {
        notice("先结束录音");
        return;
    }
    if (job) {
        job->requestStop();
        pending = Pending::Play;
        pendingEffect = effect;
        setStatus("正在准备音频…");
        return;
    }
    if (soundSession) {
        soundSession->requestStop();
        soundSession.reset();
    }
    if (!core::recorder::idle() || !core::game_audio::idle()) {
        pending = Pending::Play;
        pendingEffect = effect;
        setStatus("正在准备音频…");
        return;
    }
    auto next = core::recorder::start(false, clipPath, effect);
    if (!next) {
        notice("音频忙，请稍后再试");
        return;
    }
    job = next;
    mode = Mode::Playing;
    pending = Pending::None;
}

void VoiceLab::triggerSound(unsigned index) {
    if (index >= 8) return;
    noticeUntil = 0;
    noticeText.clear();
    if (mode == Mode::Recording) {
        notice("先结束录音");
        return;
    }
    if (job) {
        job->requestStop();
        pending = Pending::Sound;
        pendingSound = index;
        return;
    }
    if (!core::recorder::idle()) {
        pending = Pending::Sound;
        pendingSound = index;
        return;
    }
    // The speaker worker ends on its own after a failure or sleep, leaving the
    // Session alive but dead; start a fresh one rather than triggering silence.
    if (soundSession && (soundSession->failed.load() || core::game_audio::idle())) {
        soundSession->requestStop();
        soundSession.reset();
    }
    if (!soundSession) {
        soundSession = core::game_audio::start();
        if (!soundSession) {
            notice("音频忙，请稍后再试");
            return;
        }
        soundSession->setMusic(false);
    }
    soundSession->trigger(kSoundBits[index]);
    notice(kSoundNames[index]);
}

void VoiceLab::processPending(void) {
    if (pending == Pending::None) return;
    if (pending == Pending::Sound) {
        if (job || !core::recorder::idle()) return;
        const unsigned index = pendingSound;
        pending = Pending::None;
        triggerSound(index);
        return;
    }
    if (job || !core::recorder::idle() || !core::game_audio::idle()) return;
    const Pending action = pending;
    const auto effect = pendingEffect;
    pending = Pending::None;
    if (action == Pending::Record) startRecording();
    else if (action == Pending::Play) playClip(effect);
}

void VoiceLab::poll(void) {
    if (!visible) return;
    if (job && (mode == Mode::Recording || mode == Mode::Playing) &&
        sys::Power::isScreenOn() && !sys::Power::sleepPending())
        lv_display_trigger_activity(nullptr);
    if (job && mode == Mode::Recording) {
        const uint32_t elapsed = (lv_tick_get() - recordStarted) / 1000;
        if (elapsed >= kMaxSeconds && !job->stop.load()) job->requestStop();
    }
    if (job && job->done.load(std::memory_order_acquire)) {
        const bool ok = job->ok;
        const bool wasRecording = job->recording;
        const std::string path = job->path;
        const std::string error = job->error;
        job.reset();
        mode = Mode::Idle;
        if (wasRecording) {
            if (ok && !path.empty()) {
                clipPath = path;
                notice("录好了，点效果听听");
            } else {
                notice(error.empty() ? "录音已取消" : error.c_str());
            }
        } else if (!ok) {
            notice(error.empty() ? "播放失败" : error.c_str());
        } else {
            notice("播放完毕");
        }
    }
    processPending();
    refreshUi();
}

void VoiceLab::refreshUi(void) {
    if (!recordPage || !soundPage) return;
    auto pageLabel = lv_obj_get_child(pageButton, 0);
    c::setText(pageLabel, page == Page::Record ? "音效板" : "变声");

    const bool clipEmpty = clipPath.empty();
    const uint32_t signature = (page == Page::Sounds ? 1u : 0u) |
                               (static_cast<uint32_t>(mode) << 1) |
                               (clipEmpty ? 8u : 0u);
    if (signature != uiSignature) {
        uiSignature = signature;
        if (mode == Mode::Recording) {
            c::setText(bigLabel, "停止");
            c::setText(bigIcon, LV_SYMBOL_STOP);
            c::buttonBackground(big, t::danger());
        } else if (mode == Mode::Playing) {
            c::setText(bigLabel, "停止");
            c::setText(bigIcon, LV_SYMBOL_STOP);
            c::buttonBackground(big, t::danger());
        } else {
            c::setText(bigLabel, clipEmpty ? "录音" : "播放");
            c::setText(bigIcon, clipEmpty ? LV_SYMBOL_AUDIO : LV_SYMBOL_PLAY);
            c::buttonBackground(big, t::accent());
        }
        const lv_color_t ink = lv_obj_get_style_text_color(big, LV_PART_MAIN);
        lv_obj_set_style_text_color(bigIcon, ink, 0);
        lv_obj_set_style_text_color(bigLabel, ink, 0);

        for (auto *button : effectButtons) c::disabled(button, clipEmpty || mode == Mode::Recording);
        c::disabled(rerecord, mode == Mode::Recording);
        c::disabled(soundTab, mode == Mode::Recording);
        c::disabled(pageButton, mode == Mode::Recording);
        for (auto *button : soundButtons) c::disabled(button, mode == Mode::Recording);
    }

    const unsigned peak = job ? job->peak.load() : 0;
    lv_bar_set_value(meter, std::min(peak, 32768u) * 1000 / 32768, LV_ANIM_OFF);

    const bool noticeActive = noticeUntil && int32_t(lv_tick_get() - noticeUntil) < 0;
    char buffer[64];
    const char *message = nullptr;
    if (noticeActive) {
        message = noticeText.c_str();
    } else if (job && job->state.load() == core::recorder::State::Saving) {
        message = "正在保存…";
    } else if (pending != Pending::None) {
        message = "正在准备音频…";
    } else if (mode == Mode::Recording) {
        const unsigned seconds = (lv_tick_get() - recordStarted) / 1000;
        std::snprintf(buffer, sizeof(buffer), "正在录音 %u/%u 秒", seconds, kMaxSeconds);
        message = buffer;
    } else if (mode == Mode::Playing) {
        message = "正在变声播放…";
    } else if (page == Page::Sounds) {
        message = "点一下就会响";
    } else if (clipPath.empty()) {
        message = "先录一段，再试试变声";
    } else {
        message = "录好了，点效果听听";
    }
    setStatus(message);
}

void VoiceLab::setStatus(const char *text) {
    if (status) c::setText(status, text ? text : "");
}

void VoiceLab::notice(const char *text) {
    noticeText = text ? text : "";
    noticeUntil = lv_tick_get() + 2500;
    setStatus(noticeText.c_str());
}

void VoiceLab::stopActivity(void) {
    pending = Pending::None;
    if (job) job->requestStop();
    if (soundSession) {
        soundSession->requestStop();
        soundSession.reset();
    }
}

void VoiceLab::tick(lv_timer_t *timer) {
    static_cast<VoiceLab *>(lv_timer_get_user_data(timer))->poll();
}

void VoiceLab::bigClick(lv_event_t *event) {
    auto self = static_cast<VoiceLab *>(lv_event_get_user_data(event));
    if (self->mode == Mode::Recording || self->mode == Mode::Playing) {
        if (self->job) self->job->requestStop();
    } else if (self->clipPath.empty()) {
        self->startRecording();
    } else {
        self->playClip(core::recorder::Effect::None);
    }
    self->poll();
}

void VoiceLab::effectClick(lv_event_t *event) {
    auto self = static_cast<VoiceLab *>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
    const auto index = static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (self && index >= 1 && index <= 5) self->playClip(kEffects[index - 1]);
}

void VoiceLab::soundClick(lv_event_t *event) {
    auto self = static_cast<VoiceLab *>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
    const auto index = static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (self) self->triggerSound(index);
}

void VoiceLab::pageClick(lv_event_t *event) {
    auto self = static_cast<VoiceLab *>(lv_event_get_user_data(event));
    self->showPage(self->page == Page::Record ? Page::Sounds : Page::Record);
}

void VoiceLab::backClick(lv_event_t *event) {
    (void)event;
    ui::WindowManager::instance().goHome();
}

void VoiceLab::rerecordClick(lv_event_t *event) {
    auto self = static_cast<VoiceLab *>(lv_event_get_user_data(event));
    self->startRecording();
}

void VoiceLab::onShow(void) {
    visible = true;
    if (timer) lv_timer_resume(timer);
    refreshUi();
}

void VoiceLab::onHide(void) {
    visible = false;
    if (timer) lv_timer_pause(timer);
    stopActivity();
}

void VoiceLab::onDestroy(void) {
    onHide();
    if (timer) lv_timer_delete(timer);
    timer = nullptr;
    job.reset();
    soundSession.reset();
    root = nullptr;
}

bool VoiceLab::onBack(void) {
    if (mode != Mode::Idle) {
        stopActivity();
        notice("正在停止…");
        return true;
    }
    if (page == Page::Sounds) {
        showPage(Page::Record);
        return true;
    }
    return false;
}

} // namespace apps
