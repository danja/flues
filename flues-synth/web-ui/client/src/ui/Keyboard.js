// Simple on-screen keyboard that sends Note On/Off callbacks.
export class Keyboard {
    constructor(container, {
        onNoteOn = () => {},
        onNoteOff = () => {},
        velocity = 96
    } = {}) {
        this.container = container;
        this.onNoteOn = onNoteOn;
        this.onNoteOff = onNoteOff;
        this.velocity = velocity;
        this.activeNotes = new Set();

        this.keys = [
            { label: 'C4', note: 60, black: false },
            { label: 'C#', note: 61, black: true },
            { label: 'D', note: 62, black: false },
            { label: 'D#', note: 63, black: true },
            { label: 'E', note: 64, black: false },
            { label: 'F', note: 65, black: false },
            { label: 'F#', note: 66, black: true },
            { label: 'G', note: 67, black: false },
            { label: 'G#', note: 68, black: true },
            { label: 'A', note: 69, black: false },
            { label: 'A#', note: 70, black: true },
            { label: 'B', note: 71, black: false },
            { label: 'C5', note: 72, black: false }
        ];

        this._buildDOM();
    }

    _buildDOM() {
        this.container.innerHTML = '';
        const wrapper = document.createElement('div');
        wrapper.className = 'keyboard';

        this.keys.forEach((key) => {
            const btn = document.createElement('button');
            btn.className = `key ${key.black ? 'black' : 'white'}`;
            btn.textContent = key.label;
            btn.dataset.note = key.note;

            const start = (e) => {
                e.preventDefault();
                if (!this.activeNotes.has(key.note)) {
                    this.activeNotes.add(key.note);
                    btn.classList.add('active');
                    this.onNoteOn(key.note, this.velocity);
                }
            };

            const end = () => {
                if (this.activeNotes.has(key.note)) {
                    this.activeNotes.delete(key.note);
                    btn.classList.remove('active');
                    this.onNoteOff(key.note);
                }
            };

            btn.addEventListener('mousedown', start);
            btn.addEventListener('touchstart', start, { passive: false });
            btn.addEventListener('mouseup', end);
            btn.addEventListener('mouseleave', end);
            btn.addEventListener('touchend', end);

            wrapper.appendChild(btn);
        });

        // Safety: release stuck notes if pointer leaves window
        window.addEventListener('mouseup', () => this.allNotesOff());
        window.addEventListener('touchend', () => this.allNotesOff());

        this.container.appendChild(wrapper);
    }

    allNotesOff() {
        if (this.activeNotes.size === 0) return;
        [...this.activeNotes].forEach((note) => this.onNoteOff(note));
        this.activeNotes.clear();
        this.container.querySelectorAll('.key.active').forEach(el => el.classList.remove('active'));
    }
}
