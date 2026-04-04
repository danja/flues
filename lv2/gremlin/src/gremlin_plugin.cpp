#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include "GremlinEngine.hpp"

#define GREMLIN_URI "https://danja.github.io/flues/plugins/gremlin"

namespace flues::gremlin {

enum PortIndex : uint32_t {
    PORT_AUDIO_OUT_L = 0,
    PORT_AUDIO_OUT_R,
    PORT_MIDI_IN,
    PORT_MODE,
    PORT_DAMAGE,
    PORT_CHAOS,
    PORT_NOISE,
    PORT_DRIFT,
    PORT_CRUNCH,
    PORT_FOLD,
    PORT_DELAY_TIME,
    PORT_FEEDBACK,
    PORT_WARP,
    PORT_STUTTER,
    PORT_TONE,
    PORT_DAMPING,
    PORT_SPACE,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_OUTPUT,
    PORT_TOTAL_COUNT
};

enum LiveParamIndex : uint32_t {
    LIVE_MODE = 0,
    LIVE_DAMAGE,
    LIVE_CHAOS,
    LIVE_NOISE,
    LIVE_DRIFT,
    LIVE_CRUNCH,
    LIVE_FOLD,
    LIVE_DELAY_TIME,
    LIVE_FEEDBACK,
    LIVE_WARP,
    LIVE_STUTTER,
    LIVE_TONE,
    LIVE_DAMPING,
    LIVE_SPACE,
    LIVE_ATTACK,
    LIVE_RELEASE,
    LIVE_OUTPUT,
    LIVE_PARAM_COUNT
};

enum HiddenParamIndex : uint32_t {
    HIDDEN_SOURCE_GAIN = 0,
    HIDDEN_BURST,
    HIDDEN_PITCH_SPREAD,
    HIDDEN_DELAY_MIX,
    HIDDEN_CROSS_FEEDBACK,
    HIDDEN_GLITCH_LENGTH,
    HIDDEN_CHAOS_RATE,
    HIDDEN_DUCK,
    HIDDEN_PARAM_COUNT
};

struct GremlinLV2 {
    std::unique_ptr<GremlinEngine> engine;
    float sampleRate;

    float* audioOutLeft;
    float* audioOutRight;
    const LV2_Atom_Sequence* midiIn;

    const float* mode;
    const float* damage;
    const float* chaos;
    const float* noise;
    const float* drift;
    const float* crunch;
    const float* fold;
    const float* delayTime;
    const float* feedback;
    const float* warp;
    const float* stutter;
    const float* tone;
    const float* damping;
    const float* space;
    const float* attack;
    const float* release;
    const float* output;

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;

    float liveParams[LIVE_PARAM_COUNT];
    bool liveOverrides[LIVE_PARAM_COUNT];
    float hiddenParams[HIDDEN_PARAM_COUNT];
    float macroFaders[8];
    float masterTrim;
    bool momentaryButtons[8];
    int currentScene;
    int currentNote;
    float postGain;
    uint32_t rngState;
};

static constexpr std::array<uint8_t, 16> kPrimaryKnobCCs = {
    16, 20, 24, 28, 46, 50, 54, 58,
    17, 21, 25, 29, 47, 51, 55, 59
};

static constexpr std::array<LiveParamIndex, 16> kPrimaryKnobTargets = {
    LIVE_DAMAGE,
    LIVE_CHAOS,
    LIVE_NOISE,
    LIVE_DRIFT,
    LIVE_CRUNCH,
    LIVE_FOLD,
    LIVE_ATTACK,
    LIVE_RELEASE,
    LIVE_DELAY_TIME,
    LIVE_FEEDBACK,
    LIVE_WARP,
    LIVE_STUTTER,
    LIVE_TONE,
    LIVE_DAMPING,
    LIVE_SPACE,
    LIVE_OUTPUT
};

static constexpr std::array<uint8_t, 8> kHiddenKnobCCs = {
    18, 22, 26, 30, 48, 52, 56, 60
};

static constexpr std::array<HiddenParamIndex, 8> kHiddenKnobTargets = {
    HIDDEN_SOURCE_GAIN,
    HIDDEN_BURST,
    HIDDEN_PITCH_SPREAD,
    HIDDEN_DELAY_MIX,
    HIDDEN_CROSS_FEEDBACK,
    HIDDEN_GLITCH_LENGTH,
    HIDDEN_CHAOS_RATE,
    HIDDEN_DUCK
};

static constexpr std::array<uint8_t, 8> kMacroFaderCCs = {
    19, 23, 27, 31, 49, 53, 57, 61
};

static constexpr uint8_t kMasterFaderCC = 62;

static constexpr std::array<uint8_t, 8> kMuteNotes = {
    1, 4, 7, 10, 13, 16, 19, 22
};

static constexpr std::array<uint8_t, 8> kSoloMuteNotes = {
    2, 5, 8, 11, 14, 17, 20, 23
};

static constexpr std::array<uint8_t, 8> kRecArmNotes = {
    3, 6, 9, 12, 15, 18, 21, 24
};

static constexpr uint8_t kBankLeftNote = 25;
static constexpr uint8_t kBankRightNote = 26;
static constexpr uint8_t kSoloNote = 27;

static float clampf(float value, float minValue = 0.0f, float maxValue = 1.0f) {
    return std::clamp(value, minValue, maxValue);
}

static float default_live_value(LiveParamIndex index) {
    switch (index) {
        case LIVE_MODE: return 0.0f;
        case LIVE_DAMAGE: return 0.55f;
        case LIVE_CHAOS: return 0.60f;
        case LIVE_NOISE: return 0.30f;
        case LIVE_DRIFT: return 0.35f;
        case LIVE_CRUNCH: return 0.45f;
        case LIVE_FOLD: return 0.35f;
        case LIVE_DELAY_TIME: return 0.32f;
        case LIVE_FEEDBACK: return 0.55f;
        case LIVE_WARP: return 0.45f;
        case LIVE_STUTTER: return 0.35f;
        case LIVE_TONE: return 0.65f;
        case LIVE_DAMPING: return 0.45f;
        case LIVE_SPACE: return 0.55f;
        case LIVE_ATTACK: return 0.05f;
        case LIVE_RELEASE: return 0.25f;
        case LIVE_OUTPUT: return 0.70f;
        default: return 0.5f;
    }
}

static float default_hidden_value(HiddenParamIndex index) {
    switch (index) {
        case HIDDEN_SOURCE_GAIN: return 0.72f;
        case HIDDEN_BURST: return 0.40f;
        case HIDDEN_PITCH_SPREAD: return 0.50f;
        case HIDDEN_DELAY_MIX: return 0.55f;
        case HIDDEN_CROSS_FEEDBACK: return 0.50f;
        case HIDDEN_GLITCH_LENGTH: return 0.50f;
        case HIDDEN_CHAOS_RATE: return 0.55f;
        case HIDDEN_DUCK: return 0.10f;
        default: return 0.5f;
    }
}

static float port_or_default(const float* port, float fallback) {
    return port ? *port : fallback;
}

static uint32_t next_u32(GremlinLV2* self) {
    self->rngState = self->rngState * 1664525u + 1013904223u;
    return self->rngState;
}

static float rand01(GremlinLV2* self) {
    return static_cast<float>((next_u32(self) >> 8) & 0x00FFFFFFu) * (1.0f / 16777215.0f);
}

static float rand_range(GremlinLV2* self, float minValue, float maxValue) {
    return minValue + (maxValue - minValue) * rand01(self);
}

static void set_live_param(GremlinLV2* self, LiveParamIndex index, float value) {
    if (!self) {
        return;
    }

    if (index == LIVE_MODE) {
        self->liveParams[index] = clampf(std::round(value), 0.0f, 3.0f);
    } else {
        self->liveParams[index] = clampf(value);
    }
    self->liveOverrides[index] = true;
}

static void set_hidden_param(GremlinLV2* self, HiddenParamIndex index, float value) {
    if (!self) {
        return;
    }
    self->hiddenParams[index] = clampf(value);
}

static void sync_from_ports(GremlinLV2* self) {
    if (!self) {
        return;
    }

    if (!self->liveOverrides[LIVE_MODE]) self->liveParams[LIVE_MODE] = clampf(port_or_default(self->mode, default_live_value(LIVE_MODE)), 0.0f, 3.0f);
    if (!self->liveOverrides[LIVE_DAMAGE]) self->liveParams[LIVE_DAMAGE] = clampf(port_or_default(self->damage, default_live_value(LIVE_DAMAGE)));
    if (!self->liveOverrides[LIVE_CHAOS]) self->liveParams[LIVE_CHAOS] = clampf(port_or_default(self->chaos, default_live_value(LIVE_CHAOS)));
    if (!self->liveOverrides[LIVE_NOISE]) self->liveParams[LIVE_NOISE] = clampf(port_or_default(self->noise, default_live_value(LIVE_NOISE)));
    if (!self->liveOverrides[LIVE_DRIFT]) self->liveParams[LIVE_DRIFT] = clampf(port_or_default(self->drift, default_live_value(LIVE_DRIFT)));
    if (!self->liveOverrides[LIVE_CRUNCH]) self->liveParams[LIVE_CRUNCH] = clampf(port_or_default(self->crunch, default_live_value(LIVE_CRUNCH)));
    if (!self->liveOverrides[LIVE_FOLD]) self->liveParams[LIVE_FOLD] = clampf(port_or_default(self->fold, default_live_value(LIVE_FOLD)));
    if (!self->liveOverrides[LIVE_DELAY_TIME]) self->liveParams[LIVE_DELAY_TIME] = clampf(port_or_default(self->delayTime, default_live_value(LIVE_DELAY_TIME)));
    if (!self->liveOverrides[LIVE_FEEDBACK]) self->liveParams[LIVE_FEEDBACK] = clampf(port_or_default(self->feedback, default_live_value(LIVE_FEEDBACK)));
    if (!self->liveOverrides[LIVE_WARP]) self->liveParams[LIVE_WARP] = clampf(port_or_default(self->warp, default_live_value(LIVE_WARP)));
    if (!self->liveOverrides[LIVE_STUTTER]) self->liveParams[LIVE_STUTTER] = clampf(port_or_default(self->stutter, default_live_value(LIVE_STUTTER)));
    if (!self->liveOverrides[LIVE_TONE]) self->liveParams[LIVE_TONE] = clampf(port_or_default(self->tone, default_live_value(LIVE_TONE)));
    if (!self->liveOverrides[LIVE_DAMPING]) self->liveParams[LIVE_DAMPING] = clampf(port_or_default(self->damping, default_live_value(LIVE_DAMPING)));
    if (!self->liveOverrides[LIVE_SPACE]) self->liveParams[LIVE_SPACE] = clampf(port_or_default(self->space, default_live_value(LIVE_SPACE)));
    if (!self->liveOverrides[LIVE_ATTACK]) self->liveParams[LIVE_ATTACK] = clampf(port_or_default(self->attack, default_live_value(LIVE_ATTACK)));
    if (!self->liveOverrides[LIVE_RELEASE]) self->liveParams[LIVE_RELEASE] = clampf(port_or_default(self->release, default_live_value(LIVE_RELEASE)));
    if (!self->liveOverrides[LIVE_OUTPUT]) self->liveParams[LIVE_OUTPUT] = clampf(port_or_default(self->output, default_live_value(LIVE_OUTPUT)));
}

static void reset_midimix_state(GremlinLV2* self) {
    if (!self) {
        return;
    }

    for (uint32_t i = 0; i < LIVE_PARAM_COUNT; ++i) {
        self->liveParams[i] = default_live_value(static_cast<LiveParamIndex>(i));
        self->liveOverrides[i] = false;
    }
    for (uint32_t i = 0; i < HIDDEN_PARAM_COUNT; ++i) {
        self->hiddenParams[i] = default_hidden_value(static_cast<HiddenParamIndex>(i));
    }
    for (float& macro : self->macroFaders) {
        macro = 0.5f;
    }
    for (bool& held : self->momentaryButtons) {
        held = false;
    }

    self->masterTrim = 0.75f;
    self->currentScene = -1;
    self->postGain = 1.0f;
}

static int array_index(const std::array<uint8_t, 8>& ids, uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (ids[static_cast<size_t>(i)] == value) {
            return i;
        }
    }
    return -1;
}

static int primary_cc_index(uint8_t cc) {
    for (int i = 0; i < 16; ++i) {
        if (kPrimaryKnobCCs[static_cast<size_t>(i)] == cc) {
            return i;
        }
    }
    return -1;
}

static int hidden_cc_index(uint8_t cc) {
    for (int i = 0; i < 8; ++i) {
        if (kHiddenKnobCCs[static_cast<size_t>(i)] == cc) {
            return i;
        }
    }
    return -1;
}

static int macro_cc_index(uint8_t cc) {
    for (int i = 0; i < 8; ++i) {
        if (kMacroFaderCCs[static_cast<size_t>(i)] == cc) {
            return i;
        }
    }
    return -1;
}

static void cycle_mode(GremlinLV2* self, int delta) {
    const int mode = (static_cast<int>(std::lround(self->liveParams[LIVE_MODE])) + delta + 4) % 4;
    set_live_param(self, LIVE_MODE, static_cast<float>(mode));
}

static void load_scene(GremlinLV2* self, int sceneIndex) {
    if (!self) {
        return;
    }

    switch (sceneIndex) {
        case 0:
            set_live_param(self, LIVE_MODE, 0.0f);
            set_live_param(self, LIVE_DAMAGE, 0.68f);
            set_live_param(self, LIVE_CHAOS, 0.58f);
            set_live_param(self, LIVE_NOISE, 0.18f);
            set_live_param(self, LIVE_CRUNCH, 0.64f);
            set_live_param(self, LIVE_FOLD, 0.53f);
            set_live_param(self, LIVE_DELAY_TIME, 0.16f);
            set_live_param(self, LIVE_FEEDBACK, 0.42f);
            set_live_param(self, LIVE_WARP, 0.27f);
            set_live_param(self, LIVE_STUTTER, 0.36f);
            set_live_param(self, LIVE_TONE, 0.72f);
            set_hidden_param(self, HIDDEN_SOURCE_GAIN, 0.74f);
            set_hidden_param(self, HIDDEN_BURST, 0.62f);
            set_hidden_param(self, HIDDEN_PITCH_SPREAD, 0.42f);
            set_hidden_param(self, HIDDEN_DELAY_MIX, 0.35f);
            set_hidden_param(self, HIDDEN_GLITCH_LENGTH, 0.28f);
            break;
        case 1:
            set_live_param(self, LIVE_MODE, 1.0f);
            set_live_param(self, LIVE_DAMAGE, 0.46f);
            set_live_param(self, LIVE_CHAOS, 0.72f);
            set_live_param(self, LIVE_DRIFT, 0.52f);
            set_live_param(self, LIVE_DELAY_TIME, 0.48f);
            set_live_param(self, LIVE_FEEDBACK, 0.74f);
            set_live_param(self, LIVE_WARP, 0.62f);
            set_live_param(self, LIVE_STUTTER, 0.22f);
            set_live_param(self, LIVE_DAMPING, 0.62f);
            set_live_param(self, LIVE_SPACE, 0.78f);
            set_hidden_param(self, HIDDEN_DELAY_MIX, 0.82f);
            set_hidden_param(self, HIDDEN_CROSS_FEEDBACK, 0.72f);
            set_hidden_param(self, HIDDEN_GLITCH_LENGTH, 0.76f);
            set_hidden_param(self, HIDDEN_CHAOS_RATE, 0.48f);
            set_hidden_param(self, HIDDEN_DUCK, 0.18f);
            break;
        case 2:
            set_live_param(self, LIVE_MODE, 2.0f);
            set_live_param(self, LIVE_DAMAGE, 0.78f);
            set_live_param(self, LIVE_CHAOS, 0.84f);
            set_live_param(self, LIVE_NOISE, 0.56f);
            set_live_param(self, LIVE_CRUNCH, 0.78f);
            set_live_param(self, LIVE_FOLD, 0.66f);
            set_live_param(self, LIVE_DELAY_TIME, 0.24f);
            set_live_param(self, LIVE_FEEDBACK, 0.51f);
            set_live_param(self, LIVE_WARP, 0.54f);
            set_live_param(self, LIVE_STUTTER, 0.61f);
            set_hidden_param(self, HIDDEN_SOURCE_GAIN, 0.86f);
            set_hidden_param(self, HIDDEN_BURST, 0.74f);
            set_hidden_param(self, HIDDEN_PITCH_SPREAD, 0.72f);
            set_hidden_param(self, HIDDEN_DELAY_MIX, 0.46f);
            set_hidden_param(self, HIDDEN_GLITCH_LENGTH, 0.58f);
            set_hidden_param(self, HIDDEN_CHAOS_RATE, 0.78f);
            break;
        case 3:
        default:
            set_live_param(self, LIVE_MODE, 3.0f);
            set_live_param(self, LIVE_DAMAGE, 0.60f);
            set_live_param(self, LIVE_CHAOS, 0.66f);
            set_live_param(self, LIVE_NOISE, 0.34f);
            set_live_param(self, LIVE_DRIFT, 0.64f);
            set_live_param(self, LIVE_DELAY_TIME, 0.72f);
            set_live_param(self, LIVE_FEEDBACK, 0.86f);
            set_live_param(self, LIVE_WARP, 0.71f);
            set_live_param(self, LIVE_STUTTER, 0.41f);
            set_live_param(self, LIVE_TONE, 0.40f);
            set_live_param(self, LIVE_DAMPING, 0.76f);
            set_live_param(self, LIVE_SPACE, 0.88f);
            set_hidden_param(self, HIDDEN_DELAY_MIX, 0.88f);
            set_hidden_param(self, HIDDEN_CROSS_FEEDBACK, 0.82f);
            set_hidden_param(self, HIDDEN_GLITCH_LENGTH, 0.68f);
            set_hidden_param(self, HIDDEN_DUCK, 0.30f);
            break;
    }

    self->currentScene = sceneIndex;
    self->engine->triggerBurst(0.65f);
}

static void cycle_scene(GremlinLV2* self, int delta) {
    const int scene = self->currentScene >= 0 ? self->currentScene : 0;
    load_scene(self, (scene + delta + 4) % 4);
}

static void randomize_source(GremlinLV2* self) {
    set_live_param(self, LIVE_MODE, static_cast<float>(static_cast<int>(rand01(self) * 4.0f) % 4));
    set_live_param(self, LIVE_DAMAGE, rand_range(self, 0.25f, 0.90f));
    set_live_param(self, LIVE_CHAOS, rand_range(self, 0.20f, 0.95f));
    set_live_param(self, LIVE_NOISE, rand_range(self, 0.0f, 0.75f));
    set_live_param(self, LIVE_DRIFT, rand_range(self, 0.0f, 0.85f));
    set_live_param(self, LIVE_CRUNCH, rand_range(self, 0.15f, 0.92f));
    set_live_param(self, LIVE_FOLD, rand_range(self, 0.08f, 0.85f));
    set_hidden_param(self, HIDDEN_SOURCE_GAIN, rand_range(self, 0.35f, 0.95f));
    set_hidden_param(self, HIDDEN_BURST, rand_range(self, 0.18f, 0.95f));
    set_hidden_param(self, HIDDEN_PITCH_SPREAD, rand_range(self, 0.12f, 0.92f));
    set_hidden_param(self, HIDDEN_CHAOS_RATE, rand_range(self, 0.08f, 0.95f));
    self->engine->reseedChaos(next_u32(self));
}

static void randomize_delay(GremlinLV2* self) {
    set_live_param(self, LIVE_DELAY_TIME, rand_range(self, 0.05f, 0.95f));
    set_live_param(self, LIVE_FEEDBACK, rand_range(self, 0.10f, 0.95f));
    set_live_param(self, LIVE_WARP, rand_range(self, 0.05f, 0.95f));
    set_live_param(self, LIVE_STUTTER, rand_range(self, 0.0f, 0.95f));
    set_live_param(self, LIVE_TONE, rand_range(self, 0.20f, 0.95f));
    set_live_param(self, LIVE_DAMPING, rand_range(self, 0.05f, 0.92f));
    set_live_param(self, LIVE_SPACE, rand_range(self, 0.10f, 0.98f));
    set_hidden_param(self, HIDDEN_DELAY_MIX, rand_range(self, 0.10f, 0.95f));
    set_hidden_param(self, HIDDEN_CROSS_FEEDBACK, rand_range(self, 0.0f, 0.92f));
    set_hidden_param(self, HIDDEN_GLITCH_LENGTH, rand_range(self, 0.12f, 0.95f));
    set_hidden_param(self, HIDDEN_DUCK, rand_range(self, 0.0f, 0.55f));
}

static void clear_midimix_overrides(GremlinLV2* self) {
    reset_midimix_state(self);
    sync_from_ports(self);
}

static void apply_live_state(GremlinLV2* self) {
    if (!self || !self->engine) {
        return;
    }

    float damage = self->liveParams[LIVE_DAMAGE];
    float chaos = self->liveParams[LIVE_CHAOS];
    float noise = self->liveParams[LIVE_NOISE];
    float drift = self->liveParams[LIVE_DRIFT];
    float crunch = self->liveParams[LIVE_CRUNCH];
    float fold = self->liveParams[LIVE_FOLD];
    float delayTime = self->liveParams[LIVE_DELAY_TIME];
    float feedback = self->liveParams[LIVE_FEEDBACK];
    float warp = self->liveParams[LIVE_WARP];
    float stutter = self->liveParams[LIVE_STUTTER];
    float tone = self->liveParams[LIVE_TONE];
    float damping = self->liveParams[LIVE_DAMPING];
    float space = self->liveParams[LIVE_SPACE];
    float attack = self->liveParams[LIVE_ATTACK];
    float release = self->liveParams[LIVE_RELEASE];
    float output = self->liveParams[LIVE_OUTPUT];

    float sourceGain = self->hiddenParams[HIDDEN_SOURCE_GAIN];
    float burst = self->hiddenParams[HIDDEN_BURST];
    float pitchSpread = self->hiddenParams[HIDDEN_PITCH_SPREAD];
    float delayMix = self->hiddenParams[HIDDEN_DELAY_MIX];
    float crossFeedback = self->hiddenParams[HIDDEN_CROSS_FEEDBACK];
    float glitchLength = self->hiddenParams[HIDDEN_GLITCH_LENGTH];
    float chaosRate = self->hiddenParams[HIDDEN_CHAOS_RATE];
    float duck = self->hiddenParams[HIDDEN_DUCK];

    const float sourceMacro = self->macroFaders[0] - 0.5f;
    const float pitchMacro = self->macroFaders[1] - 0.5f;
    const float breakMacro = self->macroFaders[2] - 0.5f;
    const float delayMacro = self->macroFaders[3] - 0.5f;
    const float spaceMacro = self->macroFaders[4] - 0.5f;
    const float stutterMacro = self->macroFaders[5] - 0.5f;
    const float toneMacro = self->macroFaders[6] - 0.5f;
    const float outputMacro = self->macroFaders[7] - 0.5f;

    damage = clampf(damage + sourceMacro * 0.42f + breakMacro * 0.22f);
    chaos = clampf(chaos + sourceMacro * 0.52f + pitchMacro * 0.18f + (self->momentaryButtons[2] ? 0.34f : 0.0f));
    noise = clampf(noise + sourceMacro * 0.32f + (self->momentaryButtons[6] ? 0.60f : 0.0f));
    drift = clampf(drift + pitchMacro * 0.65f);
    attack = clampf(attack - pitchMacro * 0.28f);
    release = clampf(release + pitchMacro * 0.35f);

    crunch = clampf(crunch + breakMacro * 0.72f + (self->momentaryButtons[3] ? 0.58f : 0.0f));
    fold = clampf(fold + breakMacro * 0.60f);

    delayTime = clampf(delayTime + delayMacro * 0.72f);
    feedback = clampf(feedback + delayMacro * 0.52f + (self->momentaryButtons[4] ? 0.34f : 0.0f));
    warp = clampf(warp + spaceMacro * 0.55f + (self->momentaryButtons[5] ? 0.50f : 0.0f));
    stutter = clampf(stutter + stutterMacro * 0.82f + (self->momentaryButtons[1] ? 0.90f : 0.0f));

    tone = clampf(tone + toneMacro * 0.58f);
    damping = clampf(damping - toneMacro * 0.45f);
    space = clampf(space + spaceMacro * 0.76f);
    output = clampf(output + outputMacro * 0.62f);

    sourceGain = clampf(sourceGain + sourceMacro * 0.45f + outputMacro * 0.18f);
    burst = clampf(burst + pitchMacro * 0.32f + breakMacro * 0.22f);
    pitchSpread = clampf(pitchSpread + pitchMacro * 0.60f);
    delayMix = clampf(delayMix + delayMacro * 0.72f);
    crossFeedback = clampf(crossFeedback + spaceMacro * 0.58f);
    glitchLength = clampf(glitchLength + stutterMacro * 0.66f);
    chaosRate = clampf(chaosRate + sourceMacro * 0.18f + spaceMacro * 0.18f);
    duck = clampf(duck + toneMacro * 0.42f + (self->momentaryButtons[7] ? 0.55f : 0.0f));

    self->engine->setMode(self->liveParams[LIVE_MODE]);
    self->engine->setDamage(damage);
    self->engine->setChaos(chaos);
    self->engine->setNoise(noise);
    self->engine->setDrift(drift);
    self->engine->setCrunch(crunch);
    self->engine->setFold(fold);
    self->engine->setDelayTime(delayTime);
    self->engine->setFeedback(feedback);
    self->engine->setWarp(warp);
    self->engine->setStutter(stutter);
    self->engine->setSourceGain(sourceGain);
    self->engine->setBurst(burst);
    self->engine->setPitchSpread(pitchSpread);
    self->engine->setDelayMix(delayMix);
    self->engine->setCrossFeedback(crossFeedback);
    self->engine->setGlitchLength(glitchLength);
    self->engine->setChaosRate(chaosRate);
    self->engine->setDuck(duck);
    self->engine->setTone(tone);
    self->engine->setDamping(damping);
    self->engine->setSpace(space);
    self->engine->setAttack(attack);
    self->engine->setRelease(release);
    self->engine->setOutput(output);
    self->engine->setFreezeDelay(self->momentaryButtons[0]);

    self->postGain = 0.18f + self->masterTrim * 1.82f;
}

static bool handle_midimix_cc(GremlinLV2* self, uint8_t cc, uint8_t value) {
    const float normalized = static_cast<float>(value) / 127.0f;

    const int primary = primary_cc_index(cc);
    if (primary >= 0) {
        set_live_param(self, kPrimaryKnobTargets[static_cast<size_t>(primary)], normalized);
        return true;
    }

    const int hidden = hidden_cc_index(cc);
    if (hidden >= 0) {
        set_hidden_param(self, kHiddenKnobTargets[static_cast<size_t>(hidden)], normalized);
        return true;
    }

    const int macro = macro_cc_index(cc);
    if (macro >= 0) {
        self->macroFaders[static_cast<size_t>(macro)] = normalized;
        return true;
    }

    if (cc == kMasterFaderCC) {
        self->masterTrim = normalized;
        return true;
    }

    return false;
}

static bool handle_midimix_button(GremlinLV2* self, uint8_t note, bool pressed) {
    const int muteIndex = array_index(kMuteNotes, note);
    if (muteIndex >= 0) {
        self->momentaryButtons[static_cast<size_t>(muteIndex)] = pressed;
        return true;
    }

    if (!pressed) {
        return note == kSoloNote;
    }

    const int recIndex = array_index(kRecArmNotes, note);
    if (recIndex >= 0) {
        if (recIndex < 4) {
            set_live_param(self, LIVE_MODE, static_cast<float>(recIndex));
        } else {
            load_scene(self, recIndex - 4);
        }
        return true;
    }

    const int soloMuteIndex = array_index(kSoloMuteNotes, note);
    if (soloMuteIndex >= 0) {
        switch (soloMuteIndex) {
            case 0:
                self->engine->reseedChaos(next_u32(self));
                break;
            case 1:
                self->engine->triggerBurst(1.0f);
                break;
            case 2:
                randomize_source(self);
                break;
            case 3:
                randomize_delay(self);
                break;
            case 4:
                randomize_source(self);
                randomize_delay(self);
                break;
            case 5:
                cycle_scene(self, -1);
                break;
            case 6:
                cycle_scene(self, 1);
                break;
            case 7:
                clear_midimix_overrides(self);
                self->engine->allNotesOff();
                self->currentNote = -1;
                break;
            default:
                break;
        }
        return true;
    }

    if (note == kBankLeftNote) {
        cycle_mode(self, -1);
        return true;
    }
    if (note == kBankRightNote) {
        cycle_mode(self, 1);
        return true;
    }
    if (note == kSoloNote) {
        return true;
    }

    return false;
}

static bool is_midimix_button_note(uint8_t note) {
    return array_index(kMuteNotes, note) >= 0
        || array_index(kSoloMuteNotes, note) >= 0
        || array_index(kRecArmNotes, note) >= 0
        || note == kBankLeftNote
        || note == kBankRightNote
        || note == kSoloNote;
}

static bool handle_midi(GremlinLV2* self, const uint8_t* msg, uint32_t size) {
    if (!self || !self->engine || size < 1) {
        return false;
    }

    const uint8_t status = msg[0] & 0xF0u;
    const uint8_t data1 = size > 1 ? msg[1] : 0;
    const uint8_t data2 = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (data2 == 0) {
                if (is_midimix_button_note(data1)) {
                    return handle_midimix_button(self, data1, false);
                }
                if (self->currentNote == static_cast<int>(data1)) {
                    self->engine->noteOff(data1);
                    self->currentNote = -1;
                }
                break;
            }

            if (data1 <= 27 && data2 == 127 && is_midimix_button_note(data1)) {
                return handle_midimix_button(self, data1, true);
            }

            const float velocity = static_cast<float>(data2) / 127.0f;
            self->engine->noteOn(data1, velocity);
            self->currentNote = static_cast<int>(data1);
            break;
        }
        case LV2_MIDI_MSG_NOTE_OFF:
            if (is_midimix_button_note(data1)) {
                return handle_midimix_button(self, data1, false);
            }
            if (self->currentNote == static_cast<int>(data1)) {
                self->engine->noteOff(data1);
                self->currentNote = -1;
            }
            break;
        case LV2_MIDI_MSG_CONTROLLER:
            if (data1 == LV2_MIDI_CTL_ALL_NOTES_OFF || data1 == LV2_MIDI_CTL_ALL_SOUNDS_OFF) {
                self->engine->allNotesOff();
                self->currentNote = -1;
                return false;
            }
            return handle_midimix_cc(self, data1, data2);
        default:
            break;
    }

    return false;
}

} // namespace flues::gremlin

extern "C" {

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                              const char*, const LV2_Feature* const* features) {
    using namespace flues::gremlin;

    auto* self = new GremlinLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<GremlinEngine>(self->sampleRate);

    self->audioOutLeft = nullptr;
    self->audioOutRight = nullptr;
    self->midiIn = nullptr;

    self->mode = nullptr;
    self->damage = nullptr;
    self->chaos = nullptr;
    self->noise = nullptr;
    self->drift = nullptr;
    self->crunch = nullptr;
    self->fold = nullptr;
    self->delayTime = nullptr;
    self->feedback = nullptr;
    self->warp = nullptr;
    self->stutter = nullptr;
    self->tone = nullptr;
    self->damping = nullptr;
    self->space = nullptr;
    self->attack = nullptr;
    self->release = nullptr;
    self->output = nullptr;

    self->map = nullptr;
    self->midiEventUrid = 0;
    self->atomSequenceUrid = 0;
    self->currentScene = -1;
    self->currentNote = -1;
    self->postGain = 1.0f;
    self->rngState = 0x4d3c2b1au;

    reset_midimix_state(self);

    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!std::strcmp((*f)->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>((*f)->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);

    return self;
}

static void cleanup(LV2_Handle instance) {
    auto* self = static_cast<flues::gremlin::GremlinLV2*>(instance);
    delete self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    using namespace flues::gremlin;

    auto* self = static_cast<GremlinLV2*>(instance);
    switch (port) {
        case PORT_AUDIO_OUT_L: self->audioOutLeft = static_cast<float*>(data); break;
        case PORT_AUDIO_OUT_R: self->audioOutRight = static_cast<float*>(data); break;
        case PORT_MIDI_IN: self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_MODE: self->mode = static_cast<const float*>(data); break;
        case PORT_DAMAGE: self->damage = static_cast<const float*>(data); break;
        case PORT_CHAOS: self->chaos = static_cast<const float*>(data); break;
        case PORT_NOISE: self->noise = static_cast<const float*>(data); break;
        case PORT_DRIFT: self->drift = static_cast<const float*>(data); break;
        case PORT_CRUNCH: self->crunch = static_cast<const float*>(data); break;
        case PORT_FOLD: self->fold = static_cast<const float*>(data); break;
        case PORT_DELAY_TIME: self->delayTime = static_cast<const float*>(data); break;
        case PORT_FEEDBACK: self->feedback = static_cast<const float*>(data); break;
        case PORT_WARP: self->warp = static_cast<const float*>(data); break;
        case PORT_STUTTER: self->stutter = static_cast<const float*>(data); break;
        case PORT_TONE: self->tone = static_cast<const float*>(data); break;
        case PORT_DAMPING: self->damping = static_cast<const float*>(data); break;
        case PORT_SPACE: self->space = static_cast<const float*>(data); break;
        case PORT_ATTACK: self->attack = static_cast<const float*>(data); break;
        case PORT_RELEASE: self->release = static_cast<const float*>(data); break;
        case PORT_OUTPUT: self->output = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    using namespace flues::gremlin;

    auto* self = static_cast<GremlinLV2*>(instance);
    if (!self) {
        return;
    }

    self->engine = std::make_unique<GremlinEngine>(self->sampleRate);
    self->currentNote = -1;
    self->rngState = 0x4d3c2b1au;
    reset_midimix_state(self);
    sync_from_ports(self);
    apply_live_state(self);
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::gremlin;

    auto* self = static_cast<GremlinLV2*>(instance);
    if (!self || !self->engine || !self->audioOutLeft) {
        return;
    }

    sync_from_ports(self);
    apply_live_state(self);

    float* outLeft = self->audioOutLeft;
    float* outRight = self->audioOutRight ? self->audioOutRight : self->audioOutLeft;

    std::memset(outLeft, 0, n_samples * sizeof(float));
    if (outRight != outLeft) {
        std::memset(outRight, 0, n_samples * sizeof(float));
    }

    uint32_t frame = 0;

    if (self->midiIn && self->midiIn->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midiIn, ev) {
            const uint32_t eventFrame = ev->time.frames >= 0
                ? static_cast<uint32_t>(ev->time.frames)
                : 0u;

            if (frame < eventFrame) {
                const uint32_t limit = std::min(eventFrame, n_samples);
                for (; frame < limit; ++frame) {
                    const StereoFrame s = self->engine->process();
                    outLeft[frame] = s.left * self->postGain;
                    outRight[frame] = s.right * self->postGain;
                }
            }

            if (eventFrame >= n_samples) {
                continue;
            }

            if (ev->body.type == self->midiEventUrid) {
                const auto* msg = reinterpret_cast<const uint8_t*>(ev + 1);
                if (handle_midi(self, msg, ev->body.size)) {
                    apply_live_state(self);
                }
            }
        }
    }

    for (; frame < n_samples; ++frame) {
        const StereoFrame s = self->engine->process();
        outLeft[frame] = s.left * self->postGain;
        outRight[frame] = s.right * self->postGain;
    }
}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    GREMLIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    nullptr,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

} // extern "C"
