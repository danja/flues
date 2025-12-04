// ALSA Audio Backend - Direct ALSA PCM output
// Replaces PulseAudio for lower latency on Raspberry Pi

#include "audio_backend_alsa.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

struct AudioBackendALSA {
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* hw_params;

    float sample_rate;
    int buffer_size;     // Frames per period
    int periods;         // Number of periods

    float* audio_buffer;  // Processing buffer

    AudioProcessCallback callback;
    void* callback_user_data;

    pthread_t audio_thread;
    bool running;
};

// Audio thread function
static void* audio_thread_func(void* arg) {
    AudioBackendALSA* backend = (AudioBackendALSA*)arg;

    while (backend->running) {
        // Generate audio via callback
        backend->callback(backend->audio_buffer, backend->buffer_size,
                         backend->callback_user_data);

        // Write to ALSA
        int frames_written = snd_pcm_writei(backend->pcm_handle,
                                           backend->audio_buffer,
                                           backend->buffer_size);

        if (frames_written < 0) {
            // Handle underrun (EPIPE = -32)
            if (frames_written == -EPIPE) {
                fprintf(stderr, "ALSA: underrun occurred, recovering\n");
                snd_pcm_prepare(backend->pcm_handle);
            } else {
                fprintf(stderr, "ALSA: write error %d: %s\n",
                        frames_written, snd_strerror(frames_written));
                snd_pcm_prepare(backend->pcm_handle);
            }
        } else if (frames_written != backend->buffer_size) {
            fprintf(stderr, "ALSA: short write (expected %d, wrote %d)\n",
                    backend->buffer_size, frames_written);
        }
    }

    return NULL;
}

AudioBackendALSA* audio_backend_alsa_create(const char* device_name,
                                             float sample_rate,
                                             int buffer_size,
                                             AudioProcessCallback callback,
                                             void* user_data) {
    AudioBackendALSA* backend = (AudioBackendALSA*)calloc(1, sizeof(AudioBackendALSA));
    if (!backend) {
        fprintf(stderr, "ALSA: Failed to allocate backend\n");
        return NULL;
    }

    backend->sample_rate = sample_rate;
    backend->buffer_size = buffer_size;
    backend->periods = 2;
    backend->callback = callback;
    backend->callback_user_data = user_data;
    backend->running = false;

    // Open PCM device
    int err = snd_pcm_open(&backend->pcm_handle, device_name,
                           SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "ALSA: Cannot open audio device %s: %s\n",
                device_name, snd_strerror(err));
        free(backend);
        return NULL;
    }

    // Allocate HW params
    snd_pcm_hw_params_malloc(&backend->hw_params);
    snd_pcm_hw_params_any(backend->pcm_handle, backend->hw_params);

    // Set parameters
    snd_pcm_hw_params_set_access(backend->pcm_handle, backend->hw_params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(backend->pcm_handle, backend->hw_params,
                                  SND_PCM_FORMAT_FLOAT_LE);  // 32-bit float

    snd_pcm_hw_params_set_channels(backend->pcm_handle, backend->hw_params, 1);  // Mono

    unsigned int rate = (unsigned int)sample_rate;
    snd_pcm_hw_params_set_rate_near(backend->pcm_handle, backend->hw_params, &rate, 0);
    backend->sample_rate = (float)rate;

    snd_pcm_uframes_t period_size = buffer_size;
    snd_pcm_hw_params_set_period_size_near(backend->pcm_handle, backend->hw_params,
                                            &period_size, 0);
    backend->buffer_size = (int)period_size;

    snd_pcm_hw_params_set_periods(backend->pcm_handle, backend->hw_params,
                                   backend->periods, 0);

    // Apply params
    err = snd_pcm_hw_params(backend->pcm_handle, backend->hw_params);
    if (err < 0) {
        fprintf(stderr, "ALSA: Cannot set parameters: %s\n", snd_strerror(err));
        snd_pcm_close(backend->pcm_handle);
        snd_pcm_hw_params_free(backend->hw_params);
        free(backend);
        return NULL;
    }

    // Allocate audio buffer
    backend->audio_buffer = (float*)malloc(backend->buffer_size * sizeof(float));
    if (!backend->audio_buffer) {
        fprintf(stderr, "ALSA: Failed to allocate audio buffer\n");
        snd_pcm_close(backend->pcm_handle);
        snd_pcm_hw_params_free(backend->hw_params);
        free(backend);
        return NULL;
    }

    printf("ALSA: Initialized %s at %.0f Hz, buffer size %d frames (%.1f ms)\n",
           device_name, backend->sample_rate, backend->buffer_size,
           (backend->buffer_size * 1000.0f) / backend->sample_rate);

    return backend;
}

void audio_backend_alsa_destroy(AudioBackendALSA* backend) {
    if (!backend) return;

    if (backend->running) {
        audio_backend_alsa_stop(backend);
    }

    if (backend->pcm_handle) {
        snd_pcm_close(backend->pcm_handle);
    }

    if (backend->hw_params) {
        snd_pcm_hw_params_free(backend->hw_params);
    }

    free(backend->audio_buffer);
    free(backend);
}

bool audio_backend_alsa_start(AudioBackendALSA* backend) {
    if (!backend || backend->running) {
        return false;
    }

    // Prepare PCM
    int err = snd_pcm_prepare(backend->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "ALSA: Cannot prepare audio interface: %s\n",
                snd_strerror(err));
        return false;
    }

    backend->running = true;

    // Create audio thread
    if (pthread_create(&backend->audio_thread, NULL, audio_thread_func, backend) != 0) {
        fprintf(stderr, "ALSA: Failed to create audio thread\n");
        backend->running = false;
        return false;
    }

    printf("ALSA: Audio thread started\n");
    return true;
}

void audio_backend_alsa_stop(AudioBackendALSA* backend) {
    if (!backend || !backend->running) {
        return;
    }

    backend->running = false;

    // Wait for thread to finish
    pthread_join(backend->audio_thread, NULL);

    // Drop any remaining samples
    snd_pcm_drop(backend->pcm_handle);

    printf("ALSA: Audio thread stopped\n");
}

float audio_backend_alsa_get_sample_rate(AudioBackendALSA* backend) {
    return backend ? backend->sample_rate : 0.0f;
}

int audio_backend_alsa_get_buffer_size(AudioBackendALSA* backend) {
    return backend ? backend->buffer_size : 0;
}
