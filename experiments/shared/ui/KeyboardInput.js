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
        this.pointerNotes = new Map();
        this.container.style.touchAction = 'none';

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
        this._onPointerDown = (event) => {
            const key = this._findKeyFromEvent(event);
            if (!key) return;
            event.preventDefault();
            this._triggerNoteOn(key.midi);
            this.pointerNotes.set(event.pointerId, key.midi);
        };

        this._onPointerMove = (event) => {
            if (!this.pointerNotes.has(event.pointerId)) return;
            const key = this._findKeyFromEvent(event);
            const currentMidi = this.pointerNotes.get(event.pointerId);

            if (!key) {
                if (currentMidi !== null && currentMidi !== undefined) {
                    this._triggerNoteOff(currentMidi);
                }
                this.pointerNotes.set(event.pointerId, null);
                return;
            }

            if (key.midi === currentMidi) return;

            if (currentMidi !== null && currentMidi !== undefined) {
                this._triggerNoteOff(currentMidi);
            }
            this._triggerNoteOn(key.midi);
            this.pointerNotes.set(event.pointerId, key.midi);
        };

        this._onPointerUp = (event) => {
            const midi = this.pointerNotes.get(event.pointerId);
            if (midi !== undefined && midi !== null) {
                this._triggerNoteOff(midi);
            }
            this.pointerNotes.delete(event.pointerId);
        };

        this._onPointerCancel = (event) => {
            const midi = this.pointerNotes.get(event.pointerId);
            if (midi !== undefined && midi !== null) {
                this._triggerNoteOff(midi);
            }
            this.pointerNotes.delete(event.pointerId);
        };

        this._onContextMenu = (event) => {
            event.preventDefault();
        };

        this.container.addEventListener('pointerdown', this._onPointerDown);
        window.addEventListener('pointermove', this._onPointerMove);
        window.addEventListener('pointerup', this._onPointerUp);
        window.addEventListener('pointercancel', this._onPointerCancel);
        this.container.addEventListener('contextmenu', this._onContextMenu);
    }

    _bindKeyboardEvents() {
        this._onKeyDown = (event) => {
            const offset = this.keyMap[event.key.toLowerCase()];
            if (offset === undefined) return;
            event.preventDefault();
            const midi = this.baseMidiNote + offset;
            if (!this.activeNotes.has(midi)) {
                this._triggerNoteOn(midi);
            }
        };

        this._onKeyUp = (event) => {
            const offset = this.keyMap[event.key.toLowerCase()];
            if (offset === undefined) return;
            event.preventDefault();
            const midi = this.baseMidiNote + offset;
            this._triggerNoteOff(midi);
        };

        document.addEventListener('keydown', this._onKeyDown);
        document.addEventListener('keyup', this._onKeyUp);
    }

    _findKeyFromEvent(event) {
        const target = event.target.closest('[data-midi], [data-note]');
        if (!target || !this.container.contains(target)) {
            return null;
        }

        const identifier = target.dataset.midi ?? target.dataset.note;
        const midi = this._resolveMidi(identifier);
        if (midi === null) {
            return null;
        }

        return { midi };
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
        this.pointerNotes.clear();
    }

    destroy() {
        this.releaseAll();
        if (this._onPointerDown) {
            this.container.removeEventListener('pointerdown', this._onPointerDown);
            window.removeEventListener('pointermove', this._onPointerMove);
            window.removeEventListener('pointerup', this._onPointerUp);
            window.removeEventListener('pointercancel', this._onPointerCancel);
            this.container.removeEventListener('contextmenu', this._onContextMenu);
        }

        if (this._onKeyDown) {
            document.removeEventListener('keydown', this._onKeyDown);
            document.removeEventListener('keyup', this._onKeyUp);
        }
    }
}
