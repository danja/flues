// KeyboardController.js
// Handles keyboard interactions (both visual and computer keyboard)

export class KeyboardController {
    constructor(containerElement, onNoteOn, onNoteOff) {
        this.container = containerElement;
        this.onNoteOn = onNoteOn;
        this.onNoteOff = onNoteOff;
        this.activeKeys = new Set();
        this.keyMap = this.createKeyMap();

        this.setupListeners();
    }

    createKeyMap() {
        // Map computer keyboard to musical notes
        return {
            'a': 'C4',
            'w': 'C#4',
            's': 'D4',
            'e': 'D#4',
            'd': 'E4',
            'f': 'F4',
            't': 'F#4',
            'g': 'G4',
            'y': 'G#4',
            'h': 'A4',
            'u': 'A#4',
            'j': 'B4',
            'k': 'C5'
        };
    }

    noteToFrequency(note) {
        const notes = {
            'C4': 261.63, 'C#4': 277.18, 'D4': 293.66, 'D#4': 311.13,
            'E4': 329.63, 'F4': 349.23, 'F#4': 369.99, 'G4': 392.00,
            'G#4': 415.30, 'A4': 440.00, 'A#4': 466.16, 'B4': 493.88,
            'C5': 523.25
        };
        return notes[note] || 440;
    }

    setupListeners() {
        // Mouse/touch events on visual keyboard
        const keys = this.container.querySelectorAll('.key');

        keys.forEach(key => {
            const note = key.dataset.note;

            key.addEventListener('mousedown', (e) => {
                e.preventDefault();
                this.pressKey(note);
            });

            key.addEventListener('mouseup', () => {
                this.releaseKey(note);
            });

            key.addEventListener('mouseleave', () => {
                if (this.activeKeys.has(note)) {
                    this.releaseKey(note);
                }
            });

            // Touch events
            key.addEventListener('touchstart', (e) => {
                e.preventDefault();
                this.pressKey(note);
            });

            key.addEventListener('touchend', (e) => {
                e.preventDefault();
                this.releaseKey(note);
            });
        });

        // Computer keyboard events
        document.addEventListener('keydown', (e) => {
            const note = this.keyMap[e.key.toLowerCase()];
            if (note && !this.activeKeys.has(note)) {
                this.pressKey(note);
            }
        });

        document.addEventListener('keyup', (e) => {
            const note = this.keyMap[e.key.toLowerCase()];
            if (note) {
                this.releaseKey(note);
            }
        });
    }

    pressKey(note) {
        if (this.activeKeys.has(note)) return;

        this.activeKeys.add(note);
        const keyElement = this.container.querySelector(`[data-note="${note}"]`);
        if (keyElement) {
            keyElement.classList.add('active');
        }

        const frequency = this.noteToFrequency(note);
        this.onNoteOn(note, frequency);
    }

    releaseKey(note) {
        if (!this.activeKeys.has(note)) return;

        this.activeKeys.delete(note);
        const keyElement = this.container.querySelector(`[data-note="${note}"]`);
        if (keyElement) {
            keyElement.classList.remove('active');
        }

        this.onNoteOff(note);
    }

    releaseAllKeys() {
        this.activeKeys.forEach(note => {
            this.releaseKey(note);
        });
    }
}
