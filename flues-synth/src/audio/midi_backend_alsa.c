// ALSA MIDI Backend - Auto-connects to external MIDI devices
// For headless operation on Raspberry Pi

#define _GNU_SOURCE  // For strdup()
#include "midi_backend_alsa.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

struct MidiBackendALSA {
    snd_seq_t* seq_handle;
    int input_port;
    int connected_port;

    MidiEventCallback callback;
    void* callback_user_data;

    pthread_t midi_thread;
    bool running;
    bool debug;
};

// Find best external MIDI input port
// Returns client:port string or NULL
static char* find_best_midi_port(snd_seq_t* seq) {
    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    char* best_port = NULL;
    int best_score = -1;

    // Iterate through all clients
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);

        // Skip our own client and system ports
        if (client == snd_seq_client_id(seq) || client == 0) {
            continue;
        }

        // Iterate through client's ports
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            unsigned int type = snd_seq_port_info_get_type(pinfo);

            // Check if port can send (output) and is hardware
            if ((caps & SND_SEQ_PORT_CAP_READ) &&
                (caps & SND_SEQ_PORT_CAP_SUBS_READ)) {

                int score = 0;
                const char* name = snd_seq_port_info_get_name(pinfo);

                // Prefer hardware ports
                if (type & SND_SEQ_PORT_TYPE_HARDWARE) {
                    score += 10;
                }

                // Prefer MIDI generic over specific types
                if (type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) {
                    score += 5;
                }

                // Avoid ports with "Through" in the name
                if (strstr(name, "Through") == NULL) {
                    score += 3;
                }

                printf("MIDI: Found port %d:%d '%s' (score: %d)\n",
                       client, snd_seq_port_info_get_port(pinfo), name, score);

                if (score > best_score) {
                    best_score = score;
                    if (best_port) free(best_port);

                    char port_str[32];
                    snprintf(port_str, sizeof(port_str), "%d:%d",
                             client, snd_seq_port_info_get_port(pinfo));
                    best_port = strdup(port_str);
                }
            }
        }
    }

    return best_port;
}

// MIDI processing thread
static void* midi_thread_func(void* arg) {
    MidiBackendALSA* backend = (MidiBackendALSA*)arg;

    while (backend->running) {
        snd_seq_event_t* ev;

        // Wait for MIDI events (blocking, 1 second timeout)
        if (snd_seq_event_input(backend->seq_handle, &ev) < 0) {
            continue;
        }

        MidiEvent event;
        bool send_event = false;
        event.channel = 0;

        switch (ev->type) {
            case SND_SEQ_EVENT_NOTEON:
                if (ev->data.note.velocity > 0) {
                    event.type = MIDI_NOTE_ON;
                    event.channel = ev->data.note.channel;
                    event.note = ev->data.note.note;
                    event.velocity = ev->data.note.velocity;
                    send_event = true;
                } else {
                    // Note on with velocity 0 = note off
                    event.type = MIDI_NOTE_OFF;
                    event.channel = ev->data.note.channel;
                    event.note = ev->data.note.note;
                    event.velocity = 0;
                    send_event = true;
                }
                break;

            case SND_SEQ_EVENT_NOTEOFF:
                event.type = MIDI_NOTE_OFF;
                event.channel = ev->data.note.channel;
                event.note = ev->data.note.note;
                event.velocity = ev->data.note.velocity;
                send_event = true;
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                event.type = MIDI_CONTROL_CHANGE;
                event.channel = ev->data.control.channel;
                event.cc_number = ev->data.control.param;
                event.cc_value = ev->data.control.value;

                // Check for All Notes Off / All Sounds Off
                if (event.cc_number == 120 || event.cc_number == 123) {
                    event.type = MIDI_ALL_NOTES_OFF;
                }
                send_event = true;
                break;

            case SND_SEQ_EVENT_PGMCHANGE:
                event.type = MIDI_PROGRAM_CHANGE;
                event.channel = ev->data.control.channel;
                event.program_number = ev->data.control.value;
                send_event = true;
                break;
        }

        if (send_event && backend->callback) {
            if (backend->debug) {
                switch (event.type) {
                    case MIDI_NOTE_ON:
                        printf("MIDI DBG: ch%d note on  %3u vel %3u (src %d:%d)\n",
                               event.channel + 1,
                               event.note,
                               event.velocity,
                               ev->source.client,
                               ev->source.port);
                        break;
                    case MIDI_NOTE_OFF:
                        printf("MIDI DBG: ch%d note off %3u vel %3u (src %d:%d)\n",
                               event.channel + 1,
                               event.note,
                               event.velocity,
                               ev->source.client,
                               ev->source.port);
                        break;
                    case MIDI_CONTROL_CHANGE:
                        printf("MIDI DBG: ch%d CC %3u -> %3u (src %d:%d)\n",
                               event.channel + 1,
                               event.cc_number,
                               event.cc_value,
                               ev->source.client,
                               ev->source.port);
                        break;
                    case MIDI_ALL_NOTES_OFF:
                        printf("MIDI DBG: ch%d ALL NOTES OFF (src %d:%d)\n",
                               event.channel + 1,
                               ev->source.client,
                               ev->source.port);
                        break;
                    case MIDI_PROGRAM_CHANGE:
                        printf("MIDI DBG: ch%d PROGRAM CHANGE %3u (src %d:%d)\n",
                               event.channel + 1,
                               event.program_number,
                               ev->source.client,
                               ev->source.port);
                        break;
                }
            }
            backend->callback(&event, backend->callback_user_data);
        }

        snd_seq_free_event(ev);
    }

    return NULL;
}

MidiBackendALSA* midi_backend_alsa_create(MidiEventCallback callback, void* user_data) {
    MidiBackendALSA* backend = (MidiBackendALSA*)calloc(1, sizeof(MidiBackendALSA));
    if (!backend) {
        fprintf(stderr, "MIDI: Failed to allocate backend\n");
        return NULL;
    }

    backend->callback = callback;
    backend->callback_user_data = user_data;
    const char* dbg = getenv("FLUES_MIDI_DEBUG");
    backend->debug = dbg && dbg[0] == '1';
    backend->running = false;
    backend->connected_port = -1;
    backend->debug = getenv("FLUES_MIDI_DEBUG") != NULL;

    // Open ALSA sequencer
    if (snd_seq_open(&backend->seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        fprintf(stderr, "MIDI: Failed to open ALSA sequencer\n");
        free(backend);
        return NULL;
    }

    // Set client name
    snd_seq_set_client_name(backend->seq_handle, "Flues-Synth");

    // Create input port
    backend->input_port = snd_seq_create_simple_port(backend->seq_handle, "MIDI In",
                                                      SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                                      SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

    if (backend->input_port < 0) {
        fprintf(stderr, "MIDI: Failed to create input port\n");
        snd_seq_close(backend->seq_handle);
        free(backend);
        return NULL;
    }

    // Find and connect to best MIDI port
    char* best_port = find_best_midi_port(backend->seq_handle);
    if (best_port) {
        snd_seq_addr_t sender;
        if (snd_seq_parse_address(backend->seq_handle, &sender, best_port) == 0) {
            snd_seq_port_subscribe_t* subs;
            snd_seq_port_subscribe_alloca(&subs);
            snd_seq_port_subscribe_set_sender(subs, &sender);

            snd_seq_addr_t dest;
            dest.client = snd_seq_client_id(backend->seq_handle);
            dest.port = backend->input_port;
            snd_seq_port_subscribe_set_dest(subs, &dest);

            if (snd_seq_subscribe_port(backend->seq_handle, subs) == 0) {
                printf("MIDI: Auto-connected to port %s\n", best_port);
                backend->connected_port = 1;
            } else {
                fprintf(stderr, "MIDI: Failed to connect to port %s\n", best_port);
            }
        }
        free(best_port);
    } else {
        printf("MIDI: No external MIDI ports found (will accept connections manually)\n");
    }

    printf("MIDI: Initialized (client ID: %d, port: %d)\n",
           snd_seq_client_id(backend->seq_handle), backend->input_port);
    if (backend->debug) {
        printf("MIDI: Debug logging enabled (set FLUES_MIDI_DEBUG=0 to disable)\n");
    }

    return backend;
}

void midi_backend_alsa_destroy(MidiBackendALSA* backend) {
    if (!backend) return;

    if (backend->running) {
        midi_backend_alsa_stop(backend);
    }

    if (backend->seq_handle) {
        snd_seq_close(backend->seq_handle);
    }

    free(backend);
}

bool midi_backend_alsa_start(MidiBackendALSA* backend) {
    if (!backend || backend->running) {
        return false;
    }

    backend->running = true;

    // Create MIDI thread
    if (pthread_create(&backend->midi_thread, NULL, midi_thread_func, backend) != 0) {
        fprintf(stderr, "MIDI: Failed to create MIDI thread\n");
        backend->running = false;
        return false;
    }

    printf("MIDI: Thread started\n");
    return true;
}

void midi_backend_alsa_stop(MidiBackendALSA* backend) {
    if (!backend || !backend->running) {
        return;
    }

    backend->running = false;
    pthread_join(backend->midi_thread, NULL);

    printf("MIDI: Thread stopped\n");
}
