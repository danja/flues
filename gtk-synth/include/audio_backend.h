// audio_backend.h
// Audio backend interface for GTK synthesizer
// Supports PulseAudio and JACK

#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    AUDIO_BACKEND_PULSEAUDIO,
    AUDIO_BACKEND_JACK,
    AUDIO_BACKEND_ALSA
} AudioBackendType;

typedef struct AudioBackend AudioBackend;

// Callback function type for audio processing
// Called by the audio backend when it needs more samples
typedef void (*AudioProcessCallback)(float* output, int num_samples, void* user_data);

// Create and initialize audio backend
AudioBackend* audio_backend_create(AudioBackendType type,
                                   int sample_rate,
                                   int buffer_size,
                                   AudioProcessCallback callback,
                                   void* user_data);

// Destroy audio backend
void audio_backend_destroy(AudioBackend* backend);

// Start/stop audio processing
bool audio_backend_start(AudioBackend* backend);
void audio_backend_stop(AudioBackend* backend);

// Query backend status
bool audio_backend_is_running(AudioBackend* backend);
float audio_backend_get_sample_rate(AudioBackend* backend);
int audio_backend_get_buffer_size(AudioBackend* backend);
const char* audio_backend_get_name(AudioBackend* backend);

#endif // AUDIO_BACKEND_H
