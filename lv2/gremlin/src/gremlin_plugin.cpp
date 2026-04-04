#include <algorithm>
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

    int currentNote;
};

static void apply_parameters(GremlinLV2* self) {
    if (!self || !self->engine) {
        return;
    }

    auto apply = [&](const float* port, auto setter) {
        if (port) {
            (self->engine.get()->*setter)(*port);
        }
    };

    apply(self->mode, &GremlinEngine::setMode);
    apply(self->damage, &GremlinEngine::setDamage);
    apply(self->chaos, &GremlinEngine::setChaos);
    apply(self->noise, &GremlinEngine::setNoise);
    apply(self->drift, &GremlinEngine::setDrift);
    apply(self->crunch, &GremlinEngine::setCrunch);
    apply(self->fold, &GremlinEngine::setFold);
    apply(self->delayTime, &GremlinEngine::setDelayTime);
    apply(self->feedback, &GremlinEngine::setFeedback);
    apply(self->warp, &GremlinEngine::setWarp);
    apply(self->stutter, &GremlinEngine::setStutter);
    apply(self->tone, &GremlinEngine::setTone);
    apply(self->damping, &GremlinEngine::setDamping);
    apply(self->space, &GremlinEngine::setSpace);
    apply(self->attack, &GremlinEngine::setAttack);
    apply(self->release, &GremlinEngine::setRelease);
    apply(self->output, &GremlinEngine::setOutput);
}

static void handle_midi(GremlinLV2* self, const uint8_t* msg, uint32_t size) {
    if (!self || !self->engine || size < 1) {
        return;
    }

    const uint8_t status = msg[0] & 0xF0u;
    const uint8_t data1 = size > 1 ? msg[1] : 0;
    const uint8_t data2 = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (data2 == 0) {
                if (self->currentNote == static_cast<int>(data1)) {
                    self->engine->noteOff(data1);
                    self->currentNote = -1;
                }
                break;
            }
            const float velocity = static_cast<float>(data2) / 127.0f;
            self->engine->noteOn(data1, velocity);
            self->currentNote = static_cast<int>(data1);
            break;
        }
        case LV2_MIDI_MSG_NOTE_OFF:
            if (self->currentNote == static_cast<int>(data1)) {
                self->engine->noteOff(data1);
                self->currentNote = -1;
            }
            break;
        case LV2_MIDI_MSG_CONTROLLER:
            if (data1 == LV2_MIDI_CTL_ALL_NOTES_OFF || data1 == LV2_MIDI_CTL_ALL_SOUNDS_OFF) {
                self->engine->allNotesOff();
                self->currentNote = -1;
            }
            break;
        default:
            break;
    }
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
    self->currentNote = -1;

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
    auto* self = static_cast<flues::gremlin::GremlinLV2*>(instance);
    if (!self) {
        return;
    }

    self->engine = std::make_unique<flues::gremlin::GremlinEngine>(self->sampleRate);
    self->currentNote = -1;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::gremlin;

    auto* self = static_cast<GremlinLV2*>(instance);
    if (!self || !self->engine || !self->audioOutLeft) {
        return;
    }

    apply_parameters(self);

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
                    outLeft[frame] = s.left;
                    outRight[frame] = s.right;
                }
            }

            if (eventFrame >= n_samples) {
                continue;
            }

            if (ev->body.type == self->midiEventUrid) {
                const auto* msg = reinterpret_cast<const uint8_t*>(ev + 1);
                handle_midi(self, msg, ev->body.size);
            }
        }
    }

    for (; frame < n_samples; ++frame) {
        const StereoFrame s = self->engine->process();
        outLeft[frame] = s.left;
        outRight[frame] = s.right;
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
