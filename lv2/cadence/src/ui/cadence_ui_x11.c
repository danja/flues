#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CADENCE_URI "https://danja.github.io/flues/plugins/cadence"
#define CADENCE_UI_URI CADENCE_URI "#ui"

#define WINDOW_WIDTH 1080
#define WINDOW_HEIGHT 660

#define MARGIN 20
#define TITLE_HEIGHT 26
#define SLIDER_WIDTH 26
#define SLIDER_HEIGHT 132
#define KNOB_HEIGHT 16
#define COLUMN_WIDTH 168
#define ROW_GAP 116
#define BUTTON_WIDTH 150
#define BUTTON_HEIGHT 38
#define LED_WIDTH 150
#define LED_HEIGHT 38
#define CONTROL_COUNT 12
#define CONTROLS_PER_ROW 6

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_MIDI_IN,
    PORT_MIDI_OUT,
    PORT_KEY,
    PORT_SCALE,
    PORT_CYCLE_BARS,
    PORT_GRANULARITY,
    PORT_COMPLEXITY,
    PORT_MOVEMENT,
    PORT_CHORD_SIZE,
    PORT_NOTE_LENGTH,
    PORT_REGISTER,
    PORT_SPREAD,
    PORT_PASS_INPUT,
    PORT_OUTPUT_CHANNEL,
    PORT_ACTION_LEARN,
    PORT_STATUS_READY
};

typedef struct {
    uint32_t port;
    const char* label;
    float min_value;
    float max_value;
    float default_value;
    bool is_integer;
} ControlDef;

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

    float values[CONTROL_COUNT];
    float learn_counter;
    float ready_value;
    int active_control;
} CadenceUI;

static const char* kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

static const char* kScaleNames[] = {
    "Chrom", "Major", "Nat Min", "Harm Min", "Pent Maj", "Pent Min",
    "Blues", "Dorian", "Mixolyd", "Phryg", "Locrian", "Phryg Dom"
};

static const char* kGranularityNames[] = {
    "Beat", "Half Bar", "Bar"
};

static const char* kChordSizeNames[] = {
    "Triads", "Sevenths"
};

static const char* kRegisterNames[] = {
    "Low", "Mid", "High"
};

static const char* kSpreadNames[] = {
    "Close", "Open", "Drop-2"
};

static const char* kToggleNames[] = {
    "Off", "On"
};

static const ControlDef kControls[CONTROL_COUNT] = {
    {PORT_KEY, "KEY", 0.0f, 11.0f, 0.0f, true},
    {PORT_SCALE, "SCALE", 0.0f, 11.0f, 2.0f, true},
    {PORT_CYCLE_BARS, "BARS", 1.0f, 8.0f, 2.0f, true},
    {PORT_GRANULARITY, "GRID", 0.0f, 2.0f, 1.0f, true},
    {PORT_COMPLEXITY, "COMPLEXITY", 0.0f, 1.0f, 0.45f, false},
    {PORT_MOVEMENT, "MOVE", 0.0f, 1.0f, 0.65f, false},
    {PORT_CHORD_SIZE, "SIZE", 0.0f, 1.0f, 0.0f, true},
    {PORT_NOTE_LENGTH, "LENGTH", 0.10f, 1.0f, 1.0f, false},
    {PORT_REGISTER, "REG", 0.0f, 2.0f, 1.0f, true},
    {PORT_SPREAD, "VOICE", 0.0f, 2.0f, 0.0f, true},
    {PORT_PASS_INPUT, "PASS", 0.0f, 1.0f, 1.0f, true},
    {PORT_OUTPUT_CHANNEL, "CHAN", 0.0f, 16.0f, 0.0f, true}
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

static float clamp_value(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int row_y(int row) {
    return MARGIN + TITLE_HEIGHT + 74 + row * (SLIDER_HEIGHT + ROW_GAP);
}

static int controls_group_width(void) {
    return (CONTROLS_PER_ROW * COLUMN_WIDTH);
}

static int column_x(int col, int width) {
    int start = (width - controls_group_width()) / 2;
    return start + col * COLUMN_WIDTH;
}

static int slider_x(int col, int width) {
    return column_x(col, width) + (COLUMN_WIDTH - SLIDER_WIDTH) / 2;
}

static int button_x(int width) {
    return (width / 2) - BUTTON_WIDTH - 18;
}

static int led_x(int width) {
    return (width / 2) + 18;
}

static int footer_y(int height) {
    return height - MARGIN - BUTTON_HEIGHT - 10;
}

static void send_value(CadenceUI* ui, uint32_t port, float value) {
    if (!ui->write) {
        return;
    }
    ui->write(ui->controller, port, sizeof(float), 0, &value);
}

static void draw_slider(cairo_t* cr, int x, int y, float value, float min_value, float max_value) {
    cairo_set_source_rgb(cr, 0.16, 0.18, 0.22);
    cairo_rectangle(cr, x, y, SLIDER_WIDTH, SLIDER_HEIGHT);
    cairo_fill(cr);

    float denom = max_value - min_value;
    float t = (denom <= 0.0f) ? 0.0f : (value - min_value) / denom;
    t = clamp_value(t, 0.0f, 1.0f);
    float knob_y = y + (1.0f - t) * (SLIDER_HEIGHT - KNOB_HEIGHT);

    cairo_set_source_rgb(cr, 0.94, 0.73, 0.28);
    cairo_rectangle(cr, x - 2, knob_y, SLIDER_WIDTH + 4, KNOB_HEIGHT);
    cairo_fill(cr);
}

static const char* label_from_index(const char* const* labels, int count, int index) {
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;
    return labels[index];
}

static void format_value_label(int control_index, float value, char* out, size_t out_size) {
    switch (control_index) {
        case 0:
            snprintf(out, out_size, "%s", label_from_index(kNoteNames, 12, (int)lroundf(value)));
            break;
        case 1:
            snprintf(out, out_size, "%s", label_from_index(kScaleNames, 12, (int)lroundf(value)));
            break;
        case 2:
            snprintf(out, out_size, "%d", (int)lroundf(value));
            break;
        case 3:
            snprintf(out, out_size, "%s", label_from_index(kGranularityNames, 3, (int)lroundf(value)));
            break;
        case 4:
            snprintf(out, out_size, "%.2f", value);
            break;
        case 5:
            snprintf(out, out_size, "%.2f", value);
            break;
        case 6:
            snprintf(out, out_size, "%s", label_from_index(kChordSizeNames, 2, (int)lroundf(value)));
            break;
        case 7:
            snprintf(out, out_size, "%d%%", (int)lroundf(value * 100.0f));
            break;
        case 8:
            snprintf(out, out_size, "%s", label_from_index(kRegisterNames, 3, (int)lroundf(value)));
            break;
        case 9:
            snprintf(out, out_size, "%s", label_from_index(kSpreadNames, 3, (int)lroundf(value)));
            break;
        case 10:
            snprintf(out, out_size, "%s", label_from_index(kToggleNames, 2, (int)lroundf(value)));
            break;
        case 11:
            if ((int)lroundf(value) <= 0) {
                snprintf(out, out_size, "Input");
            } else {
                snprintf(out, out_size, "%d", (int)lroundf(value));
            }
            break;
        default:
            snprintf(out, out_size, "%.2f", value);
            break;
    }
}

static void draw_centered_text(cairo_t* cr, double cx, double y, const char* text, double size, bool bold, double r, double g, double b) {
    cairo_text_extents_t extents;
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, text, &extents);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, cx - (extents.width * 0.5) - extents.x_bearing, y);
    cairo_show_text(cr, text);
}

static void draw_ui(CadenceUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.07, 0.08, 0.10);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.14, 0.15, 0.18);
    cairo_rectangle(cr, MARGIN, MARGIN, ui->width - (MARGIN * 2), ui->height - (MARGIN * 2));
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.94, 0.73, 0.28);
    cairo_rectangle(cr, MARGIN, MARGIN, ui->width - (MARGIN * 2), 4);
    cairo_fill(cr);

    draw_centered_text(cr, ui->width * 0.5, MARGIN + 22, "CADENCE", 18.0, true, 0.94, 0.73, 0.28);
    draw_centered_text(cr, ui->width * 0.5, MARGIN + 42, "Cycle-Learned MIDI Harmonizer", 11.0, false, 0.78, 0.80, 0.85);

    for (int row = 0; row < 2; ++row) {
        const int gy = row_y(row) - 58;
        cairo_set_source_rgb(cr, 0.11, 0.12, 0.15);
        cairo_rectangle(cr, column_x(0, ui->width) - 12, gy, controls_group_width() + 24, SLIDER_HEIGHT + 98);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.23, 0.24, 0.28);
        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, column_x(0, ui->width) - 11.5, gy + 0.5, controls_group_width() + 23, SLIDER_HEIGHT + 97);
        cairo_stroke(cr);
        draw_centered_text(cr,
                           ui->width * 0.5,
                           gy + 18,
                           row == 0 ? "CONTEXT" : "VOICING AND ROUTING",
                           11.0,
                           true,
                           0.72, 0.75, 0.80);
    }

    for (int i = 0; i < CONTROL_COUNT; ++i) {
        int row = i / CONTROLS_PER_ROW;
        int col = i % CONTROLS_PER_ROW;
        int sx = slider_x(col, ui->width);
        int sy = row_y(row);
        int cx = column_x(col, ui->width) + COLUMN_WIDTH / 2;
        char value_label[32];

        draw_slider(cr, sx, sy, ui->values[i], kControls[i].min_value, kControls[i].max_value);
        draw_centered_text(cr, cx, sy - 18, kControls[i].label, 11.0, true, 0.83, 0.85, 0.90);

        format_value_label(i, ui->values[i], value_label, sizeof(value_label));
        draw_centered_text(cr, cx, sy + SLIDER_HEIGHT + 26, value_label, 11.0, false, 0.84, 0.86, 0.92);
    }

    const int by = footer_y(ui->height);
    const int bx = button_x(ui->width);
    const int lx = led_x(ui->width);
    const bool ready = ui->ready_value >= 0.5f;

    cairo_set_source_rgb(cr, 0.21, 0.24, 0.28);
    cairo_rectangle(cr, bx, by, BUTTON_WIDTH, BUTTON_HEIGHT);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.94, 0.73, 0.28);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, bx + 0.5, by + 0.5, BUTTON_WIDTH - 1.0, BUTTON_HEIGHT - 1.0);
    cairo_stroke(cr);
    draw_centered_text(cr, bx + (BUTTON_WIDTH * 0.5), by + 21, "LEARN", 12.0, true, 0.94, 0.73, 0.28);

    cairo_set_source_rgb(cr, ready ? 0.18 : 0.18, ready ? 0.62 : 0.18, ready ? 0.29 : 0.20);
    cairo_rectangle(cr, lx, by, LED_WIDTH, LED_HEIGHT);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, ready ? 0.55 : 0.35, ready ? 0.92 : 0.35, ready ? 0.66 : 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, lx + 0.5, by + 0.5, LED_WIDTH - 1.0, LED_HEIGHT - 1.0);
    cairo_stroke(cr);
    draw_centered_text(cr, lx + (LED_WIDTH * 0.5), by + 21, ready ? "READY" : "LEARNING", 12.0, true,
                       ready ? 0.88 : 0.70, ready ? 0.96 : 0.72, ready ? 0.90 : 0.76);

    draw_centered_text(cr, ui->width * 0.5, ui->height - 12, "Generated chords retrigger at each segment boundary; CHAN=Input follows the source channel",
                       10.0, false, 0.66, 0.69, 0.74);

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static bool slider_hit(int x, int y, int sx, int sy) {
    return (x >= sx - 6 && x <= sx + SLIDER_WIDTH + 6 &&
            y >= sy - 6 && y <= sy + SLIDER_HEIGHT + 6);
}

static float value_from_y(int y, int sy, float min_value, float max_value) {
    float t = 1.0f - ((float)(y - sy) / (float)(SLIDER_HEIGHT - KNOB_HEIGHT));
    t = clamp_value(t, 0.0f, 1.0f);
    return min_value + t * (max_value - min_value);
}

static void update_control_from_y(CadenceUI* ui, int control_index, int y) {
    const int row = control_index / CONTROLS_PER_ROW;
    const int sy = row_y(row);
    float value = value_from_y(y, sy, kControls[control_index].min_value, kControls[control_index].max_value);
    if (kControls[control_index].is_integer) {
        value = roundf(value);
    }
    value = clamp_value(value, kControls[control_index].min_value, kControls[control_index].max_value);
    ui->values[control_index] = value;
    send_value(ui, kControls[control_index].port, value);
    ui->needs_redraw = true;
}

static bool button_hit(CadenceUI* ui, int x, int y) {
    const int bx = button_x(ui->width);
    const int by = footer_y(ui->height);
    return x >= bx && x <= bx + BUTTON_WIDTH && y >= by && y <= by + BUTTON_HEIGHT;
}

static void handle_button_press(CadenceUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);

    ui->active_control = -1;
    for (int i = 0; i < CONTROL_COUNT; ++i) {
        const int row = i / CONTROLS_PER_ROW;
        const int col = i % CONTROLS_PER_ROW;
        if (slider_hit(ev->x, ev->y, slider_x(col, ui->width), row_y(row))) {
            ui->active_control = i;
            update_control_from_y(ui, i, ev->y);
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    if (button_hit(ui, ev->x, ev->y)) {
        ui->learn_counter += 1.0f;
        send_value(ui, PORT_ACTION_LEARN, ui->learn_counter);
        ui->ready_value = 0.0f;
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(CadenceUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->active_control = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(CadenceUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_control >= 0) {
        update_control_from_y(ui, ui->active_control, ev->y);
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* data) {
    CadenceUI* ui = (CadenceUI*)data;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);
            switch (ev.type) {
                case Expose:
                    ui->needs_redraw = true;
                    break;
                case ConfigureNotify:
                    pthread_mutex_lock(&ui->mutex);
                    if (ev.xconfigure.width != ui->width || ev.xconfigure.height != ui->height) {
                        ui->width = ev.xconfigure.width;
                        ui->height = ev.xconfigure.height;
                        cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);
                        ui->needs_redraw = true;
                    }
                    pthread_mutex_unlock(&ui->mutex);
                    break;
                case DestroyNotify:
                    ui->running = false;
                    break;
                case ButtonPress:
                    handle_button_press(ui, &ev.xbutton);
                    break;
                case ButtonRelease:
                    handle_button_release(ui);
                    break;
                case MotionNotify:
                    handle_motion(ui, &ev.xmotion);
                    break;
                default:
                    break;
            }
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

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor* /*descriptor*/,
                                   const char* /*plugin_uri*/,
                                   const char* /*bundle_path*/,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    ensure_xlib_threads();

    CadenceUI* ui = (CadenceUI*)calloc(1, sizeof(CadenceUI));
    if (!ui) {
        return NULL;
    }

    ui->write = write_function;
    ui->controller = controller;
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->active_control = -1;
    for (int i = 0; i < CONTROL_COUNT; ++i) {
        ui->values[i] = kControls[i].default_value;
    }

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "cadence-ui: no parent window provided\n");
        free(ui);
        return NULL;
    }

    Display* display = XOpenDisplay(NULL);
    if (!display) {
        free(ui);
        return NULL;
    }

    ui->display = display;
    ui->screen = DefaultScreen(display);

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(display, ui->screen);
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask;

    ui->window = XCreateWindow(display,
                               (Window)(uintptr_t)parent,
                               0, 0,
                               ui->width, ui->height,
                               0,
                               CopyFromParent,
                               InputOutput,
                               CopyFromParent,
                               CWBackPixel | CWEventMask,
                               &attrs);

    if (!ui->window) {
        XCloseDisplay(display);
        free(ui);
        return NULL;
    }

    XStoreName(display, ui->window, "Cadence");
    XMapWindow(display, ui->window);

    ui->surface = cairo_xlib_surface_create(display,
                                            ui->window,
                                            DefaultVisual(display, ui->screen),
                                            ui->width,
                                            ui->height);
    cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);

    pthread_mutex_init(&ui->mutex, NULL);
    ui->running = true;
    ui->needs_redraw = true;

    if (pthread_create(&ui->thread, NULL, event_thread_main, ui) != 0) {
        cairo_surface_destroy(ui->surface);
        XDestroyWindow(display, ui->window);
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    CadenceUI* ui = (CadenceUI*)handle;
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
    CadenceUI* ui = (CadenceUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    float value = *(const float*)buffer;

    for (int i = 0; i < CONTROL_COUNT; ++i) {
        if (port_index == kControls[i].port) {
            ui->values[i] = clamp_value(value, kControls[i].min_value, kControls[i].max_value);
            if (kControls[i].is_integer) {
                ui->values[i] = roundf(ui->values[i]);
            }
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    if (port_index == PORT_ACTION_LEARN) {
        ui->learn_counter = value;
        ui->needs_redraw = true;
    } else if (port_index == PORT_STATUS_READY) {
        ui->ready_value = clamp_value(value, 0.0f, 1.0f);
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    CADENCE_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
