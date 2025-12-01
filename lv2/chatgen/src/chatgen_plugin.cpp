// chatgen_plugin.cpp - LV2 plugin wrapper for ChatGen
// Text to MIDI CC generator for Chatterbox speech synthesis control

#include "ChatGenEngine.hpp"

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>

#include <cstdlib>
#include <cstring>

#define CHATGEN_URI "https://danja.github.io/flues/plugins/chatgen"

// Port indices (must match chatgen.ttl)
enum PortIndex {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT = 1,
    PORT_PLAY = 2,
    PORT_LOOP = 3,
    PORT_TEXT_OUT = 4
};

// Plugin instance data
typedef struct {
    // Ports
    const LV2_Atom_Sequence* control;
    LV2_Atom_Sequence* midiOut;
    const float* playPort;
    const float* loopPort;
    LV2_Atom_Sequence* textOut;

    // Features
    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;
    LV2_URID timePositionUrid;
    LV2_URID timeBeatsPerMinuteUrid;
    LV2_URID timeSpeedUrid;
    LV2_URID atomFloatUrid;
    LV2_URID atomStringUrid;

    // Atom forge
    LV2_Atom_Forge forge;

    // Engine
    chatgen::ChatGenEngine* engine;

    // Sample rate
    double sampleRate;

    // Tempo from host (0 = use BPM port)
    float hostBPM;

} ChatGenLV2;

// LV2 instantiate callback
static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                              double rate,
                              const char* bundle_path,
                              const LV2_Feature* const* features) {
    (void)descriptor;
    (void)bundle_path;

    ChatGenLV2* self = new ChatGenLV2;
    if (!self) {
        return nullptr;
    }

    // Initialize ports to null
    self->control = nullptr;
    self->midiOut = nullptr;
    self->playPort = nullptr;
    self->loopPort = nullptr;
    self->textOut = nullptr;

    // Get required features
    self->map = nullptr;
    for (int i = 0; features[i]; i++) {
        if (std::strcmp(features[i]->URI, LV2_URID__map) == 0) {
            self->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    // Map URIDs
    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->timePositionUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#Position");
    self->timeBeatsPerMinuteUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#beatsPerMinute");
    self->timeSpeedUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#speed");
    self->atomFloatUrid = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->atomStringUrid = self->map->map(self->map->handle, LV2_ATOM__String);

    // Initialize atom forge
    lv2_atom_forge_init(&self->forge, self->map);

    // Initialize host state
    self->hostBPM = 0.0f;

    // Create engine
    self->engine = new chatgen::ChatGenEngine();
    self->sampleRate = rate;

    // Initialize engine with sample rate and URIDs
    self->engine->init(rate, self->midiEventUrid, self->atomSequenceUrid);

    return static_cast<LV2_Handle>(self);
}

// LV2 connect_port callback
static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    ChatGenLV2* self = static_cast<ChatGenLV2*>(instance);

    switch (port) {
        case PORT_CONTROL:
            self->control = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_MIDI_OUT:
            self->midiOut = static_cast<LV2_Atom_Sequence*>(data);
            break;
        case PORT_PLAY:
            self->playPort = static_cast<const float*>(data);
            break;
        case PORT_LOOP:
            self->loopPort = static_cast<const float*>(data);
            break;
        case PORT_TEXT_OUT:
            self->textOut = static_cast<LV2_Atom_Sequence*>(data);
            break;
    }
}

// LV2 activate callback
static void activate(LV2_Handle instance) {
    ChatGenLV2* self = static_cast<ChatGenLV2*>(instance);
    self->engine->reset();
}

// LV2 run callback - main processing function
static void run(LV2_Handle instance, uint32_t nframes) {
    ChatGenLV2* self = static_cast<ChatGenLV2*>(instance);

    // Initialize output sequences
    if (!self->midiOut) return;
    const uint32_t capacity = 8192;

    self->midiOut->atom.type = self->atomSequenceUrid;
    self->midiOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midiOut->body.unit = 0;
    self->midiOut->body.pad = 0;

    if (self->textOut) {
        self->textOut->atom.type = self->atomSequenceUrid;
        self->textOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
        self->textOut->body.unit = 0;
        self->textOut->body.pad = 0;
    }

    // Parse control port for time position and MIDI events
    self->hostBPM = 0.0f;
    float transportSpeed = 0.0f;
    bool transportPlaying = false;

    // Debug control port contents
    if (!self->control) {
        static int noControlCount = 0;
        if (noControlCount++ % 1000 == 0) {
            fprintf(stderr, "ChatGen ERROR: No control port!\n");
        }
        return;
    }

    if (self->control->atom.type != self->atomSequenceUrid) {
        static int wrongTypeCount = 0;
        if (wrongTypeCount++ % 1000 == 0) {
            fprintf(stderr, "ChatGen ERROR: Control port wrong type (expected %u, got %u)\n",
                    self->atomSequenceUrid, self->control->atom.type);
        }
        return;
    }

    // Count events in control port
    uint32_t eventCount = 0;
    uint32_t midiEventCount = 0;
    LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
        eventCount++;
        if (ev->body.type == self->midiEventUrid) {
            midiEventCount++;
        }
    }

    static int frameCount = 0;
    if (frameCount++ % 100 == 0) {
        fprintf(stderr, "ChatGen: Frame %d - control port has %u events (%u MIDI)\n",
                frameCount, eventCount, midiEventCount);
    }

    if (self->control && self->control->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
            // Handle time position
            if (ev->body.type == self->timePositionUrid) {
                const LV2_Atom_Object* obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);

                // Read BPM
                const LV2_Atom_Float* bpm = nullptr;
                lv2_atom_object_get(obj, self->timeBeatsPerMinuteUrid, &bpm, 0);
                if (bpm && bpm->atom.type == self->atomFloatUrid) {
                    self->hostBPM = bpm->body;
                }

                // Read transport speed (0.0 = stopped, 1.0 = playing)
                const LV2_Atom_Float* speed = nullptr;
                lv2_atom_object_get(obj, self->timeSpeedUrid, &speed, 0);
                if (speed && speed->atom.type == self->atomFloatUrid) {
                    transportSpeed = speed->body;
                    transportPlaying = (transportSpeed > 0.0f);
                }
            }
            // Handle MIDI events - pass through to output
            else if (ev->body.type == self->midiEventUrid) {
                const uint8_t* midiMsg = reinterpret_cast<const uint8_t*>(ev + 1);

                // Debug MIDI pass-through - log ALL MIDI
                const uint8_t status = midiMsg[0] & 0xF0;
                const uint8_t note = ev->body.size > 1 ? midiMsg[1] : 0;
                const uint8_t velocity = ev->body.size > 2 ? midiMsg[2] : 0;

                if (status == 0x90) {  // Note on
                    if (velocity > 0) {
                        fprintf(stderr, "ChatGen IN: Note ON  %3u vel %3u → passing through\n", note, velocity);
                    } else {
                        fprintf(stderr, "ChatGen IN: Note OFF %3u (vel=0) → passing through\n", note);
                    }
                } else if (status == 0x80) {  // Note off
                    fprintf(stderr, "ChatGen IN: Note OFF %3u vel %3u → passing through\n", note, velocity);
                } else if (status == 0xB0) {  // CC
                    fprintf(stderr, "ChatGen IN: CC %u = %u → passing through\n", note, velocity);
                } else {
                    fprintf(stderr, "ChatGen IN: Unknown MIDI 0x%02x → passing through\n", midiMsg[0]);
                }

                // Copy MIDI event to output
                LV2_Atom_Event outEv;
                outEv.time.frames = ev->time.frames;
                outEv.body.type = self->midiEventUrid;
                outEv.body.size = ev->body.size;

                LV2_Atom_Event* appendedEvent = lv2_atom_sequence_append_event(
                    self->midiOut, capacity, &outEv);

                if (appendedEvent) {
                    uint8_t* outMsg = reinterpret_cast<uint8_t*>(appendedEvent + 1);
                    std::memcpy(outMsg, midiMsg, ev->body.size);
                }
            }
            // Handle text string from UI
            else if (ev->body.type == self->atomStringUrid) {
                const LV2_Atom_String* str = reinterpret_cast<const LV2_Atom_String*>(&ev->body);
                const char* text = reinterpret_cast<const char*>(str + 1);
                self->engine->setText(std::string(text));

                // Debug: show phonemes
                const std::vector<chatgen::Phoneme>& phonemes = self->engine->getPhonemes();
                fprintf(stderr, "ChatGen DSP: Received text '%s' → %zu phonemes: ",
                        text, phonemes.size());
                for (size_t i = 0; i < std::min(phonemes.size(), size_t(10)); i++) {
                    fprintf(stderr, "[%s] ", chatgen::TextParser::getPhonemeName(phonemes[i]));
                }
                if (phonemes.size() > 10) fprintf(stderr, "...");
                fprintf(stderr, "\n");
            }
            // Unknown event type - debug what we're receiving
            else {
                static int unknownCount = 0;
                if (unknownCount++ < 10) {
                    fprintf(stderr, "ChatGen DSP: Unknown event type %u (string=%u, timePos=%u, midi=%u)\n",
                            ev->body.type, self->atomStringUrid, self->timePositionUrid, self->midiEventUrid);
                }
            }
        }
    }

    // Send current text to UI on EVERY callback (like float control ports)
    // The UI has strcmp protection to avoid unnecessary redraws
    // This ensures newly created UIs immediately get the current text
    if (self->textOut) {
        const std::string& currentText = self->engine->getText();
        if (!currentText.empty()) {
            LV2_Atom_Forge_Frame frame;
            lv2_atom_forge_set_buffer(&self->forge, (uint8_t*)self->textOut, capacity);
            lv2_atom_forge_sequence_head(&self->forge, &frame, 0);
            lv2_atom_forge_frame_time(&self->forge, 0);
            lv2_atom_forge_string(&self->forge, currentText.c_str(), currentText.length());
            lv2_atom_forge_pop(&self->forge, &frame);
        }
    }

    // Use host BPM if available, otherwise default to 120
    float effectiveBPM = self->hostBPM;
    if (effectiveBPM <= 0.0f) {
        effectiveBPM = 120.0f;
    }
    self->engine->setBPM(effectiveBPM);

    // Use transport state if available, otherwise use Play toggle
    bool shouldPlay = transportPlaying;
    if (!transportPlaying && self->playPort) {
        shouldPlay = *self->playPort > 0.5f;
    }

    static bool lastPlayState = false;
    if (shouldPlay != lastPlayState) {
        fprintf(stderr, "ChatGen: Transport %s, BPM: %.1f\n",
                shouldPlay ? "PLAYING" : "STOPPED", effectiveBPM);
        lastPlayState = shouldPlay;
    }

    self->engine->setPlaying(shouldPlay);

    if (self->loopPort) {
        self->engine->setLoop(*self->loopPort > 0.5f);
    }

    // Process formant CCs (engine adds them to the already-initialized midiOut)
    self->engine->process(self->midiOut, capacity, nframes);
}

// LV2 deactivate callback
static void deactivate(LV2_Handle instance) {
    ChatGenLV2* self = static_cast<ChatGenLV2*>(instance);
    self->engine->reset();
}

// LV2 cleanup callback
static void cleanup(LV2_Handle instance) {
    ChatGenLV2* self = static_cast<ChatGenLV2*>(instance);
    delete self->engine;
    delete self;
}

// LV2 extension_data callback
static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

// LV2 plugin descriptor
static const LV2_Descriptor descriptor = {
    CHATGEN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

// LV2 plugin entry point
extern "C" {
LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}
}
