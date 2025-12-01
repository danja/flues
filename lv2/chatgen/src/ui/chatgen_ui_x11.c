// chatgen_ui_x11.c - X11/Cairo UI for ChatGen
// Text input and phoneme preview display

#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

#define CHATGEN_UI_URI "https://danja.github.io/flues/plugins/chatgen#ui"

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 300
#define TEXT_BOX_HEIGHT 40
#define BUTTON_HEIGHT 30
#define MARGIN 20

// Port indices (must match chatgen.ttl)
enum {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT = 1,
    PORT_PLAY = 2,
    PORT_LOOP = 3,
    PORT_TEXT_OUT = 4
};

typedef struct {
    // X11/Cairo
    Display* display;
    Window window;
    cairo_surface_t* surface;
    int screen;

    // Threading
    pthread_t event_thread;
    pthread_mutex_t mutex;
    volatile bool running;

    // State
    char text[256];
    char phonemes[512];  // Preview string
    size_t cursor_pos;
    bool play;
    bool loop;
    bool text_changed;
    bool needs_redraw;
    bool user_is_editing;  // True while user is typing, false after pressing Enter

    // Cursor blink
    bool cursor_visible;
    uint64_t last_blink;

    // LV2 communication
    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;
    LV2_URID_Map* map;
    LV2UI_Port_Subscribe* port_subscribe;

    LV2_URID atomStringUrid;
    LV2_URID atomSequenceUrid;
    LV2_URID atomEventTransferUrid;
    LV2_Atom_Forge forge;

} ChatGenUI;

// Get current time in milliseconds
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// Update phoneme preview (matches DSP TextParser logic)
static void update_phoneme_preview(ChatGenUI* ui) {
    ui->phonemes[0] = '\0';
    char* out = ui->phonemes;
    const char* text = ui->text;
    const char* out_end = ui->phonemes + sizeof(ui->phonemes) - 10;

    size_t i = 0;
    size_t len = strlen(text);

    while (i < len && out < out_end) {
        char c = text[i];
        if (c >= 'A' && c <= 'Z') c += 32; // lowercase

        // Check digraphs first (2 characters)
        if (i + 1 < len) {
            char c2 = text[i+1];
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

            if (c == 'e' && c2 == 'e') { strcpy(out, "[i] "); out += 4; i += 2; continue; }
            if (c == 'e' && c2 == 'a') { strcpy(out, "[i] "); out += 4; i += 2; continue; }
            if (c == 'o' && c2 == 'o') { strcpy(out, "[u] "); out += 4; i += 2; continue; }
            if (c == 'a' && c2 == 'h') { strcpy(out, "[a] "); out += 4; i += 2; continue; }
            if (c == 'o' && c2 == 'h') { strcpy(out, "[o] "); out += 4; i += 2; continue; }
            if (c == 'a' && c2 == 'w') { strcpy(out, "[aw] "); out += 5; i += 2; continue; }
            if (c == 'u' && c2 == 'h') { strcpy(out, "[uh] "); out += 5; i += 2; continue; }
            if (c == 'e' && c2 == 'r') { strcpy(out, "[er] "); out += 5; i += 2; continue; }
            if (c == 'u' && c2 == 'r') { strcpy(out, "[er] "); out += 5; i += 2; continue; }
            if (c == 'i' && c2 == 'r') { strcpy(out, "[er] "); out += 5; i += 2; continue; }
            if (c == 'o' && c2 == 'u') { strcpy(out, "[uu] "); out += 5; i += 2; continue; }
            if (c == 's' && c2 == 'h') { strcpy(out, "[sh] "); out += 5; i += 2; continue; }
            if (c == 'c' && c2 == 'h') { strcpy(out, "[ch] "); out += 5; i += 2; continue; }
            if (c == 't' && c2 == 'h') { strcpy(out, "[th] "); out += 5; i += 2; continue; }
            if (c == 'd' && c2 == 'h') { strcpy(out, "[dh] "); out += 5; i += 2; continue; }
            if (c == 'z' && c2 == 'h') { strcpy(out, "[zh] "); out += 5; i += 2; continue; }
            if (c == 'n' && c2 == 'g') { strcpy(out, "[ng] "); out += 5; i += 2; continue; }
        }

        // Single character mapping (matches DSP phonemes)
        switch (c) {
            case 'a': strcpy(out, "[ae] "); out += 5; break;
            case 'e': strcpy(out, "[e] "); out += 4; break;
            case 'i': strcpy(out, "[ih] "); out += 5; break;
            case 'o': strcpy(out, "[o] "); out += 4; break;
            case 'u': strcpy(out, "[uh] "); out += 5; break;
            case 'p': case 'b': case 't': case 'd': case 'k': case 'g':
            case 'f': case 'v': case 's': case 'z': case 'h': case 'j':
            case 'm': case 'n': case 'l': case 'r': case 'w': case 'y': case 'c':
                *out++ = '['; *out++ = c; *out++ = ']'; *out++ = ' ';
                break;
            default:
                // Skip spaces, punctuation
                break;
        }
        i++;
    }
    *out = '\0';
}

// Send text to DSP
static void send_text_to_dsp(ChatGenUI* ui) {
    // Forge bare atom:String (host will wrap it in sequence automatically)
    uint8_t buf[1024];
    lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));

    // Forge string atom - returns offset, not pointer
    LV2_Atom_Forge_Ref ref = lv2_atom_forge_string(
        &ui->forge, ui->text, strlen(ui->text));

    if (ref) {
        // The atom is at the start of the buffer
        LV2_Atom* msg = (LV2_Atom*)buf;

        // Send with atomEventTransferUrid - host adds it to the control sequence
        ui->write_function(ui->controller, PORT_CONTROL,
                          lv2_atom_total_size(msg),
                          ui->atomEventTransferUrid, msg);
        fprintf(stderr, "ChatGen UI: Sent bare atom:String to DSP: '%s' (size=%u, type=%u)\n",
                ui->text, lv2_atom_total_size(msg), msg->type);
    } else {
        fprintf(stderr, "ChatGen UI: ERROR - Failed to forge string atom\n");
    }
}

// Render the UI
static void render_ui(ChatGenUI* ui) {
    if (!ui->surface || !ui->display) return;

    pthread_mutex_lock(&ui->mutex);
    ui->needs_redraw = false;
    pthread_mutex_unlock(&ui->mutex);

    cairo_t* cr = cairo_create(ui->surface);
    if (!cr) return;

    // Background
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_paint(cr);

    int y = MARGIN;

    // Title
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    cairo_move_to(cr, MARGIN, y + 16);
    cairo_show_text(cr, "ChatGen - Text to MIDI CC");

    y += 30;

    // Text input box
    cairo_set_source_rgb(cr, 0.25, 0.25, 0.25);
    cairo_rectangle(cr, MARGIN, y, WINDOW_WIDTH - 2*MARGIN, TEXT_BOX_HEIGHT);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, MARGIN, y, WINDOW_WIDTH - 2*MARGIN, TEXT_BOX_HEIGHT);
    cairo_stroke(cr);

    // Text content
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, MARGIN + 5, y + TEXT_BOX_HEIGHT/2 + 5);
    cairo_show_text(cr, ui->text);

    // Cursor
    if (ui->cursor_visible) {
        cairo_text_extents_t extents;
        char temp[256];
        strncpy(temp, ui->text, ui->cursor_pos);
        temp[ui->cursor_pos] = '\0';
        cairo_text_extents(cr, temp, &extents);

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, 2);
        double cursor_x = MARGIN + 5 + extents.x_advance;
        cairo_move_to(cr, cursor_x, y + 10);
        cairo_line_to(cr, cursor_x, y + TEXT_BOX_HEIGHT - 10);
        cairo_stroke(cr);
    }

    y += TEXT_BOX_HEIGHT + MARGIN/2;

    // Phoneme preview
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, MARGIN, y + 12);
    cairo_show_text(cr, "Phonemes:");

    y += 20;
    cairo_set_source_rgb(cr, 0.5, 0.8, 1.0);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, MARGIN, y + 14);
    cairo_show_text(cr, ui->phonemes);

    y += 40;

    // Play button
    cairo_set_source_rgb(cr, ui->play ? 0.3 : 0.2, ui->play ? 0.7 : 0.25, ui->play ? 0.3 : 0.2);
    cairo_rectangle(cr, MARGIN, y, 100, BUTTON_HEIGHT);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, MARGIN, y, 100, BUTTON_HEIGHT);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, MARGIN + 30, y + BUTTON_HEIGHT/2 + 5);
    cairo_show_text(cr, ui->play ? "Playing" : "Stopped");

    // Loop button
    cairo_set_source_rgb(cr, ui->loop ? 0.3 : 0.2, ui->loop ? 0.6 : 0.25, ui->loop ? 0.7 : 0.2);
    cairo_rectangle(cr, MARGIN + 120, y, 100, BUTTON_HEIGHT);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, MARGIN + 120, y, 100, BUTTON_HEIGHT);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_move_to(cr, MARGIN + 145, y + BUTTON_HEIGHT/2 + 5);
    cairo_show_text(cr, ui->loop ? "Loop: ON" : "Loop: OFF");

    y += BUTTON_HEIGHT + MARGIN;

    // Instructions
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, MARGIN, y + 10);
    cairo_show_text(cr, "Type text above. DAW transport controls playback.");

    cairo_destroy(cr);

    // Flush to display
    XFlush(ui->display);
}

// Handle keyboard input
static void handle_key_press(ChatGenUI* ui, XKeyEvent* event) {
    char buf[32];
    KeySym keysym;
    int len = XLookupString(event, buf, sizeof(buf), &keysym, NULL);

    if (len > 0 && buf[0] >= 32 && buf[0] < 127) {
        fprintf(stderr, "ChatGen UI: Key '%c'\n", buf[0]);
    } else {
        fprintf(stderr, "ChatGen UI: Special key 0x%lx\n", keysym);
    }

    pthread_mutex_lock(&ui->mutex);

    bool changed = false;
    bool sendToDSP = false;

    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        // Enter key - send current text to DSP
        sendToDSP = true;
    }
    else if (keysym == XK_BackSpace) {
        if (ui->cursor_pos > 0) {
            memmove(&ui->text[ui->cursor_pos - 1], &ui->text[ui->cursor_pos],
                    strlen(ui->text) - ui->cursor_pos + 1);
            ui->cursor_pos--;
            changed = true;
        }
    }
    else if (keysym == XK_Left) {
        if (ui->cursor_pos > 0) ui->cursor_pos--;
    }
    else if (keysym == XK_Right) {
        if (ui->cursor_pos < strlen(ui->text)) ui->cursor_pos++;
    }
    else if (keysym == XK_Home) {
        ui->cursor_pos = 0;
    }
    else if (keysym == XK_End) {
        ui->cursor_pos = strlen(ui->text);
    }
    else if (len > 0 && buf[0] >= 32 && buf[0] < 127) {
        // Printable character
        if (strlen(ui->text) < 255) {
            memmove(&ui->text[ui->cursor_pos + 1], &ui->text[ui->cursor_pos],
                    strlen(ui->text) - ui->cursor_pos + 1);
            ui->text[ui->cursor_pos] = buf[0];
            ui->cursor_pos++;
            changed = true;
        }
    }

    if (changed) {
        update_phoneme_preview(ui);
        ui->text_changed = true;
        ui->user_is_editing = true;  // User is actively editing
    }

    if (sendToDSP) {
        send_text_to_dsp(ui);
        ui->user_is_editing = false;  // Done editing, sent to DSP
        fprintf(stderr, "ChatGen UI: Sent text to DSP: '%s'\n", ui->text);
    }

    ui->needs_redraw = true;
    pthread_mutex_unlock(&ui->mutex);
}

// Handle mouse clicks
static void handle_button_press(ChatGenUI* ui, XButtonEvent* event) {
    fprintf(stderr, "ChatGen UI: Button press at (%d, %d)\n", event->x, event->y);

    pthread_mutex_lock(&ui->mutex);

    // Calculate positions matching render_ui()
    int text_box_y = MARGIN + 30;  // After title
    int button_y = text_box_y + TEXT_BOX_HEIGHT + MARGIN/2 + 20 + 40;  // 50 + 40 + 10 + 20 + 40 = 160

    // Text box click - set focus
    if (event->x >= MARGIN && event->x <= WINDOW_WIDTH - MARGIN &&
        event->y >= text_box_y && event->y <= text_box_y + TEXT_BOX_HEIGHT) {
        // Click in text box - request keyboard focus
        XSetInputFocus(ui->display, ui->window, RevertToParent, CurrentTime);
        fprintf(stderr, "ChatGen UI: Clicked in text box, requesting keyboard focus\n");
        ui->needs_redraw = true;
    }
    // Play button
    else if (event->x >= MARGIN && event->x <= MARGIN + 100 &&
        event->y >= button_y && event->y <= button_y + BUTTON_HEIGHT) {
        ui->play = !ui->play;
        ui->write_function(ui->controller, PORT_PLAY, sizeof(float), 0,
                          &(float){ui->play ? 1.0f : 0.0f});
        fprintf(stderr, "ChatGen UI: Play toggled to %s\n", ui->play ? "ON" : "OFF");
        ui->needs_redraw = true;
    }
    // Loop button
    else if (event->x >= MARGIN + 120 && event->x <= MARGIN + 220 &&
        event->y >= button_y && event->y <= button_y + BUTTON_HEIGHT) {
        ui->loop = !ui->loop;
        ui->write_function(ui->controller, PORT_LOOP, sizeof(float), 0,
                          &(float){ui->loop ? 1.0f : 0.0f});
        fprintf(stderr, "ChatGen UI: Loop toggled to %s\n", ui->loop ? "ON" : "OFF");
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

// Event thread
static void* event_thread_func(void* arg) {
    ChatGenUI* ui = (ChatGenUI*)arg;

    while (ui->running) {
        // Handle X11 events
        while (XPending(ui->display)) {
            XEvent event;
            XNextEvent(ui->display, &event);

            switch (event.type) {
                case KeyPress:
                    handle_key_press(ui, &event.xkey);
                    break;
                case ButtonPress:
                    handle_button_press(ui, &event.xbutton);
                    break;
                case Expose:
                    pthread_mutex_lock(&ui->mutex);
                    ui->needs_redraw = true;
                    pthread_mutex_unlock(&ui->mutex);
                    break;
                case ConfigureNotify:
                    pthread_mutex_lock(&ui->mutex);
                    if (event.xconfigure.width != WINDOW_WIDTH ||
                        event.xconfigure.height != WINDOW_HEIGHT) {
                        cairo_xlib_surface_set_size(ui->surface,
                                                    event.xconfigure.width,
                                                    event.xconfigure.height);
                        ui->needs_redraw = true;
                    }
                    pthread_mutex_unlock(&ui->mutex);
                    break;
            }
        }

        // Cursor blink
        uint64_t now = get_time_ms();
        if (now - ui->last_blink > 500) {
            pthread_mutex_lock(&ui->mutex);
            ui->cursor_visible = !ui->cursor_visible;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            ui->last_blink = now;
        }

        // Render if needed
        if (ui->needs_redraw) {
            render_ui(ui);
        }

        usleep(16000);  // ~60 FPS
    }

    return NULL;
}

// LV2 UI instantiate
static LV2UI_Handle instantiate(
    const LV2UI_Descriptor* descriptor,
    const char* plugin_uri,
    const char* bundle_path,
    LV2UI_Write_Function write_function,
    LV2UI_Controller controller,
    LV2UI_Widget* widget,
    const LV2_Feature* const* features)
{
    (void)descriptor;
    (void)plugin_uri;
    (void)bundle_path;

    // Get parent window and features
    void* parent = NULL;
    LV2_URID_Map* map = NULL;
    LV2UI_Port_Subscribe* port_subscribe = NULL;

    for (int i = 0; features[i]; i++) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = features[i]->data;
        } else if (!strcmp(features[i]->URI, LV2_URID__map)) {
            map = (LV2_URID_Map*)features[i]->data;
        } else if (!strcmp(features[i]->URI, LV2_UI__portSubscribe)) {
            port_subscribe = (LV2UI_Port_Subscribe*)features[i]->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "ChatGen UI: No parent window provided\n");
        return NULL;
    }

    if (!map) {
        fprintf(stderr, "ChatGen UI: No URID map feature\n");
        return NULL;
    }

    ChatGenUI* ui = (ChatGenUI*)calloc(1, sizeof(ChatGenUI));
    if (!ui) return NULL;

    ui->write_function = write_function;
    ui->controller = controller;
    ui->map = map;

    // Map URIDs
    ui->atomStringUrid = ui->map->map(ui->map->handle, LV2_ATOM__String);
    ui->atomSequenceUrid = ui->map->map(ui->map->handle, LV2_ATOM__Sequence);
    ui->atomEventTransferUrid = ui->map->map(ui->map->handle, LV2_ATOM__eventTransfer);

    // Initialize atom forge
    lv2_atom_forge_init(&ui->forge, ui->map);

    // Initialize state
    strcpy(ui->text, "hello world");
    ui->cursor_pos = strlen(ui->text);
    ui->play = true;   // Default to playing
    ui->loop = true;
    ui->cursor_visible = true;
    ui->needs_redraw = true;
    ui->user_is_editing = false;  // Not editing initially, will receive text from DSP
    ui->last_blink = get_time_ms();
    update_phoneme_preview(ui);

    // Initialize mutex
    pthread_mutex_init(&ui->mutex, NULL);

    // Initialize X11
    XInitThreads();
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, "ChatGen UI: Cannot open X display\n");
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    ui->screen = DefaultScreen(ui->display);
    Window parent_window = (Window)(uintptr_t)parent;

    // Create window as child of parent
    XSetWindowAttributes attr;
    attr.background_pixel = BlackPixel(ui->display, ui->screen);
    attr.event_mask = ExposureMask | StructureNotifyMask |
                     KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask;

    ui->window = XCreateWindow(ui->display, parent_window,
                               0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attr);

    // Create Cairo surface
    ui->surface = cairo_xlib_surface_create(
        ui->display, ui->window,
        DefaultVisual(ui->display, ui->screen),
        WINDOW_WIDTH, WINDOW_HEIGHT);

    XMapWindow(ui->display, ui->window);

    // Enable input focus for keyboard events
    XSetWMProtocols(ui->display, ui->window, NULL, 0);

    XFlush(ui->display);

    // Start event thread
    ui->running = true;
    pthread_create(&ui->event_thread, NULL, event_thread_func, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;

    // Send initial state to DSP
    float play_val = ui->play ? 1.0f : 0.0f;
    float loop_val = ui->loop ? 1.0f : 0.0f;
    ui->write_function(ui->controller, PORT_PLAY, sizeof(float), 0, &play_val);
    ui->write_function(ui->controller, PORT_LOOP, sizeof(float), 0, &loop_val);
    // DON'T send text on initialization - only send when user presses Enter
    // (prevents overwriting DSP text when UI is recreated on focus changes)

    // Subscribe to text output port so we receive text from DSP
    if (port_subscribe) {
        port_subscribe->subscribe(port_subscribe->handle, PORT_TEXT_OUT,
                                 ui->atomEventTransferUrid, NULL);
        fprintf(stderr, "ChatGen UI: Subscribed to text output port\n");
    } else {
        fprintf(stderr, "ChatGen UI: WARNING - No port subscribe feature, text persistence won't work\n");
    }

    // Store port_subscribe for cleanup
    ui->port_subscribe = port_subscribe;

    fprintf(stderr, "ChatGen UI: Initialized - Play: %s, Loop: %s, Text: '%s' (waiting for DSP text...)\n",
            ui->play ? "ON" : "OFF", ui->loop ? "ON" : "OFF", ui->text);

    return ui;
}

// LV2 UI cleanup
static void cleanup(LV2UI_Handle handle) {
    ChatGenUI* ui = (ChatGenUI*)handle;

    // Unsubscribe from text output port
    if (ui->port_subscribe) {
        ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, PORT_TEXT_OUT,
                                       ui->atomEventTransferUrid, NULL);
        fprintf(stderr, "ChatGen UI: Unsubscribed from text output port\n");
    }

    ui->running = false;
    pthread_join(ui->event_thread, NULL);
    pthread_mutex_destroy(&ui->mutex);

    if (ui->surface) {
        cairo_surface_destroy(ui->surface);
    }

    if (ui->display) {
        XDestroyWindow(ui->display, ui->window);
        XCloseDisplay(ui->display);
    }

    free(ui);
}

// LV2 UI port_event (receive updates from DSP)
static void port_event(
    LV2UI_Handle handle,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t format,
    const void* buffer)
{
    ChatGenUI* ui = (ChatGenUI*)handle;

    if (format == 0) {  // Float
        const float value = *(const float*)buffer;

        pthread_mutex_lock(&ui->mutex);

        if (port_index == PORT_PLAY) {
            ui->play = (value > 0.5f);
        }
        else if (port_index == PORT_LOOP) {
            ui->loop = (value > 0.5f);
        }

        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
    }
    else if (format == ui->atomEventTransferUrid && port_index == PORT_TEXT_OUT) {
        // Handle text from DSP (sent on every audio callback for persistence)
        const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)buffer;

        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type == ui->atomStringUrid) {
                const LV2_Atom_String* str = (const LV2_Atom_String*)&ev->body;
                const char* text = (const char*)(str + 1);

                pthread_mutex_lock(&ui->mutex);

                // Only update if user is NOT actively editing (prevent overwriting user input)
                // and text has actually changed (avoid unnecessary redraws)
                if (!ui->user_is_editing && strcmp(ui->text, text) != 0) {
                    strncpy(ui->text, text, sizeof(ui->text) - 1);
                    ui->text[sizeof(ui->text) - 1] = '\0';
                    ui->cursor_pos = strlen(ui->text);
                    update_phoneme_preview(ui);
                    ui->needs_redraw = true;
                    fprintf(stderr, "ChatGen UI: Text updated from DSP: '%s'\n", text);
                }

                pthread_mutex_unlock(&ui->mutex);
            }
        }
    }
}

static const LV2UI_Descriptor descriptor = {
    CHATGEN_UI_URI,
    instantiate,
    cleanup,
    port_event,
    NULL  // extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
