// MidiInputManager.js
// Thin wrapper around the Web MIDI API providing note events + device tracking.

export class MidiInputManager {
    constructor({
        onNoteOn,
        onNoteOff,
        onControlChange = null,
        onActivity = null,
    } = {}) {
        this.onNoteOn = typeof onNoteOn === 'function' ? onNoteOn : null;
        this.onNoteOff = typeof onNoteOff === 'function' ? onNoteOff : null;
        this.onControlChange = typeof onControlChange === 'function' ? onControlChange : null;
        this.onActivity = typeof onActivity === 'function' ? onActivity : null;

        this.midiAccess = null;
        this.selectedInput = null;
        this.selectedChannel = 'all';
        this.enabled = true;
        this.activeNotes = new Map();
        this.onDeviceChange = null;
    }

    async initialize() {
        if (!navigator.requestMIDIAccess) {
            console.warn('[MidiInputManager] Web MIDI API not available');
            return false;
        }

        try {
            this.midiAccess = await navigator.requestMIDIAccess();
            this.midiAccess.onstatechange = (event) => {
                if (typeof this.onDeviceChange === 'function') {
                    this.onDeviceChange(this.listInputs());
                }

                if (event.port === this.selectedInput && event.port.state !== 'connected') {
                    this._disconnectCurrentInput();
                }
            };

            const inputs = this.listInputs();
            if (inputs.length > 0) {
                this.selectInput(inputs[0].id);
            }

            return true;
        } catch (error) {
            console.error('[MidiInputManager] Failed to initialize', error);
            return false;
        }
    }

    listInputs() {
        if (!this.midiAccess) return [];

        const devices = [];
        for (const input of this.midiAccess.inputs.values()) {
            devices.push({
                id: input.id,
                name: input.name || 'Unknown Device',
                manufacturer: input.manufacturer || '',
                state: input.state,
            });
        }
        return devices;
    }

    selectInput(inputId) {
        this._disconnectCurrentInput();

        if (!inputId || !this.midiAccess) {
            this.selectedInput = null;
            return;
        }

        const input = this.midiAccess.inputs.get(inputId);
        if (!input) {
            console.warn(`[MidiInputManager] Input ${inputId} not found`);
            return;
        }

        this.selectedInput = input;
        this.selectedInput.onmidimessage = (event) => this._handleMessage(event);
    }

    _disconnectCurrentInput() {
        if (this.selectedInput) {
            this.selectedInput.onmidimessage = null;
        }
    }

    setChannel(channel) {
        if (channel === 'all') {
            this.selectedChannel = 'all';
            return;
        }

        const channelNumber = parseInt(channel, 10);
        if (Number.isInteger(channelNumber) && channelNumber >= 1 && channelNumber <= 16) {
            this.selectedChannel = channelNumber;
        }
    }

    setEnabled(enabled) {
        this.enabled = !!enabled;
        if (!this.enabled) {
            this._releaseAllNotes();
        }
    }

    shutdown() {
        this._releaseAllNotes();
        this._disconnectCurrentInput();
        this.selectedInput = null;
    }

    _handleMessage(event) {
        if (!this.enabled) return;

        const [status, data1, data2] = event.data;
        const messageType = status & 0xf0;
        const channel = (status & 0x0f) + 1;

        if (this.selectedChannel !== 'all' && channel !== this.selectedChannel) {
            return;
        }

        if (this.onActivity) {
            this.onActivity({ messageType, channel });
        }

        switch (messageType) {
            case 0x90: // Note on
                if (data2 > 0) {
                    this._handleNoteOn(data1, data2, channel);
                } else {
                    this._handleNoteOff(data1, channel);
                }
                break;

            case 0x80: // Note off
                this._handleNoteOff(data1, channel);
                break;

            case 0xb0: // Control change
                if (this.onControlChange) {
                    this.onControlChange({ controller: data1, value: data2, channel });
                }
                break;

            default:
                break;
        }
    }

    _handleNoteOn(midiNote, velocity, channel) {
        const timestamp = performance.now();
        this.activeNotes.set(midiNote, { velocity, channel, timestamp });

        if (this.onNoteOn) {
            const payload = {
                midi: midiNote,
                velocity: velocity / 127,
                channel,
                name: MidiInputManager.midiToName(midiNote),
                frequency: MidiInputManager.midiToFrequency(midiNote),
            };
            try {
                this.onNoteOn(payload);
            } catch (error) {
                console.error('[MidiInputManager] onNoteOn callback failed', error);
            }
        }
    }

    _handleNoteOff(midiNote, channel) {
        const note = this.activeNotes.get(midiNote);
        if (!note) return;
        this.activeNotes.delete(midiNote);

        if (this.onNoteOff) {
            const payload = {
                midi: midiNote,
                channel,
                name: MidiInputManager.midiToName(midiNote),
            };
            try {
                this.onNoteOff(payload);
            } catch (error) {
                console.error('[MidiInputManager] onNoteOff callback failed', error);
            }
        }
    }

    _releaseAllNotes() {
        for (const midiNote of this.activeNotes.keys()) {
            this._handleNoteOff(midiNote, this.activeNotes.get(midiNote)?.channel ?? 'all');
        }
        this.activeNotes.clear();
    }

    static midiToFrequency(midiNote) {
        return 440 * Math.pow(2, (midiNote - 69) / 12);
    }

    static midiToName(midiNote) {
        const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const index = midiNote % 12;
        const octave = Math.floor(midiNote / 12) - 1;
        return `${names[index]}${octave}`;
    }
}

