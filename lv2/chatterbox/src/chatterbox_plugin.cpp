// chatterbox_plugin.cpp
// LV2 plugin wrapper for Chatterbox speech synthesizer

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <algorithm>

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include "ChatterboxEngine.hpp"

#define CHATTERBOX_URI "https://danja.github.io/flues/plugins/chatterbox"

namespace flues::chatterbox {

enum PortIndex : uint32_t {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_PITCH,
    PORT_VOICED,
    PORT_ASPIRATED,
    PORT_NOISE_LEVEL,
    PORT_F1,
    PORT_F2,
    PORT_F3,
    PORT_F4,
    PORT_NASAL,
    PORT_SING,
    PORT_SHOUT,
    PORT_FRY,
    PORT_STRESS,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
};

struct ChatterboxLV2 {
    std::unique_ptr<ChatterboxEngine> engine;
    float sampleRate;

    // Audio/MIDI ports
    const LV2_Atom_Sequence* midiIn;
    float* audioOut;

    // Control ports
    const float* pitch;
    const float* voiced;
    const float* aspirated;
    const float* noiseLevel;
    const float* f1;
    const float* f2;
    const float* f3;
    const float* f4;
    const float* nasal;
    const float* sing;
    const float* shout;
    const float* fry;
    const float* stress;
    const float* attack;
    const float* release;
    const float* reverbSize;
    const float* reverbLevel;
    const float* masterGain;

    // URID mapping
    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;

    int currentNote;
};

/**
 * Map normalized pitch parameter to frequency in Hz
 * Range: 80-400 Hz (exponential)
 */
static float mapPitchHz(float normalized) {
    const float min = 80.0f;
    const float max = 400.0f;
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    return min * std::pow(max / min, clamped);
}

/**
 * Map normalized formant parameter to frequency in Hz
 */
static float mapFormantHz(float normalized, float min, float max) {
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    return min * std::pow(max / min, clamped);
}

/**
 * Apply all control port parameters to the engine
 */
static void apply_parameters(ChatterboxLV2* self) {
    if (!self->engine) {
        return;
    }

    // Source parameters
    // Only apply pitch control when no MIDI note is active
    if (self->pitch && self->currentNote < 0) {
        self->engine->setPitch(mapPitchHz(*self->pitch));
    }
    // Only apply voiced control when no MIDI note is active
    // (MIDI noteOn/noteOff controls voiced mode)
    if (self->voiced && self->currentNote < 0) {
        self->engine->setVoiced(*self->voiced > 0.5f);
    }
    if (self->aspirated) {
        self->engine->setAspirated(*self->aspirated > 0.5f);
    }
    if (self->noiseLevel) {
        self->engine->setNoiseLevel(*self->noiseLevel);
    }

    // Formants (F1-F4) - only update if control port value changed
    // F1: 200-1000 Hz (jaw opening)
    // F2: 500-3000 Hz (tongue front/back)
    // F3: 1500-4000 Hz (lip rounding)
    // F4: 2500-4500 Hz (voice quality)
    static float lastF1 = -1.0f;
    static float lastF2 = -1.0f;
    static float lastF3 = -1.0f;
    static float lastF4 = -1.0f;

    if (self->f1 && *self->f1 != lastF1) {
        const float freq = mapFormantHz(*self->f1, 200.0f, 1000.0f);
        self->engine->setFormant(0, freq, 80.0f);
        lastF1 = *self->f1;
    }
    if (self->f2 && *self->f2 != lastF2) {
        const float freq = mapFormantHz(*self->f2, 500.0f, 3000.0f);
        self->engine->setFormant(1, freq, 120.0f);
        lastF2 = *self->f2;
    }
    if (self->f3 && *self->f3 != lastF3) {
        const float freq = mapFormantHz(*self->f3, 1500.0f, 4000.0f);
        self->engine->setFormant(2, freq, 150.0f);
        lastF3 = *self->f3;
    }
    if (self->f4 && *self->f4 != lastF4) {
        const float freq = mapFormantHz(*self->f4, 2500.0f, 4500.0f);
        self->engine->setFormant(3, freq, 200.0f);
        lastF4 = *self->f4;
    }

    // Vocal modes
    if (self->nasal) {
        self->engine->setNasal(*self->nasal > 0.5f);
    }
    if (self->sing) {
        self->engine->setSing(*self->sing > 0.5f);
    }
    if (self->shout) {
        self->engine->setShout(*self->shout > 0.5f);
    }
    if (self->fry) {
        self->engine->setFry(*self->fry > 0.5f);
    }

    // Dynamics
    if (self->stress) {
        self->engine->setStress(*self->stress);
    }

    // Envelope
    if (self->attack && self->release) {
        self->engine->setEnvelope(*self->attack, *self->release);
    }

    // Reverb
    if (self->reverbSize) {
        self->engine->setReverbSize(*self->reverbSize);
    }
    if (self->reverbLevel) {
        self->engine->setReverbLevel(*self->reverbLevel);
    }

    // Master
    if (self->masterGain) {
        self->engine->setMasterGain(*self->masterGain);
    }
}

/**
 * Handle incoming MIDI messages
 */
static void handle_midi(ChatterboxLV2* self, const uint8_t* msg, uint32_t size) {
    if (size < 1 || !self->engine) {
        return;
    }

    const uint8_t status = msg[0] & 0xF0U;
    const uint8_t data1 = size > 1 ? msg[1] : 0;
    const uint8_t data2 = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (data2 == 0) {
                // Note on with velocity 0 is note off
                if (self->currentNote == data1) {
                    self->engine->noteOff(data1);
                    self->currentNote = -1;
                }
                break;
            }
            // Convert MIDI note to frequency
            const float freq = 440.0f * std::pow(2.0f, (static_cast<int>(data1) - 69) / 12.0f);
            const float velocity = static_cast<float>(data2) / 127.0f;
            self->engine->noteOn(data1, freq, velocity);
            self->currentNote = data1;
            break;
        }
        case LV2_MIDI_MSG_NOTE_OFF: {
            if (self->currentNote == data1) {
                self->engine->noteOff(data1);
                self->currentNote = -1;
            }
            break;
        }
        case LV2_MIDI_MSG_CONTROLLER: {
            if (data1 == LV2_MIDI_CTL_ALL_SOUNDS_OFF || data1 == LV2_MIDI_CTL_ALL_NOTES_OFF) {
                self->engine->allNotesOff();
                self->currentNote = -1;
                break;
            }

            // Map MIDI CCs to parameters
            const float ccValue = static_cast<float>(data2) / 127.0f;

            switch (data1) {
                // CC 1 (Mod Wheel) -> Stress
                case 1:
                    self->engine->setStress(ccValue);
                    break;

                // CC 7 (Volume) -> Master Gain
                case 7:
                    self->engine->setMasterGain(ccValue);
                    break;

                // CC 10 (Pan) -> F2 (Tongue position)
                case 10:
                    {
                        const float f2 = mapFormantHz(ccValue, 500.0f, 3000.0f);
                        self->engine->setFormant(1, f2, 120.0f);
                    }
                    break;

                // CC 71 (Resonance) -> F1 (Jaw opening)
                case 71:
                    {
                        const float f1 = mapFormantHz(ccValue, 200.0f, 1000.0f);
                        self->engine->setFormant(0, f1, 80.0f);
                    }
                    break;

                // CC 72 (Release) -> Envelope Release
                case 72:
                    self->engine->setEnvelope(
                        self->attack ? *self->attack : 0.33f,
                        ccValue
                    );
                    break;

                // CC 73 (Attack) -> Envelope Attack
                case 73:
                    self->engine->setEnvelope(
                        ccValue,
                        self->release ? *self->release : 0.33f
                    );
                    break;

                // CC 74 (Brightness) -> F3 (Lips)
                case 74:
                    {
                        const float f3 = mapFormantHz(ccValue, 1500.0f, 4000.0f);
                        self->engine->setFormant(2, f3, 150.0f);
                    }
                    break;

                // CC 75 (Decay) -> F4 (Quality)
                case 75:
                    {
                        const float f4 = mapFormantHz(ccValue, 2500.0f, 4500.0f);
                        self->engine->setFormant(3, f4, 200.0f);
                    }
                    break;

                // CC 80 -> Nasal (toggle on >= 64)
                case 80:
                    self->engine->setNasal(data2 >= 64);
                    break;

                // CC 81 -> Sing/Vibrato (toggle on >= 64)
                case 81:
                    self->engine->setSing(data2 >= 64);
                    break;

                // CC 82 -> Shout (toggle on >= 64)
                case 82:
                    self->engine->setShout(data2 >= 64);
                    break;

                // CC 83 -> Fry (toggle on >= 64)
                case 83:
                    self->engine->setFry(data2 >= 64);
                    break;

                // CC 84 -> Reverb Size
                case 84:
                    self->engine->setReverbSize(ccValue);
                    break;

                // CC 85 -> Reverb Level
                case 85:
                    self->engine->setReverbLevel(ccValue);
                    break;

                // CC 91 (Effects Level) -> Reverb Level (alternative)
                case 91:
                    self->engine->setReverbLevel(ccValue);
                    break;

                // CC 102 -> Noise Level
                case 102:
                    self->engine->setNoiseLevel(ccValue);
                    break;

                // CC 103 -> Aspirated (toggle on >= 64)
                case 103:
                    self->engine->setAspirated(data2 >= 64);
                    break;

                // CC 104 -> Voiced (toggle on >= 64)
                case 104:
                    self->engine->setVoiced(data2 >= 64);
                    break;

                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

} // namespace flues::chatterbox

extern "C" {

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                              const char*, const LV2_Feature* const* features) {
    using namespace flues::chatterbox;

    auto* self = new ChatterboxLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<ChatterboxEngine>(self->sampleRate);
    self->midiIn = nullptr;
    self->audioOut = nullptr;

    // Initialize all control ports to nullptr
    self->pitch = nullptr;
    self->voiced = nullptr;
    self->aspirated = nullptr;
    self->noiseLevel = nullptr;
    self->f1 = nullptr;
    self->f2 = nullptr;
    self->f3 = nullptr;
    self->f4 = nullptr;
    self->nasal = nullptr;
    self->sing = nullptr;
    self->shout = nullptr;
    self->fry = nullptr;
    self->stress = nullptr;
    self->attack = nullptr;
    self->release = nullptr;
    self->reverbSize = nullptr;
    self->reverbLevel = nullptr;
    self->masterGain = nullptr;

    self->map = nullptr;
    self->midiEventUrid = 0;
    self->atomSequenceUrid = 0;
    self->currentNote = -1;

    // Extract required features
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>((*f)->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    // Map URIDs
    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);

    return self;
}

static void cleanup(LV2_Handle instance) {
    auto* self = static_cast<flues::chatterbox::ChatterboxLV2*>(instance);
    delete self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    using namespace flues::chatterbox;
    auto* self = static_cast<ChatterboxLV2*>(instance);

    switch (port) {
        case PORT_AUDIO_OUT:    self->audioOut = static_cast<float*>(data); break;
        case PORT_MIDI_IN:      self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_PITCH:        self->pitch = static_cast<const float*>(data); break;
        case PORT_VOICED:       self->voiced = static_cast<const float*>(data); break;
        case PORT_ASPIRATED:    self->aspirated = static_cast<const float*>(data); break;
        case PORT_NOISE_LEVEL:  self->noiseLevel = static_cast<const float*>(data); break;
        case PORT_F1:           self->f1 = static_cast<const float*>(data); break;
        case PORT_F2:           self->f2 = static_cast<const float*>(data); break;
        case PORT_F3:           self->f3 = static_cast<const float*>(data); break;
        case PORT_F4:           self->f4 = static_cast<const float*>(data); break;
        case PORT_NASAL:        self->nasal = static_cast<const float*>(data); break;
        case PORT_SING:         self->sing = static_cast<const float*>(data); break;
        case PORT_SHOUT:        self->shout = static_cast<const float*>(data); break;
        case PORT_FRY:          self->fry = static_cast<const float*>(data); break;
        case PORT_STRESS:       self->stress = static_cast<const float*>(data); break;
        case PORT_ATTACK:       self->attack = static_cast<const float*>(data); break;
        case PORT_RELEASE:      self->release = static_cast<const float*>(data); break;
        case PORT_REVERB_SIZE:  self->reverbSize = static_cast<const float*>(data); break;
        case PORT_REVERB_LEVEL: self->reverbLevel = static_cast<const float*>(data); break;
        case PORT_MASTER_GAIN:  self->masterGain = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<flues::chatterbox::ChatterboxLV2*>(instance);
    if (!self) {
        return;
    }
    // Recreate engine on activate
    self->engine = std::make_unique<ChatterboxEngine>(self->sampleRate);
    self->currentNote = -1;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::chatterbox;
    auto* self = static_cast<ChatterboxLV2*>(instance);
    if (!self || !self->audioOut) {
        return;
    }

    // Apply all control port parameters
    apply_parameters(self);

    float* out = self->audioOut;
    std::memset(out, 0, n_samples * sizeof(float));

    uint32_t frame = 0;

    // Process MIDI events with sample-accurate timing
    if (self->midiIn && self->midiIn->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midiIn, ev) {
            const uint32_t eventFrame = ev->time.frames >= 0
                ? static_cast<uint32_t>(ev->time.frames)
                : 0u;

            // Render audio up to the event
            if (frame < eventFrame) {
                const uint32_t limit = std::min(eventFrame, n_samples);
                self->engine->process(&out[frame], limit - frame);
                frame = limit;
            }

            // Handle MIDI event
            if (ev->body.type == self->midiEventUrid) {
                const uint8_t* msg = reinterpret_cast<const uint8_t*>(ev + 1);
                handle_midi(self, msg, ev->body.size);
            }
        }
    }

    // Render remaining audio
    if (frame < n_samples) {
        self->engine->process(&out[frame], n_samples - frame);
    }
}

static void deactivate(LV2_Handle) {}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    CHATTERBOX_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

} // extern "C"
