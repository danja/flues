// KeyboardInput.js
// Responsive on-screen + computer keyboard handler with MIDI-aware utilities.

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

const DEFAULT_KEY_MAP = {
    a: 0,
    w: 1,
    s: 2,
    e: 3,
    d: 4,
    f: 5,
    t: 6,
    g: 7,
    y: 8,
    h: 9,
    u: 10,
    j: 11,
    k: 12,
    o: 13,
    l: 14,
    p: 15,
    ';': 16,
    "'": 17,
    ']': 18,
    z: 19,
    x: 20,
    c: 21,
    v: 22,
    b: 23,
    n: 24,
};

export class KeyboardInput {
    constructor({
        container,
        onNoteOn,
        onNoteOff,
        baseMidiNote = 60, // Middle C (C4)
        keyMap = DEFAULT_KEY_MAP,
        velocity = 1,
    }) {
        if (!container) {
            throw new Error('[KeyboardInput] container element is required');
        }

        if (typeof onNoteOn !== 'function' || typeof onNoteOff !== 'function') {
            throw new Error('[KeyboardInput] onNoteOn and onNoteOff callbacks are required');
        }

        this.container = container;
        this.onNoteOn = onNoteOn;
        this.onNoteOff = onNoteOff;
        this.baseMidiNote = baseMidiNote;
        this.keyMap = keyMap;
        this.velocity = velocity;

        this.activeNotes = new Set();

        this._bindPointerEvents();
        this._bindKeyboardEvents();
    }

    static midiToFrequency(midiNote) {
        return 440 * Math.pow(2, (midiNote - 69) / 12);
    }

    static midiToName(midiNote) {
        const note = NOTE_NAMES[midiNote % 12];
        const octave = Math.floor(midiNote / 12) - 1;
        return `${note}${octave}`;
    }

    static nameToMidi(noteName) {
        const match = /^([A-Ga-g])([#b]?)(-?\d+)$/.exec(noteName);
        if (!match) return null;

        const [, base, accidental, octaveStr] = match;
        const octave = parseInt(octaveStr, 10);

        const normalizedBase = base.toUpperCase();
        let index = NOTE_NAMES.findIndex((n) => n.startsWith(normalizedBase));
        if (index < 0) return null;

        if (accidental === '#') {
            index = (index + 1) % 12;
        } else if (accidental === 'b') {
            index = (index + 11) % 12;
        }

        return index + (octave + 1) * 12;
    }

    _bindPointerEvents() {
        const keys = this.container.querySelectorAll('[data-midi], [data-note]');
        keys.forEach((keyEl) => {
            const identifier = keyEl.dataset.midi ?? keyEl.dataset.note;

            keyEl.addEventListener('mousedown', (event) => {
                event.preventDefault();
                this._press(identifier);
            });

            keyEl.addEventListener('mouseup', () => {
                this._release(identifier);
            });

            keyEl.addEventListener('mouseleave', () => {
                this._release(identifier);
            });

            keyEl.addEventListener('touchstart', (event) => {
                event.preventDefault();
                this._press(identifier);
            }, { passive: false });

            keyEl.addEventListener('touchend', (event) => {
                event.preventDefault();
                this._release(identifier);
            }, { passive: false });
        });
    }

    _bindKeyboardEvents() {
        document.addEventListener('keydown', (event) => {
            const offset = this.keyMap[event.key.toLowerCase()];
            if (offset === undefined) return;
            const midi = this.baseMidiNote + offset;
            if (!this.activeNotes.has(midi)) {
                this._triggerNoteOn(midi);
            }
        });

        document.addEventListener('keyup', (event) => {
            const offset = this.keyMap[event.key.toLowerCase()];
            if (offset === undefined) return;
            const midi = this.baseMidiNote + offset;
            this._triggerNoteOff(midi);
        });
    }

    _resolveMidi(identifier) {
        if (identifier === undefined) return null;
        if (identifier === null) return null;

        if (typeof identifier === 'number') {
            return identifier;
        }

        if (/^\d+$/.test(identifier)) {
            return parseInt(identifier, 10);
        }

        return KeyboardInput.nameToMidi(identifier);
    }

    _press(identifier) {
        const midi = this._resolveMidi(identifier);
        if (midi === null || this.activeNotes.has(midi)) {
            return;
        }
        this._triggerNoteOn(midi);
    }

    _release(identifier) {
        const midi = this._resolveMidi(identifier);
        if (midi === null) {
            return;
        }
        this._triggerNoteOff(midi);
    }

    _triggerNoteOn(midi) {
        this.activeNotes.add(midi);
        this._setKeyActiveClass(midi, true);

        const payload = {
            midi,
            name: KeyboardInput.midiToName(midi),
            frequency: KeyboardInput.midiToFrequency(midi),
            velocity: this.velocity,
        };

        try {
            this.onNoteOn(payload);
        } catch (error) {
            console.error('[KeyboardInput] onNoteOn callback failure', error);
        }
    }

    _triggerNoteOff(midi) {
        if (!this.activeNotes.has(midi)) return;

        this.activeNotes.delete(midi);
        this._setKeyActiveClass(midi, false);

        const payload = {
            midi,
            name: KeyboardInput.midiToName(midi),
        };

        try {
            this.onNoteOff(payload);
        } catch (error) {
            console.error('[KeyboardInput] onNoteOff callback failure', error);
        }
    }

    _setKeyActiveClass(midi, active) {
        const name = KeyboardInput.midiToName(midi);
        const selector = `[data-midi="${midi}"], [data-note="${name}"]`;
        const keyEl = this.container.querySelector(selector);
        if (keyEl) {
            keyEl.classList.toggle('active', active);
        }
    }

    releaseAll() {
        [...this.activeNotes].forEach((midi) => this._triggerNoteOff(midi));
    }
}

