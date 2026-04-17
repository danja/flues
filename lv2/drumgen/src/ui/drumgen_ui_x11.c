#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "drumgen_schema.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PLUGIN_URI "https://danja.github.io/flues/plugins/drumgen"
#define UI_URI PLUGIN_URI "#ui"

#define PREVIEW_MAX_STEPS 64
#define PREVIEW_LANES DRUMGEN_LANE_COUNT

typedef enum {
    GROUP_GLOBAL = 0,
    GROUP_FEEL,
    GROUP_LANES,
    GROUP_ACTIONS,
    GROUP_PREVIEW,
    GROUP_COUNT
} GroupIndex;

typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    bool is_int;
} ControlDesc;

typedef struct {
    uint32_t port;
    const char* label;
    float min;
    float max;
    float value;
    bool is_int;
    int x;
    int y;
    int width;
    int height;
} Slider;

typedef struct {
    uint32_t port;
    const char* label;
    const char** items;
    int count;
    int value;
    int host_offset;
    bool open;
    int item_height;
    int x;
    int y;
    int width;
    int height;
} Selector;

typedef struct {
    uint32_t port;
    const char* label;
    int counter;
    int x;
    int y;
    int width;
    int height;
} ActionButton;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} GroupRect;

typedef struct {
    uint32_t state;
} PreviewRng;

typedef struct {
    int total_steps;
    int steps_per_bar;
    int hits[PREVIEW_LANES][PREVIEW_MAX_STEPS];
} PreviewPattern;

typedef struct {
    LV2UI_Write_Function write;
    LV2UI_Controller controller;

    Display* display;
    int screen;
    Window window;
    cairo_surface_t* surface;

    pthread_t thread;
    pthread_mutex_t mutex;
    volatile bool running;
    volatile bool needs_redraw;

    int width;
    int height;

    Slider sliders[11];
    int slider_count;
    int active_slider;

    Selector selectors[5];
    int selector_count;
    int open_selector;

    ActionButton actions[3];
    GroupRect groups[GROUP_COUNT];
} DrumGenUI;

static const char* kGroupNames[GROUP_COUNT] = {
    "GLOBAL",
    "FEEL",
    "LANES",
    "ACTIONS",
    "PREVIEW"
};

static const char* kGenreNames[] = {
    "Rock", "Disco", "Shuffle", "Electro", "Dub", "Motorik", "Bossa", "Afro"
};

static const char* kChannelNames[] = {
    "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};

static const char* kKitMapNames[] = {
    "Flues Drumkit", "GM"
};

static const char* kBarNames[] = {
    "1", "2", "3", "4"
};

static const char* kResolutionNames[] = {
    "1/8", "1/16", "1/16T"
};

static const char* kLaneNames[PREVIEW_LANES] = {
    "KICK", "CLAP", "SNARE", "CRASH", "CHH", "TOML", "OHH", "TOMH", "BASH", "COW", "CLAVE"
};

static const ControlDesc kControlDescs[] = {
    { GROUP_FEEL, "DENS", PORT_DENSITY, 0.0f, 1.0f, 0.58f, false },
    { GROUP_FEEL, "VAR", PORT_VARIATION, 0.0f, 1.0f, 0.35f, false },
    { GROUP_FEEL, "FILL", PORT_FILL, 0.0f, 1.0f, 0.30f, false },
    { GROUP_FEEL, "VARY", PORT_VARY, 0.0f, 100.0f, 0.0f, true },
    { GROUP_FEEL, "SEED", PORT_SEED, 0.0f, 65535.0f, 1.0f, true },
    { GROUP_LANES, "KICK", PORT_KICK_AMT, 0.0f, 1.0f, 0.78f, false },
    { GROUP_LANES, "BACK", PORT_BACKBEAT_AMT, 0.0f, 1.0f, 0.76f, false },
    { GROUP_LANES, "HAT", PORT_HAT_AMT, 0.0f, 1.0f, 0.82f, false },
    { GROUP_LANES, "PERC", PORT_AUX_AMT, 0.0f, 1.0f, 0.28f, false },
    { GROUP_LANES, "TOM", PORT_TOM_AMT, 0.0f, 1.0f, 0.30f, false },
    { GROUP_LANES, "METAL", PORT_METAL_AMT, 0.0f, 1.0f, 0.26f, false }
};

static pthread_mutex_t g_xlib_init_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_xlib_threads_ready = false;

static void ensure_xlib_threads(void) {
    pthread_mutex_lock(&g_xlib_init_lock);
    if (!g_xlib_threads_ready) {
        XInitThreads();
        g_xlib_threads_ready = true;
    }
    pthread_mutex_unlock(&g_xlib_init_lock);
}

static void notify_host(DrumGenUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static float clampf_local(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int clampi_local(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < (rx + rw) && y >= ry && y < (ry + rh);
}

static int steps_per_beat_for_resolution(int resolution) {
    switch (resolution) {
        case 0: return 2;
        case 2: return 3;
        case 1:
        default: return 4;
    }
}

static bool is_offbeat_step(int step_in_bar, int steps_per_beat) {
    if (steps_per_beat == 3) {
        return (step_in_bar % steps_per_beat) == 2;
    }
    return (step_in_bar % steps_per_beat) == (steps_per_beat / 2);
}

static bool is_fill_zone_step(int step_in_bar, int steps_per_bar, int steps_per_beat, float fill) {
    const int fill_beats = fill > 0.62f ? 2 : 1;
    const int fill_steps = clampi_local(fill_beats * steps_per_beat, steps_per_beat, steps_per_bar);
    return step_in_bar >= steps_per_bar - fill_steps;
}

static void preview_rng_seed(PreviewRng* rng, uint32_t seed_value) {
    rng->state = seed_value ? seed_value : 0x13579BDFu;
}

static uint32_t preview_rng_next_u32(PreviewRng* rng) {
    rng->state = rng->state * 1664525u + 1013904223u;
    return rng->state;
}

static float preview_rng_next_float(PreviewRng* rng) {
    return (float)(preview_rng_next_u32(rng) & 0x00FFFFFFu) / 16777215.0f;
}

static int preview_rng_next_int(PreviewRng* rng, int min_value, int max_value) {
    if (max_value <= min_value) {
        return min_value;
    }
    return min_value + (int)(preview_rng_next_u32(rng) % (uint32_t)(max_value - min_value + 1));
}

static bool euclid_hit(int step, int pulses, int offset, int length) {
    if (length <= 0 || pulses <= 0) {
        return false;
    }
    if (pulses >= length) {
        return true;
    }
    {
        const int base_step = (step - offset + length) % length;
        return ((base_step * pulses) % length) < pulses;
    }
}

static float selector_value(const DrumGenUI* ui, uint32_t port, float fallback) {
    int i;
    for (i = 0; i < ui->selector_count; ++i) {
        if (ui->selectors[i].port == port) {
            return (float)(ui->selectors[i].value + ui->selectors[i].host_offset);
        }
    }
    return fallback;
}

static float slider_value(const DrumGenUI* ui, uint32_t port, float fallback) {
    int i;
    for (i = 0; i < ui->slider_count; ++i) {
        if (ui->sliders[i].port == port) {
            return ui->sliders[i].value;
        }
    }
    return fallback;
}

static float preview_anchor_probability(int genre,
                                        int lane,
                                        int bar_index,
                                        int beat_index,
                                        int sub_index,
                                        int steps_per_beat,
                                        bool fill_bar,
                                        float kick,
                                        float backbeat,
                                        float hat,
                                        float tom,
                                        float metal,
                                        float perc,
                                        float fill,
                                        float variation) {
    const bool beat_start = sub_index == 0;
    const bool offbeat = is_offbeat_step(beat_index * steps_per_beat + sub_index, steps_per_beat);
    const bool late_sub = sub_index == steps_per_beat - 1;

    switch (lane) {
        case LANE_KICK:
            switch (genre) {
                case 1:
                case 5:
                    if (beat_start) return 0.86f + 0.12f * kick;
                    if (offbeat && beat_index == 3) return 0.12f + 0.10f * variation;
                    break;
                case 2:
                    if (beat_index == 0 && beat_start) return 0.96f;
                    if (beat_index == 2 && beat_start) return 0.62f + 0.18f * kick;
                    if (offbeat && (beat_index == 1 || beat_index == 3)) return 0.16f + 0.10f * variation;
                    break;
                case 3:
                    if (beat_index == 0 && beat_start) return 0.94f;
                    if (beat_index == 2 && beat_start) return 0.40f + 0.22f * kick;
                    if (late_sub) return 0.12f + 0.22f * variation;
                    break;
                case 4:
                    if (beat_index == 0 && beat_start) return 0.96f;
                    if (beat_index == 2 && beat_start) return 0.24f + 0.18f * kick;
                    if (offbeat && beat_index == 3) return 0.10f + 0.10f * variation;
                    break;
                case 6:
                    if (beat_index == 0 && beat_start) return 0.82f;
                    if (beat_index == 1 && late_sub) return 0.34f;
                    if (beat_index == 2 && beat_start) return 0.56f;
                    if (beat_index == 3 && offbeat) return 0.42f;
                    break;
                case 7:
                    if (beat_index == 0 && beat_start) return 0.84f;
                    if (beat_index == 1 && offbeat) return 0.34f;
                    if (beat_index == 2 && beat_start) return 0.58f;
                    if (beat_index == 3 && offbeat) return 0.38f;
                    break;
                default:
                    if (beat_index == 0 && beat_start) return 0.98f;
                    if (beat_index == 2 && beat_start) return 0.68f + 0.18f * kick;
                    if (beat_index == 3 && late_sub) return 0.10f + 0.18f * variation;
                    if (beat_index == 1 && beat_start) return 0.12f + 0.10f * kick;
                    break;
            }
            break;
        case LANE_SNARE:
            if ((beat_index == 1 || beat_index == 3) && beat_start) {
                if (genre == 1) return 0.76f + 0.18f * backbeat;
                if (genre == 3) return 0.72f + 0.18f * backbeat;
                if (genre == 4) return 0.64f + 0.16f * backbeat;
                return 0.84f + 0.12f * backbeat;
            }
            if (fill_bar && beat_index >= 2 && late_sub) return 0.08f + 0.24f * fill * backbeat;
            if (genre == 7 && offbeat) return 0.08f + 0.10f * variation;
            if (genre == 2 && late_sub) return 0.06f + 0.10f * variation;
            break;
        case LANE_CLAP:
            if ((beat_index == 1 || beat_index == 3) && beat_start) {
                if (genre == 1) return 0.78f + 0.16f * backbeat;
                if (genre == 3) return 0.52f + 0.20f * backbeat;
                if (genre == 4) return 0.18f + 0.14f * backbeat;
                return 0.18f + 0.34f * backbeat;
            }
            if (genre == 1 && offbeat) return 0.08f + 0.14f * variation;
            if (fill_bar && beat_index == 3 && !beat_start) return 0.06f + 0.18f * fill * backbeat;
            break;
        case LANE_CRASH:
            if (beat_start && beat_index == 0) {
                return (bar_index == 0 ? 0.34f : 0.16f) + 0.18f * metal;
            }
            if (fill_bar && beat_start && beat_index >= 2) {
                return 0.08f + 0.18f * fill * (0.55f + 0.45f * metal);
            }
            break;
        case LANE_CLOSED_HAT:
            if (steps_per_beat == 3) {
                if (sub_index == 0) return 0.70f + 0.18f * hat;
                if (sub_index == 2) return 0.54f + 0.20f * hat;
                return 0.18f + 0.16f * variation;
            }
            if (steps_per_beat == 2) {
                return offbeat ? 0.66f + 0.20f * hat : 0.74f + 0.16f * hat;
            }
            if (sub_index == 0 || sub_index == 2) return 0.74f + 0.18f * hat;
            return 0.18f + 0.28f * hat;
        case LANE_OPEN_HAT:
            if (offbeat) {
                if (genre == 1 || genre == 5) return 0.34f + 0.32f * hat;
                if (genre == 4) return 0.14f + 0.18f * hat;
                return 0.18f + 0.20f * hat;
            }
            if (fill_bar && beat_index == 3 && late_sub) return 0.16f + 0.14f * fill;
            break;
        case LANE_LOW_TOM:
            if (fill_bar && beat_index >= 2 && (offbeat || late_sub)) return 0.08f + 0.28f * fill + 0.12f * tom;
            break;
        case LANE_HIGH_TOM:
            if (fill_bar && beat_index >= 2 && !beat_start) return 0.08f + 0.26f * fill + 0.12f * tom;
            break;
        case LANE_BASH:
            switch (genre) {
                case 3:
                    if (beat_start && beat_index == 0) return 0.14f + 0.16f * metal;
                    if (fill_bar && beat_index >= 2 && (beat_start || late_sub)) return 0.10f + 0.26f * fill * metal;
                    if (late_sub && beat_index == 3) return 0.08f + 0.14f * variation;
                    break;
                case 5:
                    if (beat_start && beat_index == 0) return 0.12f + 0.18f * metal;
                    if (fill_bar && beat_index == 3 && beat_start) return 0.10f + 0.22f * fill * metal;
                    break;
                case 4:
                    if ((offbeat && beat_index == 3) || (late_sub && beat_index == 2)) return 0.10f + 0.18f * metal;
                    if (fill_bar && beat_index == 3) return 0.10f + 0.18f * fill * metal;
                    break;
                default:
                    if (fill_bar && beat_index >= 3 && (offbeat || late_sub)) return 0.06f + 0.18f * fill * metal;
                    break;
            }
            break;
        case LANE_COWBELL:
            switch (genre) {
                case 1:
                    if (offbeat) return 0.16f + 0.24f * perc;
                    if (beat_start && (beat_index == 1 || beat_index == 3)) return 0.08f + 0.14f * perc;
                    break;
                case 5:
                    if (beat_start && (beat_index == 0 || beat_index == 2)) return 0.10f + 0.18f * perc;
                    if (offbeat) return 0.10f + 0.16f * perc;
                    break;
                case 6:
                    if ((beat_index == 0 || beat_index == 2) && late_sub) return 0.16f + 0.18f * perc;
                    if ((beat_index == 1 || beat_index == 3) && offbeat) return 0.16f + 0.20f * perc;
                    break;
                case 7:
                    if (offbeat || late_sub) return 0.14f + 0.20f * perc;
                    break;
                default:
                    if (fill_bar && beat_index >= 2 && offbeat) return 0.06f + 0.16f * fill * perc;
                    break;
            }
            break;
        case LANE_CLAVE:
            switch (genre) {
                case 6:
                    if (beat_index == 0 && beat_start) return 0.26f + 0.16f * perc;
                    if (beat_index == 1 && offbeat) return 0.22f + 0.16f * perc;
                    if (beat_index == 2 && late_sub) return 0.22f + 0.16f * perc;
                    if (beat_index == 3 && beat_start) return 0.24f + 0.16f * perc;
                    break;
                case 7:
                    if (beat_index == 0 && beat_start) return 0.20f + 0.18f * perc;
                    if (beat_index == 1 && late_sub) return 0.18f + 0.18f * perc;
                    if (beat_index == 2 && offbeat) return 0.22f + 0.18f * perc;
                    if (beat_index == 3 && beat_start) return 0.18f + 0.16f * perc;
                    break;
                case 2:
                    if (beat_index == 1 && late_sub) return 0.10f + 0.14f * perc;
                    if (beat_index == 3 && offbeat) return 0.10f + 0.14f * perc;
                    break;
                default:
                    if (fill_bar && beat_index == 3 && !beat_start) return 0.06f + 0.14f * fill * perc;
                    break;
            }
            break;
        default:
            break;
    }
    return 0.0f;
}

static int preview_euclid_pulses(int lane,
                                 int genre,
                                 int steps_per_bar,
                                 bool fill_bar,
                                 float density,
                                 float variation,
                                 float fill,
                                 float kick,
                                 float backbeat,
                                 float hat,
                                 float tom,
                                 float metal,
                                 float perc,
                                 PreviewRng* rng) {
    float desired_hits = 0.0f;
    switch (lane) {
        case LANE_KICK:
            desired_hits = 1.0f + ((genre == 1 || genre == 5) ? 3.0f : 1.6f) * density * kick;
            break;
        case LANE_CLAP:
            desired_hits = ((genre == 1 || genre == 3) ? 1.6f : 0.8f) * density * backbeat;
            break;
        case LANE_SNARE:
            desired_hits = 1.0f + 1.0f * density * backbeat * (0.4f + 0.6f * variation);
            break;
        case LANE_CRASH:
            desired_hits = fill_bar ? (0.3f + 1.2f * fill * metal) : (0.15f + 0.35f * metal);
            break;
        case LANE_CLOSED_HAT:
            desired_hits = (steps_per_bar * (0.20f + 0.55f * density * hat)) + (steps_per_bar >= 16 ? 1.5f : 0.0f);
            break;
        case LANE_OPEN_HAT:
            desired_hits = 0.4f + 1.5f * density * hat;
            break;
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM:
            desired_hits = fill_bar ? (0.4f + 2.4f * fill * tom) : 0.0f;
            break;
        case LANE_BASH:
            desired_hits = fill_bar
                ? (0.2f + 1.4f * fill * metal)
                : (((genre == 3) || (genre == 4) || (genre == 5))
                    ? (0.15f + 0.75f * variation * metal)
                    : (0.05f + 0.35f * variation * metal));
            break;
        case LANE_COWBELL:
            if (genre == 1 || genre == 5) {
                desired_hits = 0.8f + 3.0f * density * perc;
            } else if (genre == 6 || genre == 7) {
                desired_hits = 0.8f + 2.4f * density * perc;
            } else {
                desired_hits = 0.2f + 1.0f * density * variation * perc;
            }
            break;
        case LANE_CLAVE:
            if (genre == 6 || genre == 7) {
                desired_hits = 0.8f + 2.0f * density * perc;
            } else if (genre == 2) {
                desired_hits = 0.4f + 1.4f * density * perc;
            } else {
                desired_hits = 0.15f + 0.8f * variation * perc;
            }
            break;
        default:
            break;
    }
    return clampi_local((int)lroundf(desired_hits) + preview_rng_next_int(rng, -(int)lroundf(variation * 2.0f), (int)lroundf(variation * 2.0f)),
                        0,
                        steps_per_bar);
}

static float preview_euclid_influence(int lane, bool fill_bar, float variation, float kick, float backbeat, float hat, float tom, float metal, float perc) {
    switch (lane) {
        case LANE_KICK: return 0.10f + 0.35f * variation * kick;
        case LANE_CLAP:
        case LANE_SNARE: return 0.10f + 0.32f * variation * backbeat;
        case LANE_CRASH: return 0.10f + 0.28f * variation * metal + (fill_bar ? 0.16f : 0.0f);
        case LANE_CLOSED_HAT: return 0.24f + 0.52f * variation * hat;
        case LANE_OPEN_HAT: return 0.16f + 0.42f * variation * hat;
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM: return 0.10f + 0.44f * variation * tom + (fill_bar ? 0.24f : 0.0f);
        case LANE_BASH: return 0.10f + 0.36f * variation * metal + (fill_bar ? 0.28f : 0.0f);
        case LANE_COWBELL: return 0.18f + 0.34f * variation * perc;
        case LANE_CLAVE: return 0.16f + 0.32f * variation * perc + (fill_bar ? 0.10f : 0.0f);
        default: return 0.20f;
    }
}

static void clear_preview_hit(PreviewPattern* pattern, int lane, int step) {
    if (!pattern || lane < 0 || lane >= PREVIEW_LANES || step < 0 || step >= pattern->total_steps) {
        return;
    }
    pattern->hits[lane][step] = 0;
}

static void set_preview_hit(PreviewPattern* pattern, int lane, int step) {
    if (!pattern || lane < 0 || lane >= PREVIEW_LANES || step < 0 || step >= pattern->total_steps) {
        return;
    }
    pattern->hits[lane][step] = 1;
}

static void apply_preview_fill_overlay(PreviewPattern* pattern,
                                       int genre,
                                       int steps_per_beat,
                                       float fill,
                                       float metal,
                                       uint32_t seed) {
    PreviewRng rng;
    const int steps_per_bar = pattern->steps_per_bar;
    const int bars = pattern->total_steps / steps_per_bar;
    const int last_bar = bars - 1;
    const int bar_start = last_bar * steps_per_bar;
    const int bar_end = pattern->total_steps;
    const int fill_beats = fill > 0.62f ? 2 : 1;
    const int fill_steps = clampi_local(fill_beats * steps_per_beat, steps_per_beat, steps_per_bar);
    const int zone_start = clampi_local(bar_end - fill_steps, bar_start, bar_end);
    int motif = 0;

    if (!pattern || fill < 0.08f || bars <= 0) {
        return;
    }

    preview_rng_seed(&rng, seed ^ 0xC001D00Du);

    switch (genre) {
        case 0:
        case 2:
            motif = preview_rng_next_int(&rng, 0, 1);
            break;
        case 1:
        case 5:
            motif = 2 + preview_rng_next_int(&rng, 0, 1);
            break;
        case 3:
        case 4:
            motif = 1 + preview_rng_next_int(&rng, 0, 2);
            break;
        case 6:
        case 7:
            motif = (preview_rng_next_float(&rng) < 0.65f) ? 2 : 3;
            break;
        default:
            motif = preview_rng_next_int(&rng, 0, 3);
            break;
    }

    {
        int step;
        for (step = zone_start; step < bar_end; ++step) {
            clear_preview_hit(pattern, LANE_OPEN_HAT, step);
            clear_preview_hit(pattern, LANE_CRASH, step);
            clear_preview_hit(pattern, LANE_BASH, step);
            if (fill > 0.45f && ((step - zone_start) % 2 == 1)) {
                clear_preview_hit(pattern, LANE_CLOSED_HAT, step);
            }
            if (fill > 0.60f && ((step % steps_per_beat) != 0)) {
                clear_preview_hit(pattern, LANE_KICK, step);
            }
        }
    }

    switch (motif) {
        case 0: {
            int index = 0;
            const int stride = fill > 0.65f ? 1 : 2;
            int step;
            for (step = zone_start; step < bar_end; step += stride) {
                set_preview_hit(pattern, (index % 2 == 0) ? LANE_LOW_TOM : LANE_HIGH_TOM, step);
                index += 1;
            }
            set_preview_hit(pattern, ((genre == 3) || (genre == 5)) ? LANE_BASH : LANE_CRASH, bar_end - 1);
            break;
        }
        case 1: {
            int step;
            for (step = zone_start; step < bar_end; ++step) {
                set_preview_hit(pattern, LANE_SNARE, step);
                if ((step - zone_start) % 2 == 1) {
                    set_preview_hit(pattern, LANE_CLAP, step);
                }
            }
            set_preview_hit(pattern, LANE_HIGH_TOM, clampi_local(bar_end - 2, zone_start, bar_end - 1));
            if (metal > 0.20f) {
                set_preview_hit(pattern, LANE_BASH, bar_end - 1);
            }
            break;
        }
        case 2: {
            int toggle = 0;
            int step;
            for (step = zone_start; step < bar_end; ++step) {
                if (is_offbeat_step(step - bar_start, steps_per_beat) || ((step - zone_start) % 2 == 0)) {
                    set_preview_hit(pattern, (toggle % 2 == 0) ? LANE_COWBELL : LANE_CLAVE, step);
                    toggle += 1;
                }
            }
            set_preview_hit(pattern, LANE_LOW_TOM, zone_start);
            set_preview_hit(pattern, LANE_HIGH_TOM, clampi_local(bar_end - 2, zone_start, bar_end - 1));
            break;
        }
        case 3:
        default: {
            int step;
            int index = 0;
            set_preview_hit(pattern, LANE_BASH, clampi_local(zone_start + fill_steps / 2, zone_start, bar_end - 1));
            for (step = zone_start; step < bar_end; step += 2) {
                const int lane = (index % 3 == 0) ? LANE_HIGH_TOM : ((index % 3 == 1) ? LANE_COWBELL : LANE_CLAVE);
                set_preview_hit(pattern, lane, step);
                index += 1;
            }
            set_preview_hit(pattern, LANE_CLAVE, clampi_local(bar_end - 2, zone_start, bar_end - 1));
            set_preview_hit(pattern, LANE_HIGH_TOM, bar_end - 1);
            break;
        }
    }
}

static void build_preview_pattern(DrumGenUI* ui, PreviewPattern* pattern) {
    const int genre = (int)selector_value(ui, PORT_GENRE, 0.0f);
    const int bars = (int)selector_value(ui, PORT_BARS, 2.0f);
    const int resolution = (int)selector_value(ui, PORT_RESOLUTION, 1.0f);
    const int steps_per_beat = steps_per_beat_for_resolution(resolution);
    const int steps_per_bar = steps_per_beat * 4;
    const int total_steps = clampi_local(bars * steps_per_bar, 1, PREVIEW_MAX_STEPS);
    const float density = slider_value(ui, PORT_DENSITY, 0.58f);
    const float variation = slider_value(ui, PORT_VARIATION, 0.35f);
    const float fill = slider_value(ui, PORT_FILL, 0.30f);
    const float kick = slider_value(ui, PORT_KICK_AMT, 0.78f);
    const float backbeat = slider_value(ui, PORT_BACKBEAT_AMT, 0.76f);
    const float hat = slider_value(ui, PORT_HAT_AMT, 0.82f);
    const float perc = slider_value(ui, PORT_AUX_AMT, 0.28f);
    const float tom = slider_value(ui, PORT_TOM_AMT, 0.30f);
    const float metal = slider_value(ui, PORT_METAL_AMT, 0.26f);
    const uint32_t seed = (uint32_t)slider_value(ui, PORT_SEED, 1.0f);

    int lane;
    int bar;

    memset(pattern, 0, sizeof(*pattern));
    pattern->steps_per_bar = steps_per_bar;
    pattern->total_steps = total_steps;

    for (bar = 0; bar < bars; ++bar) {
        PreviewRng rng;
        const bool fill_bar = (bar == bars - 1);
        const uint32_t bar_seed = seed ^ (uint32_t)(bar + 1) * 0x27D4EB2Du ^ (fill_bar ? 0xA511E9B3u : 0u);
        int pulses[PREVIEW_LANES];
        int offsets[PREVIEW_LANES];
        int step;

        preview_rng_seed(&rng, bar_seed);
        for (lane = 0; lane < PREVIEW_LANES; ++lane) {
            pulses[lane] = preview_euclid_pulses(lane, genre, steps_per_bar, fill_bar, density, variation, fill, kick, backbeat, hat, tom, metal, perc, &rng);
            offsets[lane] = pulses[lane] > 0 ? preview_rng_next_int(&rng, 0, steps_per_bar - 1) : 0;
        }

        for (step = 0; step < steps_per_bar; ++step) {
            const int global_step = bar * steps_per_bar + step;
            const int beat_index = step / steps_per_beat;
            const int sub_index = step % steps_per_beat;
            if (global_step >= total_steps) {
                break;
            }
            for (lane = 0; lane < PREVIEW_LANES; ++lane) {
                const float anchor_prob = preview_anchor_probability(genre, lane, bar, beat_index, sub_index, steps_per_beat, fill_bar,
                                                                     kick, backbeat, hat, tom, metal, perc, fill, variation);
                const bool anchor = anchor_prob > 0.0f && preview_rng_next_float(&rng) < clampf_local(anchor_prob, 0.0f, 1.0f);
                const bool euclid = euclid_hit(step, pulses[lane], offsets[lane], steps_per_bar);
                bool hit = anchor;
                if (!hit && euclid) {
                    const float chance = preview_euclid_influence(lane, fill_bar, variation, kick, backbeat, hat, tom, metal, perc);
                    hit = preview_rng_next_float(&rng) < chance;
                }
                if (hit) {
                    pattern->hits[lane][global_step] = 1;
                }
            }
        }
    }

    {
        int step;
        for (step = 0; step < total_steps; ++step) {
            if (pattern->hits[LANE_CLOSED_HAT][step] && pattern->hits[LANE_OPEN_HAT][step]) {
                if (is_offbeat_step(step % steps_per_bar, steps_per_beat)) {
                    pattern->hits[LANE_CLOSED_HAT][step] = 0;
                } else {
                    pattern->hits[LANE_OPEN_HAT][step] = 0;
                }
            }
            if (pattern->hits[LANE_COWBELL][step] && pattern->hits[LANE_CLAVE][step]) {
                if (genre == 6 || genre == 7) {
                    pattern->hits[LANE_COWBELL][step] = 0;
                } else {
                    pattern->hits[LANE_CLAVE][step] = 0;
                }
            }
            if (pattern->hits[LANE_SNARE][step] && pattern->hits[LANE_CLAP][step] && genre != 1 && genre != 3) {
                pattern->hits[LANE_CLAP][step] = 0;
            }
        }
    }

    apply_preview_fill_overlay(pattern, genre, steps_per_beat, fill, metal, seed);

    {
        int bar;
        for (bar = 0; bar < bars; ++bar) {
            int crash_hits = 0;
            int bash_hits = 0;
            int step;
            const int bar_start = bar * steps_per_bar;
            for (step = 0; step < steps_per_bar && (bar_start + step) < total_steps; ++step) {
                const int idx = bar_start + step;
                if (pattern->hits[LANE_CRASH][idx]) {
                    crash_hits += 1;
                    if (crash_hits > 2) {
                        pattern->hits[LANE_CRASH][idx] = 0;
                    }
                }
                if (pattern->hits[LANE_BASH][idx]) {
                    bash_hits += 1;
                    if (bash_hits > 2) {
                        pattern->hits[LANE_BASH][idx] = 0;
                    }
                }
                if (is_fill_zone_step(step, steps_per_bar, steps_per_beat, fill)) {
                    const bool fill_activity =
                        pattern->hits[LANE_LOW_TOM][idx] ||
                        pattern->hits[LANE_HIGH_TOM][idx] ||
                        pattern->hits[LANE_BASH][idx] ||
                        pattern->hits[LANE_COWBELL][idx] ||
                        pattern->hits[LANE_CLAVE][idx];
                    if (fill_activity) {
                        pattern->hits[LANE_OPEN_HAT][idx] = 0;
                        if ((step - (steps_per_bar - steps_per_beat)) % 2 != 0) {
                            pattern->hits[LANE_CLOSED_HAT][idx] = 0;
                        }
                    }
                }
            }
        }
    }
}

static int slider_at(DrumGenUI* ui, int x, int y) {
    int i;
    for (i = 0; i < ui->slider_count; ++i) {
        Slider* s = &ui->sliders[i];
        if (point_in_rect(x, y, s->x, s->y, s->width, s->height)) {
            return i;
        }
    }
    return -1;
}

static float slider_value_from_y(const Slider* slider, int y) {
    const int top = slider->y;
    const int bottom = slider->y + slider->height;
    float t = 1.0f - (float)(y - top) / (float)(bottom - top);
    t = clampf_local(t, 0.0f, 1.0f);
    return slider->min + t * (slider->max - slider->min);
}

static void set_slider_value(DrumGenUI* ui, Slider* slider, float value, bool notify) {
    value = clampf_local(value, slider->min, slider->max);
    if (slider->is_int) {
        value = floorf(value + 0.5f);
    }
    if (fabsf(value - slider->value) > 0.0001f) {
        slider->value = value;
        if (notify) {
            notify_host(ui, slider->port, value);
        }
        ui->needs_redraw = true;
    }
}

static int selector_menu_y(DrumGenUI* ui, const Selector* s) {
    const int menu_h = s->count * s->item_height;
    const int down_y = s->y + s->height;
    if (down_y + menu_h <= ui->height - 8) {
        return down_y;
    }
    return s->y - menu_h;
}

static int selector_index_at(DrumGenUI* ui, int x, int y) {
    int i;
    for (i = 0; i < ui->selector_count; ++i) {
        Selector* s = &ui->selectors[i];
        if (point_in_rect(x, y, s->x, s->y, s->width, s->height)) {
            return i;
        }
    }
    return -1;
}

static int selector_menu_item_at_ui(DrumGenUI* ui, const Selector* s, int x, int y) {
    const int menu_y = selector_menu_y(ui, s);
    if (!s->open) {
        return -1;
    }
    if (!point_in_rect(x, y, s->x, menu_y, s->width, s->count * s->item_height)) {
        return -1;
    }
    return clampi_local((y - menu_y) / s->item_height, 0, s->count - 1);
}

static ActionButton* action_at(DrumGenUI* ui, int x, int y) {
    int i;
    for (i = 0; i < 3; ++i) {
        ActionButton* a = &ui->actions[i];
        if (point_in_rect(x, y, a->x, a->y, a->width, a->height)) {
            return a;
        }
    }
    return NULL;
}

static void draw_group(cairo_t* cr, const GroupRect* rect, const char* title) {
    cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
    cairo_set_source_rgb(cr, 0.12, 0.13, 0.17);
    cairo_fill(cr);

    cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
    cairo_set_source_rgb(cr, 0.32, 0.34, 0.39);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.95, 0.79, 0.40);
    cairo_move_to(cr, rect->x + 10, rect->y + 16);
    cairo_show_text(cr, title);
}

static void draw_slider(cairo_t* cr, const Slider* s) {
    const int track_x = s->x + (s->width - 8) / 2;
    char value[24];
    cairo_text_extents_t ext;
    const float norm = (s->value - s->min) / (s->max - s->min);
    const int fill_h = (int)lrintf(norm * s->height);
    const int fill_y = s->y + (s->height - fill_h);

    cairo_rectangle(cr, track_x, s->y, 8, s->height);
    cairo_set_source_rgb(cr, 0.21, 0.22, 0.25);
    cairo_fill(cr);

    cairo_rectangle(cr, track_x, fill_y, 8, fill_h);
    cairo_set_source_rgb(cr, 0.95, 0.55, 0.18);
    cairo_fill(cr);

    cairo_rectangle(cr, s->x + 2, fill_y - 3, s->width - 4, 6);
    cairo_set_source_rgb(cr, 0.96, 0.92, 0.84);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);

    if (s->port == PORT_VARY) {
        snprintf(value, sizeof(value), "%d%%", (int)lroundf(s->value));
    } else if (s->is_int) {
        snprintf(value, sizeof(value), "%d", (int)lroundf(s->value));
    } else {
        snprintf(value, sizeof(value), "%.2f", s->value);
    }

    cairo_text_extents(cr, value, &ext);
    cairo_move_to(cr, s->x + (s->width - ext.width) / 2.0, s->y + s->height + 11.0);
    cairo_show_text(cr, value);

    cairo_text_extents(cr, s->label, &ext);
    cairo_move_to(cr, s->x + (s->width - ext.width) / 2.0, s->y + s->height + 24.0);
    cairo_show_text(cr, s->label);
}

static void draw_selector(cairo_t* cr, const Selector* s) {
    char text[96];
    cairo_text_extents_t ext;

    cairo_rectangle(cr, s->x, s->y, s->width, s->height);
    cairo_set_source_rgb(cr, 0.17, 0.18, 0.21);
    cairo_fill(cr);

    cairo_rectangle(cr, s->x, s->y, s->width, s->height);
    cairo_set_source_rgb(cr, 0.88, 0.72, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.94);

    snprintf(text, sizeof(text), "%s: %s", s->label, s->items[s->value]);
    cairo_text_extents(cr, text, &ext);
    cairo_move_to(cr, s->x + 8, s->y + (s->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, text);

    cairo_move_to(cr, s->x + s->width - 16, s->y + s->height / 2 - 3);
    cairo_line_to(cr, s->x + s->width - 8, s->y + s->height / 2 - 3);
    cairo_line_to(cr, s->x + s->width - 12, s->y + s->height / 2 + 3);
    cairo_close_path(cr);
    cairo_fill(cr);
}

static void draw_selector_open(cairo_t* cr, DrumGenUI* ui, const Selector* s) {
    int i;
    const int menu_y = selector_menu_y(ui, s);
    draw_selector(cr, s);

    cairo_rectangle(cr, s->x, menu_y, s->width, s->count * s->item_height);
    cairo_set_source_rgb(cr, 0.14, 0.15, 0.18);
    cairo_fill(cr);

    cairo_rectangle(cr, s->x, menu_y, s->width, s->count * s->item_height);
    cairo_set_source_rgb(cr, 0.88, 0.72, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);

    for (i = 0; i < s->count; ++i) {
        const int iy = menu_y + i * s->item_height;
        cairo_text_extents_t ext;
        if (i == s->value) {
            cairo_rectangle(cr, s->x + 1, iy + 1, s->width - 2, s->item_height - 2);
            cairo_set_source_rgb(cr, 0.25, 0.31, 0.21);
            cairo_fill(cr);
        }
        cairo_set_source_rgb(cr, 0.94, 0.94, 0.94);
        cairo_text_extents(cr, s->items[i], &ext);
        cairo_move_to(cr, s->x + 8, iy + (s->item_height - ext.height) / 2.0 - ext.y_bearing);
        cairo_show_text(cr, s->items[i]);
    }
}

static void draw_action(cairo_t* cr, const ActionButton* a) {
    cairo_text_extents_t ext;

    cairo_rectangle(cr, a->x, a->y, a->width, a->height);
    cairo_set_source_rgb(cr, 0.20, 0.23, 0.27);
    cairo_fill(cr);

    cairo_rectangle(cr, a->x, a->y, a->width, a->height);
    cairo_set_source_rgb(cr, 0.45, 0.77, 0.48);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.97, 0.97, 0.97);
    cairo_text_extents(cr, a->label, &ext);
    cairo_move_to(cr, a->x + (a->width - ext.width) / 2.0 - ext.x_bearing,
                  a->y + (a->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, a->label);
}

static void draw_preview(cairo_t* cr, DrumGenUI* ui, int x, int y, int width, int height) {
    PreviewPattern pattern;
    const int label_w = 66;
    const int inner_x = x + label_w;
    const int inner_y = y + 20;
    const int inner_w = width - label_w - 8;
    const int inner_h = height - 28;
    const int row_h = inner_h / PREVIEW_LANES;
    int lane;
    int step;

    build_preview_pattern(ui, &pattern);

    cairo_rectangle(cr, x, y, width, height);
    cairo_set_source_rgb(cr, 0.09, 0.10, 0.13);
    cairo_fill(cr);

    cairo_rectangle(cr, x, y, width, height);
    cairo_set_source_rgb(cr, 0.30, 0.33, 0.38);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.90, 0.90, 0.90);
    cairo_move_to(cr, x + 8, y + 14);
    cairo_show_text(cr, "PATTERN");

    for (lane = 0; lane < PREVIEW_LANES; ++lane) {
        const int row_y = inner_y + lane * row_h;
        cairo_text_extents_t ext;
        cairo_set_source_rgb(cr, 0.90, 0.90, 0.90);
        cairo_text_extents(cr, kLaneNames[lane], &ext);
        cairo_move_to(cr, x + 8, row_y + row_h / 2.0 - ext.y_bearing / 2.0);
        cairo_show_text(cr, kLaneNames[lane]);

        cairo_set_source_rgba(cr, 0.65, 0.67, 0.72, 0.16);
        cairo_move_to(cr, inner_x, row_y);
        cairo_line_to(cr, inner_x + inner_w, row_y);
        cairo_stroke(cr);

        for (step = 0; step < pattern.total_steps; ++step) {
            const double t0 = (double)step / (double)pattern.total_steps;
            const double t1 = (double)(step + 1) / (double)pattern.total_steps;
            const int cell_x = inner_x + (int)floor(t0 * inner_w);
            const int cell_w = (int)floor(t1 * inner_w) - cell_x;
            const int cell_y = row_y + 2;
            const int cell_h = row_h - 4;
            const int step_in_bar = step % pattern.steps_per_bar;
            const bool beat_div = (step_in_bar % (pattern.steps_per_bar / 4 == 0 ? 1 : pattern.steps_per_bar / 4)) == 0;
            const bool bar_div = step_in_bar == 0;

            if (bar_div) {
                cairo_set_source_rgba(cr, 0.88, 0.88, 0.88, 0.35);
                cairo_move_to(cr, cell_x, inner_y);
                cairo_line_to(cr, cell_x, inner_y + inner_h);
                cairo_stroke(cr);
            } else if (beat_div) {
                cairo_set_source_rgba(cr, 0.72, 0.72, 0.72, 0.18);
                cairo_move_to(cr, cell_x, inner_y);
                cairo_line_to(cr, cell_x, inner_y + inner_h);
                cairo_stroke(cr);
            }

            cairo_rectangle(cr, cell_x + 1, cell_y, cell_w - 2 > 1 ? cell_w - 2 : 1, cell_h);
            if (pattern.hits[lane][step]) {
                const double lane_bias = 0.18 + 0.08 * lane;
                cairo_set_source_rgb(cr, 0.92, 0.42 + lane_bias * 0.3, 0.16 + lane_bias * 0.2);
            } else {
                cairo_set_source_rgb(cr, 0.15, 0.16, 0.19);
            }
            cairo_fill(cr);
        }
    }

    cairo_set_source_rgba(cr, 0.70, 0.70, 0.70, 0.20);
    cairo_rectangle(cr, inner_x, inner_y, inner_w, inner_h);
    cairo_stroke(cr);
}

static void draw_ui(DrumGenUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);
    char status[224];
    int i;

    cairo_set_source_rgb(cr, 0.08, 0.08, 0.10);
    cairo_paint(cr);

    for (i = 0; i < GROUP_COUNT; ++i) {
        draw_group(cr, &ui->groups[i], kGroupNames[i]);
    }

    for (i = 0; i < ui->slider_count; ++i) {
        draw_slider(cr, &ui->sliders[i]);
    }
    for (i = 0; i < ui->selector_count; ++i) {
        draw_selector(cr, &ui->selectors[i]);
    }
    for (i = 0; i < 3; ++i) {
        draw_action(cr, &ui->actions[i]);
    }
    if (ui->open_selector >= 0) {
        draw_selector_open(cr, ui, &ui->selectors[ui->open_selector]);
    }

    draw_preview(cr,
                 ui,
                 ui->groups[GROUP_PREVIEW].x + 12,
                 ui->groups[GROUP_PREVIEW].y + 30,
                 ui->groups[GROUP_PREVIEW].width - 24,
                 ui->groups[GROUP_PREVIEW].height - 42);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.88, 0.88, 0.88);
    snprintf(status, sizeof(status),
             "Genre %s  Ch %s  Map %s  Bars %s  Res %s  Density %.2f  Variation %.2f  Fill %.2f  Vary %d%%",
             ui->selectors[0].items[ui->selectors[0].value],
             ui->selectors[1].items[ui->selectors[1].value],
             ui->selectors[2].items[ui->selectors[2].value],
             ui->selectors[3].items[ui->selectors[3].value],
             ui->selectors[4].items[ui->selectors[4].value],
             ui->sliders[0].value,
             ui->sliders[1].value,
             ui->sliders[2].value,
             (int)lroundf(ui->sliders[3].value));
    cairo_move_to(cr, 16, ui->height - 12);
    cairo_show_text(cr, status);

    cairo_destroy(cr);
}

static void setup_layout(DrumGenUI* ui) {
    Slider* s;
    Selector* sel;

    ui->width = 940;
    ui->height = 650;

    ui->groups[GROUP_GLOBAL] = (GroupRect){10, 10, 430, 150};
    ui->groups[GROUP_FEEL] = (GroupRect){10, 170, 430, 170};
    ui->groups[GROUP_LANES] = (GroupRect){450, 10, 480, 170};
    ui->groups[GROUP_ACTIONS] = (GroupRect){450, 190, 480, 150};
    ui->groups[GROUP_PREVIEW] = (GroupRect){10, 350, 920, 280};

    ui->slider_count = 0;
    ui->selector_count = 0;

    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_DENSITY; s->label = "DENS"; s->min = 0.0f; s->max = 1.0f; s->value = 0.58f; s->is_int = false;
    s->x = ui->groups[GROUP_FEEL].x + 24; s->y = ui->groups[GROUP_FEEL].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_VARIATION; s->label = "VAR"; s->min = 0.0f; s->max = 1.0f; s->value = 0.35f; s->is_int = false;
    s->x = ui->groups[GROUP_FEEL].x + 88; s->y = ui->groups[GROUP_FEEL].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_FILL; s->label = "FILL"; s->min = 0.0f; s->max = 1.0f; s->value = 0.30f; s->is_int = false;
    s->x = ui->groups[GROUP_FEEL].x + 152; s->y = ui->groups[GROUP_FEEL].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_VARY; s->label = "VARY"; s->min = 0.0f; s->max = 100.0f; s->value = 0.0f; s->is_int = true;
    s->x = ui->groups[GROUP_FEEL].x + 216; s->y = ui->groups[GROUP_FEEL].y + 34; s->width = 42; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_SEED; s->label = "SEED"; s->min = 0.0f; s->max = 65535.0f; s->value = 1.0f; s->is_int = true;
    s->x = ui->groups[GROUP_FEEL].x + 288; s->y = ui->groups[GROUP_FEEL].y + 34; s->width = 56; s->height = 100;

    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_KICK_AMT; s->label = "KICK"; s->min = 0.0f; s->max = 1.0f; s->value = 0.78f; s->is_int = false;
    s->x = ui->groups[GROUP_LANES].x + 24; s->y = ui->groups[GROUP_LANES].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_BACKBEAT_AMT; s->label = "BACK"; s->min = 0.0f; s->max = 1.0f; s->value = 0.76f; s->is_int = false;
    s->x = ui->groups[GROUP_LANES].x + 88; s->y = ui->groups[GROUP_LANES].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_HAT_AMT; s->label = "HAT"; s->min = 0.0f; s->max = 1.0f; s->value = 0.82f; s->is_int = false;
    s->x = ui->groups[GROUP_LANES].x + 152; s->y = ui->groups[GROUP_LANES].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_TOM_AMT; s->label = "TOM"; s->min = 0.0f; s->max = 1.0f; s->value = 0.30f; s->is_int = false;
    s->x = ui->groups[GROUP_LANES].x + 216; s->y = ui->groups[GROUP_LANES].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_METAL_AMT; s->label = "METAL"; s->min = 0.0f; s->max = 1.0f; s->value = 0.26f; s->is_int = false;
    s->x = ui->groups[GROUP_LANES].x + 280; s->y = ui->groups[GROUP_LANES].y + 34; s->width = 30; s->height = 100;
    s = &ui->sliders[ui->slider_count++];
    s->port = PORT_AUX_AMT; s->label = "PERC"; s->min = 0.0f; s->max = 1.0f; s->value = 0.28f; s->is_int = false;
    s->x = ui->groups[GROUP_LANES].x + 344; s->y = ui->groups[GROUP_LANES].y + 34; s->width = 30; s->height = 100;

    sel = &ui->selectors[ui->selector_count++];
    sel->port = PORT_GENRE; sel->label = "Genre"; sel->items = kGenreNames; sel->count = 8; sel->value = 0; sel->host_offset = 0; sel->open = false; sel->item_height = 20;
    sel->x = ui->groups[GROUP_GLOBAL].x + 18; sel->y = ui->groups[GROUP_GLOBAL].y + 34; sel->width = 192; sel->height = 24;
    sel = &ui->selectors[ui->selector_count++];
    sel->port = PORT_CHANNEL; sel->label = "Channel"; sel->items = kChannelNames; sel->count = 16; sel->value = 9; sel->host_offset = 1; sel->open = false; sel->item_height = 18;
    sel->x = ui->groups[GROUP_GLOBAL].x + 220; sel->y = ui->groups[GROUP_GLOBAL].y + 34; sel->width = 192; sel->height = 24;
    sel = &ui->selectors[ui->selector_count++];
    sel->port = PORT_KIT_MAP; sel->label = "Map"; sel->items = kKitMapNames; sel->count = 2; sel->value = 0; sel->host_offset = 0; sel->open = false; sel->item_height = 20;
    sel->x = ui->groups[GROUP_GLOBAL].x + 18; sel->y = ui->groups[GROUP_GLOBAL].y + 70; sel->width = 192; sel->height = 24;
    sel = &ui->selectors[ui->selector_count++];
    sel->port = PORT_BARS; sel->label = "Bars"; sel->items = kBarNames; sel->count = 4; sel->value = 1; sel->host_offset = 1; sel->open = false; sel->item_height = 18;
    sel->x = ui->groups[GROUP_GLOBAL].x + 220; sel->y = ui->groups[GROUP_GLOBAL].y + 70; sel->width = 192; sel->height = 24;
    sel = &ui->selectors[ui->selector_count++];
    sel->port = PORT_RESOLUTION; sel->label = "Resolution"; sel->items = kResolutionNames; sel->count = 3; sel->value = 1; sel->host_offset = 0; sel->open = false; sel->item_height = 20;
    sel->x = ui->groups[GROUP_GLOBAL].x + 18; sel->y = ui->groups[GROUP_GLOBAL].y + 106; sel->width = 394; sel->height = 24;

    ui->actions[0] = (ActionButton){PORT_ACTION_NEW, "NEW", 0, ui->groups[GROUP_ACTIONS].x + 24, ui->groups[GROUP_ACTIONS].y + 36, 124, 38};
    ui->actions[1] = (ActionButton){PORT_ACTION_MUTATE, "MUTATE", 0, ui->groups[GROUP_ACTIONS].x + 178, ui->groups[GROUP_ACTIONS].y + 36, 124, 38};
    ui->actions[2] = (ActionButton){PORT_ACTION_FILL, "FILL", 0, ui->groups[GROUP_ACTIONS].x + 332, ui->groups[GROUP_ACTIONS].y + 36, 124, 38};

    ui->open_selector = -1;
    ui->active_slider = -1;
}

static void handle_motion(DrumGenUI* ui, XMotionEvent* ev) {
    if (ui->active_slider >= 0) {
        Slider* s = &ui->sliders[ui->active_slider];
        set_slider_value(ui, s, slider_value_from_y(s, ev->y), true);
    }
}

static void handle_press(DrumGenUI* ui, XButtonEvent* ev) {
    int sel_index;
    ActionButton* action;
    int slider_index;

    if (ev->button != Button1) {
        return;
    }

    if (ui->open_selector >= 0) {
        Selector* open = &ui->selectors[ui->open_selector];
        const int item = selector_menu_item_at_ui(ui, open, ev->x, ev->y);
        if (item >= 0) {
            open->value = item;
            notify_host(ui, open->port, (float)(open->value + open->host_offset));
            open->open = false;
            ui->open_selector = -1;
            ui->needs_redraw = true;
            return;
        }
        if (!point_in_rect(ev->x, ev->y, open->x, open->y, open->width, open->height)) {
            open->open = false;
            ui->open_selector = -1;
            ui->needs_redraw = true;
        }
    }

    sel_index = selector_index_at(ui, ev->x, ev->y);
    if (sel_index >= 0) {
        if (ui->open_selector >= 0 && ui->open_selector != sel_index) {
            ui->selectors[ui->open_selector].open = false;
        }
        ui->selectors[sel_index].open = !ui->selectors[sel_index].open;
        ui->open_selector = ui->selectors[sel_index].open ? sel_index : -1;
        ui->needs_redraw = true;
        return;
    }

    action = action_at(ui, ev->x, ev->y);
    if (action) {
        action->counter += 1;
        notify_host(ui, action->port, (float)action->counter);
        ui->needs_redraw = true;
        return;
    }

    slider_index = slider_at(ui, ev->x, ev->y);
    if (slider_index >= 0) {
        ui->active_slider = slider_index;
        set_slider_value(ui, &ui->sliders[slider_index], slider_value_from_y(&ui->sliders[slider_index], ev->y), true);
    }
}

static void handle_release(DrumGenUI* ui, XButtonEvent* ev) {
    if (ev->button == Button1) {
        ui->active_slider = -1;
    }
}

static void* event_thread_main(void* arg) {
    DrumGenUI* ui = (DrumGenUI*)arg;
    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);

            pthread_mutex_lock(&ui->mutex);
            switch (ev.type) {
                case Expose:
                case ConfigureNotify:
                    ui->needs_redraw = true;
                    break;
                case MotionNotify:
                    handle_motion(ui, &ev.xmotion);
                    break;
                case ButtonPress:
                    handle_press(ui, &ev.xbutton);
                    break;
                case ButtonRelease:
                    handle_release(ui, &ev.xbutton);
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&ui->mutex);
        }

        if (ui->needs_redraw) {
            pthread_mutex_lock(&ui->mutex);
            draw_ui(ui);
            ui->needs_redraw = false;
            pthread_mutex_unlock(&ui->mutex);
        }
        usleep(16000);
    }
    return NULL;
}

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor*,
                                   const char*,
                                   const char*,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    DrumGenUI* ui;
    Window parent;
    int i;
    XSetWindowAttributes attrs;
    Cursor hand_cursor;

    ensure_xlib_threads();

    ui = (DrumGenUI*)calloc(1, sizeof(DrumGenUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    ui->screen = DefaultScreen(ui->display);
    parent = DefaultRootWindow(ui->display);
    for (i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    setup_layout(ui);

    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display, parent, 0, 0, ui->width, ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attrs);

    hand_cursor = XCreateFontCursor(ui->display, XC_hand2);
    XDefineCursor(ui->display, ui->window, hand_cursor);
    XMapWindow(ui->display, ui->window);

    ui->surface = cairo_xlib_surface_create(ui->display, ui->window,
                                            DefaultVisual(ui->display, ui->screen),
                                            ui->width, ui->height);

    ui->running = true;
    ui->needs_redraw = true;
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    DrumGenUI* ui = (DrumGenUI*)handle;
    if (!ui) {
        return;
    }
    ui->running = false;
    pthread_join(ui->thread, NULL);
    if (ui->surface) {
        cairo_surface_destroy(ui->surface);
    }
    if (ui->window) {
        XDestroyWindow(ui->display, ui->window);
    }
    if (ui->display) {
        XCloseDisplay(ui->display);
    }
    pthread_mutex_destroy(&ui->mutex);
    free(ui);
}

static void ui_port_event(LV2UI_Handle handle,
                          uint32_t port_index,
                          uint32_t buffer_size,
                          uint32_t format,
                          const void* buffer) {
    DrumGenUI* ui = (DrumGenUI*)handle;
    float value;
    int i;

    if (!ui || !buffer || format != 0) {
        return;
    }
    (void)buffer_size;

    value = *(const float*)buffer;
    pthread_mutex_lock(&ui->mutex);

    for (i = 0; i < ui->slider_count; ++i) {
        if (ui->sliders[i].port == port_index) {
            set_slider_value(ui, &ui->sliders[i], value, false);
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    for (i = 0; i < ui->selector_count; ++i) {
        if (ui->selectors[i].port == port_index) {
            int v = (int)lroundf(value) - ui->selectors[i].host_offset;
            if (v < 0) v = 0;
            if (v >= ui->selectors[i].count) v = ui->selectors[i].count - 1;
            ui->selectors[i].value = v;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    for (i = 0; i < 3; ++i) {
        if (ui->actions[i].port == port_index) {
            ui->actions[i].counter = (int)lroundf(value);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    pthread_mutex_unlock(&ui->mutex);
}

static const LV2UI_Descriptor descriptor = {
    UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    NULL
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
