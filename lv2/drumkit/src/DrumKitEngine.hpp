// DrumKitEngine.hpp
// Main drum kit coordinator managing 9 voices and master FX chain
// Handles MIDI channel 10 routing, voice management, and signal processing

#ifndef DRUMKIT_ENGINE_HPP
#define DRUMKIT_ENGINE_HPP

#include "voices/KickVoice.hpp"
#include "voices/SnareVoice.hpp"
#include "voices/ClapVoice.hpp"
#include "voices/TomVoice.hpp"
#include "voices/HiHatVoice.hpp"
#include "voices/CrashVoice.hpp"
#include "voices/BashVoice.hpp"
#include "modules/Bitcrusher.hpp"
#include "modules/Distortion.hpp"
#include "modules/ReverbModule.hpp"
#include "modules/DCBlocker.hpp"
#include <memory>
#include <cmath>

namespace flues::drumkit {

class DrumKitEngine {
private:
    float sampleRate;

    // 9 Voice instances
    std::unique_ptr<KickVoice> kick;
    std::unique_ptr<SnareVoice> snare;
    std::unique_ptr<ClapVoice> clap;
    std::unique_ptr<TomVoice> loTom;
    std::unique_ptr<TomVoice> hiTom;
    std::unique_ptr<HiHatVoice> closedHH;
    std::unique_ptr<HiHatVoice> openHH;
    std::unique_ptr<CrashVoice> crash;
    std::unique_ptr<BashVoice> bash;

    // Master FX
    Bitcrusher bitcrusher;
    Distortion distortion;
    flues::pm::ReverbModule reverb;  // From pm-synth namespace
    DCBlocker dcBlocker;

    // Master parameters
    float masterGain;

public:
    explicit DrumKitEngine(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , kick(std::make_unique<KickVoice>(sampleRate))
        , snare(std::make_unique<SnareVoice>(sampleRate))
        , clap(std::make_unique<ClapVoice>(sampleRate))
        , loTom(std::make_unique<TomVoice>(sampleRate, 80.0f))    // Lo Tom base freq
        , hiTom(std::make_unique<TomVoice>(sampleRate, 200.0f))   // Hi Tom base freq
        , closedHH(std::make_unique<HiHatVoice>(sampleRate, true))   // Closed
        , openHH(std::make_unique<HiHatVoice>(sampleRate, false))    // Open
        , crash(std::make_unique<CrashVoice>(sampleRate))
        , bash(std::make_unique<BashVoice>(sampleRate))
        , bitcrusher()
        , distortion()
        , reverb(sampleRate)
        , dcBlocker(0.999f)
        , masterGain(0.7f)
    {
        // Initialize reverb
        reverb.setSize(0.6f);      // Fixed room size
        reverb.setLevel(0.2f);     // 20% wet by default
    }

    /**
     * Handle MIDI note on (channel 10 only)
     * @param note - MIDI note number (36-56)
     * @param velocity - MIDI velocity (0-127)
     */
    void handleNoteOn(uint8_t note, uint8_t velocity) {
        const float vel = velocity / 127.0f;

        switch (note) {
            case 36:  // C2 - Kick
                kick->trigger(vel);
                break;

            case 38:  // D2 - Snare
                snare->trigger(vel);
                break;

            case 39:  // Eb2 - Clap
                clap->trigger(1.0f);  // Fixed level
                break;

            case 45:  // A2 - Lo Tom
                loTom->trigger(vel);
                break;

            case 50:  // D3 - Hi Tom
                hiTom->trigger(vel);
                break;

            case 42:  // F#2 - Closed Hi-Hat
                openHH->kill();       // Choke open HH
                closedHH->trigger(1.0f);  // Fixed level
                break;

            case 46:  // A#2 - Open Hi-Hat
                openHH->trigger(1.0f);  // Fixed level
                break;

            case 49:  // C#3 - Crash
                crash->trigger(1.0f);  // Fixed level
                break;

            case 47:  // B2 - Bash (metal strike)
                bash->trigger(vel);
                break;

            default:
                // Ignore unsupported notes
                break;
        }
    }

    /**
     * Process one sample through all voices and master FX
     */
    float process() {
        // Sum all voice outputs
        float sum = 0.0f;
        sum += kick->process();
        sum += snare->process();
        sum += clap->process();
        sum += loTom->process();
        sum += hiTom->process();
        sum += closedHH->process();
        sum += openHH->process();
        sum += crash->process();
        sum += bash->process();

        // Master FX chain
        sum = bitcrusher.process(sum);
        sum = distortion.process(sum);
        sum = reverb.process(sum);
        sum = dcBlocker.process(sum);

        // Master gain
        sum *= masterGain;

        return std::clamp(sum, -1.0f, 1.0f);
    }

    // === Parameter Setters ===

    // Kick (4 params)
    void setKickPitch(float value) { kick->setPitch(value); }
    void setKickDecay(float value) { kick->setDecay(value); }
    void setKickDrive(float value) { kick->setDrive(value); }
    void setKickPunch(float value) { kick->setPunch(value); }

    // Snare (2 params)
    void setSnareTone(float value) { snare->setTone(value); }
    void setSnareSnap(float value) { snare->setSnap(value); }

    // Clap (2 params)
    void setClapDensity(float value) { clap->setDensity(value); }
    void setClapTone(float value) { clap->setTone(value); }

    // Toms (2 params, shared)
    void setTomPitch(float value) {
        loTom->setPitch(value);
        hiTom->setPitch(value);
    }

    void setTomDecay(float value) {
        loTom->setDecay(value);
        hiTom->setDecay(value);
    }

    // Hi-Hats (2 params, shared)
    void setHHBrightness(float value) {
        closedHH->setBrightness(value);
        openHH->setBrightness(value);
    }

    void setHHDecay(float value) {
        closedHH->setDecay(value);
        openHH->setDecay(value);
    }

    // Crash (2 params)
    void setCrashBrightness(float value) { crash->setBrightness(value); }
    void setCrashDecay(float value) { crash->setDecay(value); }

    // Master (4 params)
    void setBitCrush(float value) { bitcrusher.setAmount(value); }

    void setMasterDrive(float value) {
        // Map 0-1 to 1.0-5.0 linear
        const float drive = 1.0f + value * 4.0f;
        distortion.setDrive(drive);
    }

    void setMasterReverb(float value) {
        // Map 0-1 to 0-60% wet level
        const float level = value * 0.6f;
        reverb.setLevel(level);
    }

    void setMasterGain(float value) {
        masterGain = std::clamp(value, 0.0f, 1.0f);
    }

    // Bash (6 params)
    void setBashSize(float value) { bash->setSize(value); }
    void setBashSpread(float value) { bash->setSpread(value); }
    void setBashDecay(float value) { bash->setDecay(value); }
    void setBashDrive(float value) { bash->setDrive(value); }
    void setBashNoise(float value) { bash->setNoise(value); }
    void setBashEdge(float value) { bash->setEdge(value); }

    /**
     * Reset all voices (for All Notes Off / panic)
     */
    void reset() {
        kick->reset();
        snare->reset();
        clap->reset();
        loTom->reset();
        hiTom->reset();
        closedHH->reset();
        openHH->reset();
        crash->reset();
        bash->reset();

        reverb.reset();
        dcBlocker.reset();
    }
};

} // namespace flues::drumkit

#endif // DRUMKIT_ENGINE_HPP
