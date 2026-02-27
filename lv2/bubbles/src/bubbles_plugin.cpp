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

#include "BubblesEngine.hpp"

#define BUBBLES_URI "https://danja.github.io/flues/plugins/bubbles"

namespace flues::bubbles {

enum PortIndex : uint32_t {
    PORT_AUDIO_OUT_L = 0,
    PORT_AUDIO_OUT_R,
    PORT_MIDI_IN,
    PORT_INTENSITY,
    PORT_DENSITY,
    PORT_SIZE,
    PORT_FLOW_RATE,
    PORT_BRIGHTNESS,
    PORT_RESONANCE,
    PORT_DEPTH,
    PORT_SPACE,
    PORT_RANDOMNESS,
    PORT_HEAT,
    PORT_OUTPUT,
    PORT_MODE,
    PORT_NOISE_FLOOR,
    PORT_DRIVE,
    PORT_TOTAL_COUNT
};

struct BubblesLV2 {
    std::unique_ptr<BubblesEngine> engine;
    float sampleRate;

    float* audioOutLeft;
    float* audioOutRight;
    const LV2_Atom_Sequence* midiIn;

    const float* intensity;
    const float* density;
    const float* size;
    const float* flowRate;
    const float* brightness;
    const float* resonance;
    const float* depth;
    const float* space;
    const float* randomness;
    const float* heat;
    const float* output;
    const float* drive;
    const float* mode;
    const float* noiseFloor;

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;

    int currentNote;
};

static void apply_parameters(BubblesLV2* self) {
    if (!self || !self->engine) {
        return;
    }

    auto apply = [&](const float* port, auto setter) {
        if (port) {
            (self->engine.get()->*setter)(*port);
        }
    };

    apply(self->intensity, &BubblesEngine::setIntensity);
    apply(self->density, &BubblesEngine::setDensity);
    apply(self->size, &BubblesEngine::setSize);
    apply(self->flowRate, &BubblesEngine::setFlowRate);
    apply(self->brightness, &BubblesEngine::setBrightness);
    apply(self->resonance, &BubblesEngine::setResonance);
    apply(self->depth, &BubblesEngine::setDepth);
    apply(self->space, &BubblesEngine::setSpace);
    apply(self->randomness, &BubblesEngine::setRandomness);
    apply(self->heat, &BubblesEngine::setHeat);
    apply(self->output, &BubblesEngine::setOutput);
    apply(self->drive, &BubblesEngine::setDrive);
    apply(self->mode, &BubblesEngine::setMode);
    apply(self->noiseFloor, &BubblesEngine::setNoiseFloor);
}

static void handle_midi(BubblesLV2* self, const uint8_t* msg, uint32_t size) {
    if (!self || !self->engine || size < 1) {
        return;
    }

    const uint8_t status = msg[0] & 0xF0U;
    const uint8_t note = size > 1 ? msg[1] : 0;
    const uint8_t vel = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (vel == 0) {
                if (self->currentNote == note) {
                    self->engine->noteOff(note);
                    self->currentNote = -1;
                }
                break;
            }
            const float velocity = static_cast<float>(vel) / 127.0f;
            self->engine->noteOn(note, velocity);
            self->currentNote = static_cast<int>(note);
            break;
        }
        case LV2_MIDI_MSG_NOTE_OFF:
            if (self->currentNote == note) {
                self->engine->noteOff(note);
                self->currentNote = -1;
            }
            break;
        case LV2_MIDI_MSG_CONTROLLER:
            if (note == LV2_MIDI_CTL_ALL_NOTES_OFF || note == LV2_MIDI_CTL_ALL_SOUNDS_OFF) {
                self->engine->allNotesOff();
                self->currentNote = -1;
            }
            break;
        default:
            break;
    }
}

} // namespace flues::bubbles

extern "C" {

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                              const char*, const LV2_Feature* const* features) {
    using namespace flues::bubbles;

    auto* self = new BubblesLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<BubblesEngine>(self->sampleRate);

    self->audioOutLeft = nullptr;
    self->audioOutRight = nullptr;
    self->midiIn = nullptr;

    self->intensity = nullptr;
    self->density = nullptr;
    self->size = nullptr;
    self->flowRate = nullptr;
    self->brightness = nullptr;
    self->resonance = nullptr;
    self->depth = nullptr;
    self->space = nullptr;
    self->randomness = nullptr;
    self->heat = nullptr;
    self->output = nullptr;
    self->drive = nullptr;
    self->mode = nullptr;
    self->noiseFloor = nullptr;

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
    auto* self = static_cast<flues::bubbles::BubblesLV2*>(instance);
    delete self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    using namespace flues::bubbles;

    auto* self = static_cast<BubblesLV2*>(instance);
    switch (port) {
        case PORT_AUDIO_OUT_L: self->audioOutLeft = static_cast<float*>(data); break;
        case PORT_AUDIO_OUT_R: self->audioOutRight = static_cast<float*>(data); break;
        case PORT_MIDI_IN: self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_INTENSITY: self->intensity = static_cast<const float*>(data); break;
        case PORT_DENSITY: self->density = static_cast<const float*>(data); break;
        case PORT_SIZE: self->size = static_cast<const float*>(data); break;
        case PORT_FLOW_RATE: self->flowRate = static_cast<const float*>(data); break;
        case PORT_BRIGHTNESS: self->brightness = static_cast<const float*>(data); break;
        case PORT_RESONANCE: self->resonance = static_cast<const float*>(data); break;
        case PORT_DEPTH: self->depth = static_cast<const float*>(data); break;
        case PORT_SPACE: self->space = static_cast<const float*>(data); break;
        case PORT_RANDOMNESS: self->randomness = static_cast<const float*>(data); break;
        case PORT_HEAT: self->heat = static_cast<const float*>(data); break;
        case PORT_OUTPUT: self->output = static_cast<const float*>(data); break;
        case PORT_DRIVE: self->drive = static_cast<const float*>(data); break;
        case PORT_MODE: self->mode = static_cast<const float*>(data); break;
        case PORT_NOISE_FLOOR: self->noiseFloor = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<flues::bubbles::BubblesLV2*>(instance);
    if (!self) {
        return;
    }
    self->engine = std::make_unique<flues::bubbles::BubblesEngine>(self->sampleRate);
    self->currentNote = -1;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::bubbles;

    auto* self = static_cast<BubblesLV2*>(instance);
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

            if (ev->body.type == self->midiEventUrid) {
                const uint8_t* msg = reinterpret_cast<const uint8_t*>(ev + 1);
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

static void deactivate(LV2_Handle) {}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    BUBBLES_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

} // extern "C"
