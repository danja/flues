#ifndef ACHORD_VOICING_ENGINE_H
#define ACHORD_VOICING_ENGINE_H

#include <stdint.h>

#define ACHORD_MAX_CHORD_NOTES 24

typedef struct {
    uint8_t tonic_note;
    int bank_offset;
    uint8_t scale_index;
    uint8_t row;
    uint8_t col;
    uint8_t register_mode;
    uint8_t bass_enabled;
    uint8_t add9_enabled;
    uint8_t sus_mode;
    int8_t inversion_offset;
    uint8_t spread_mode;
    uint8_t voice_lead_enabled;
} AchordVoicingRequest;

typedef struct {
    uint8_t note_count;
    uint8_t notes[ACHORD_MAX_CHORD_NOTES];
} AchordVoicing;

#ifdef __cplusplus
extern "C" {
#endif

void achord_build_voicing(const AchordVoicingRequest *request,
                          const uint8_t *previous_notes,
                          uint8_t previous_count,
                          AchordVoicing *out_voicing);

#ifdef __cplusplus
}
#endif

#endif
