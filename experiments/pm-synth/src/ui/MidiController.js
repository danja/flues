// MidiController.js
// Handles MIDI input for note on/off and future CC mapping

export class MidiController {
    constructor(onNoteOn, onNoteOff) {
        this.onNoteOn = onNoteOn;
        this.onNoteOff = onNoteOff;
        this.midiAccess = null;
        this.selectedInput = null;
        this.selectedChannel = 'all'; // 'all' or 1-16
        this.activeNotes = new Map(); // Track active MIDI notes
        this.enabled = true;
        this.onDeviceChange = null; // Callback for device list changes
        this.onActivity = null; // Callback for MIDI activity indicator
    }

    async initialize() {
        if (!navigator.requestMIDIAccess) {
            console.warn('[MidiController] Web MIDI API not supported');
            return false;
        }

        try {
            this.midiAccess = await navigator.requestMIDIAccess();
            console.log('[MidiController] MIDI access granted');

            // Listen for device connect/disconnect
            this.midiAccess.onstatechange = (e) => {
                console.log(`[MidiController] Device ${e.port.state}: ${e.port.name}`);
                if (this.onDeviceChange) {
                    this.onDeviceChange(this.getInputDevices());
                }
            };

            // Auto-select first available input
            const inputs = this.getInputDevices();
            if (inputs.length > 0) {
                this.selectInput(inputs[0].id);
            }

            return true;
        } catch (error) {
            console.error('[MidiController] Failed to initialize MIDI:', error);
            return false;
        }
    }

    getInputDevices() {
        if (!this.midiAccess) return [];

        const devices = [];
        for (const input of this.midiAccess.inputs.values()) {
            devices.push({
                id: input.id,
                name: input.name || 'Unknown Device',
                manufacturer: input.manufacturer || '',
                state: input.state
            });
        }
        return devices;
    }

    selectInput(inputId) {
        // Disconnect previous input
        if (this.selectedInput) {
            this.selectedInput.onmidimessage = null;
        }

        if (!inputId) {
            this.selectedInput = null;
            return;
        }

        // Connect to new input
        const input = this.midiAccess.inputs.get(inputId);
        if (!input) {
            console.warn(`[MidiController] Input ${inputId} not found`);
            return;
        }

        this.selectedInput = input;
        this.selectedInput.onmidimessage = (event) => this.handleMidiMessage(event);
        console.log(`[MidiController] Selected input: ${input.name}`);
    }

    setChannel(channel) {
        // channel can be 'all' or 1-16
        this.selectedChannel = channel;
        console.log(`[MidiController] Channel set to: ${channel}`);
    }

    setEnabled(enabled) {
        this.enabled = enabled;
        if (!enabled) {
            // Release all active notes
            this.releaseAllNotes();
        }
        console.log(`[MidiController] MIDI ${enabled ? 'enabled' : 'disabled'}`);
    }

    handleMidiMessage(event) {
        if (!this.enabled) return;

        const [status, data1, data2] = event.data;
        const messageType = status & 0xF0;
        const channel = (status & 0x0F) + 1; // MIDI channels are 1-16

        // Filter by channel if not set to 'all'
        if (this.selectedChannel !== 'all' && channel !== parseInt(this.selectedChannel)) {
            return;
        }

        // Trigger activity indicator
        if (this.onActivity) {
            this.onActivity();
        }

        switch (messageType) {
            case 0x90: // Note On
                if (data2 > 0) {
                    this.handleNoteOn(data1, data2);
                } else {
                    // Note On with velocity 0 is Note Off
                    this.handleNoteOff(data1);
                }
                break;

            case 0x80: // Note Off
                this.handleNoteOff(data1);
                break;

            case 0xB0: // Control Change (for future CC mapping)
                this.handleControlChange(data1, data2);
                break;

            default:
                // Ignore other MIDI messages for now
                break;
        }
    }

    handleNoteOn(midiNote, velocity) {
        const frequency = this.midiNoteToFrequency(midiNote);
        const noteName = this.midiNoteToName(midiNote);
        const normalizedVelocity = velocity / 127;

        console.log(`[MidiController] Note On: ${noteName} (${midiNote}) @ ${frequency.toFixed(2)}Hz, vel=${velocity}`);

        // Track active note
        this.activeNotes.set(midiNote, { noteName, frequency, velocity });

        // Call the note on callback
        try {
            this.onNoteOn(noteName, frequency, normalizedVelocity);
        } catch (error) {
            console.error('[MidiController] Error calling onNoteOn:', error);
        }
    }

    handleNoteOff(midiNote) {
        const noteData = this.activeNotes.get(midiNote);
        if (!noteData) return; // Note wasn't active

        console.log(`[MidiController] Note Off: ${noteData.noteName} (${midiNote})`);

        // Remove from active notes
        this.activeNotes.delete(midiNote);

        // Call the note off callback
        try {
            this.onNoteOff(noteData.noteName);
        } catch (error) {
            console.error('[MidiController] Error calling onNoteOff:', error);
        }
    }

    handleControlChange(controller, value) {
        // Placeholder for future CC mapping implementation
        console.log(`[MidiController] CC: ${controller} = ${value}`);
        // TODO: Implement CC mapping in Phase 3
    }

    releaseAllNotes() {
        // Send note off for all active notes
        for (const [midiNote, noteData] of this.activeNotes.entries()) {
            console.log(`[MidiController] Releasing stuck note: ${noteData.noteName}`);
            try {
                this.onNoteOff(noteData.noteName);
            } catch (error) {
                console.error('[MidiController] Error releasing note:', error);
            }
        }
        this.activeNotes.clear();
    }

    midiNoteToFrequency(noteNumber) {
        // A4 (MIDI note 69) = 440 Hz
        // Formula: f = 440 * 2^((n-69)/12)
        return 440 * Math.pow(2, (noteNumber - 69) / 12);
    }

    midiNoteToName(noteNumber) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(noteNumber / 12) - 1;
        const noteName = noteNames[noteNumber % 12];
        return `${noteName}${octave}`;
    }

    shutdown() {
        this.releaseAllNotes();
        if (this.selectedInput) {
            this.selectedInput.onmidimessage = null;
        }
    }
}
