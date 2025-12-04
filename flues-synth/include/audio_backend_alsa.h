#ifndef FLUES_SYNTH_AUDIO_BACKEND_ALSA_H
#define FLUES_SYNTH_AUDIO_BACKEND_ALSA_H

#include <stdbool.h>

// Audio process callback signature
// buffer: output buffer to fill with audio samples
// num_samples: number of samples to generate
// user_data: user-provided data pointer
typedef void (*AudioProcessCallback)(float* buffer, int num_samples, void* user_data);

// Opaque audio backend structure
typedef struct AudioBackendALSA AudioBackendALSA;

// Create ALSA audio backend
// device_name: ALSA device name (e.g., "default", "hw:0,0")
// sample_rate: sample rate in Hz (e.g., 48000)
// buffer_size: buffer size in frames (e.g., 256)
// callback: audio processing callback
// user_data: pointer passed to callback
AudioBackendALSA* audio_backend_alsa_create(const char* device_name,
                                             float sample_rate,
                                             int buffer_size,
                                             AudioProcessCallback callback,
                                             void* user_data);

// Destroy audio backend and release resources
void audio_backend_alsa_destroy(AudioBackendALSA* backend);

// Start audio processing thread
bool audio_backend_alsa_start(AudioBackendALSA* backend);

// Stop audio processing thread
void audio_backend_alsa_stop(AudioBackendALSA* backend);

// Get actual sample rate (may differ from requested)
float audio_backend_alsa_get_sample_rate(AudioBackendALSA* backend);

// Get actual buffer size (may differ from requested)
int audio_backend_alsa_get_buffer_size(AudioBackendALSA* backend);

#endif // FLUES_SYNTH_AUDIO_BACKEND_ALSA_H
