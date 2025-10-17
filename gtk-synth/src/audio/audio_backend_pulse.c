// audio_backend_pulse.c
// PulseAudio backend implementation

#include "audio_backend.h"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

struct AudioBackend {
    AudioBackendType type;
    float sample_rate;
    int buffer_size;
    AudioProcessCallback callback;
    void* user_data;

    pa_simple* pa_stream;
    pthread_t audio_thread;
    bool running;
    float* process_buffer;
};

static void* audio_thread_func(void* arg) {
    AudioBackend* backend = (AudioBackend*)arg;

    while (backend->running) {
        // Generate audio samples
        backend->callback(backend->process_buffer, backend->buffer_size, backend->user_data);

        // Write to PulseAudio
        int error;
        if (pa_simple_write(backend->pa_stream, backend->process_buffer,
                           backend->buffer_size * sizeof(float), &error) < 0) {
            fprintf(stderr, "PulseAudio write error: %s\n", pa_strerror(error));
        }
    }

    return NULL;
}

AudioBackend* audio_backend_create(AudioBackendType type,
                                   int sample_rate,
                                   int buffer_size,
                                   AudioProcessCallback callback,
                                   void* user_data) {
    if (type != AUDIO_BACKEND_PULSEAUDIO) {
        fprintf(stderr, "Only PulseAudio backend is currently implemented\n");
        return NULL;
    }

    AudioBackend* backend = (AudioBackend*)calloc(1, sizeof(AudioBackend));
    if (!backend) return NULL;

    backend->type = type;
    backend->sample_rate = (float)sample_rate;
    backend->buffer_size = buffer_size;
    backend->callback = callback;
    backend->user_data = user_data;
    backend->running = false;

    // Allocate process buffer
    backend->process_buffer = (float*)calloc(buffer_size, sizeof(float));
    if (!backend->process_buffer) {
        free(backend);
        return NULL;
    }

    // Configure PulseAudio
    pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32LE,
        .rate = (uint32_t)sample_rate,
        .channels = 1
    };

    // Validate sample spec
    if (!pa_sample_spec_valid(&ss)) {
        fprintf(stderr, "Invalid PulseAudio sample spec\n");
        free(backend->process_buffer);
        free(backend);
        return NULL;
    }

    pa_buffer_attr ba = {
        .maxlength = (uint32_t)-1,
        .tlength = (uint32_t)(buffer_size * sizeof(float)),
        .prebuf = (uint32_t)-1,
        .minreq = (uint32_t)-1,
        .fragsize = (uint32_t)-1
    };

    int error;
    backend->pa_stream = pa_simple_new(
        NULL,                   // Default server
        "PM Synth GTK",        // Application name
        PA_STREAM_PLAYBACK,    // Playback mode
        NULL,                   // Default device
        "Synthesizer",         // Stream description
        &ss,                   // Sample spec
        NULL,                   // Default channel map
        &ba,                   // Buffer attributes
        &error                 // Error code
    );

    if (!backend->pa_stream) {
        fprintf(stderr, "Failed to create PulseAudio stream: %s (error code: %d)\n",
                pa_strerror(error), error);
        fprintf(stderr, "Sample rate: %d, channels: %d, buffer size: %d\n",
                sample_rate, ss.channels, buffer_size);
        free(backend->process_buffer);
        free(backend);
        return NULL;
    }

    fprintf(stderr, "PulseAudio stream created successfully\n");

    return backend;
}

void audio_backend_destroy(AudioBackend* backend) {
    if (!backend) return;

    if (backend->running) {
        audio_backend_stop(backend);
    }

    if (backend->pa_stream) {
        pa_simple_free(backend->pa_stream);
    }

    free(backend->process_buffer);
    free(backend);
}

bool audio_backend_start(AudioBackend* backend) {
    if (!backend || backend->running) return false;

    backend->running = true;

    if (pthread_create(&backend->audio_thread, NULL, audio_thread_func, backend) != 0) {
        fprintf(stderr, "Failed to create audio thread\n");
        backend->running = false;
        return false;
    }

    return true;
}

void audio_backend_stop(AudioBackend* backend) {
    if (!backend || !backend->running) return;

    backend->running = false;
    pthread_join(backend->audio_thread, NULL);

    // Drain the stream
    int error;
    pa_simple_drain(backend->pa_stream, &error);
}

bool audio_backend_is_running(AudioBackend* backend) {
    return backend && backend->running;
}

float audio_backend_get_sample_rate(AudioBackend* backend) {
    return backend ? backend->sample_rate : 0.0f;
}

int audio_backend_get_buffer_size(AudioBackend* backend) {
    return backend ? backend->buffer_size : 0;
}

const char* audio_backend_get_name(AudioBackend* backend) {
    if (!backend) return "None";

    switch (backend->type) {
        case AUDIO_BACKEND_PULSEAUDIO: return "PulseAudio";
        case AUDIO_BACKEND_JACK: return "JACK";
        case AUDIO_BACKEND_ALSA: return "ALSA";
        default: return "Unknown";
    }
}
