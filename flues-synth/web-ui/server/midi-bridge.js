/**
 * MIDI Bridge - Sends MIDI CCs from WebSocket to virtual MIDI port
 * Uses 'easymidi' package (cross-platform, wraps ALSA on Linux)
 */

import easymidi from 'easymidi';

export class MidiBridge {
    constructor(portName = 'flues-web-midi') {
        this.portName = portName;
        this.output = null;
    }

    async start() {
        try {
            // Create virtual MIDI output port
            this.output = new easymidi.Output(this.portName, true); // true = virtual port

            console.log(`MIDI Bridge: Created virtual port '${this.portName}'`);
            console.log('Available MIDI outputs:', easymidi.getOutputs());
            return true;
        } catch (err) {
            console.error('MIDI Bridge: Failed to create port:', err);
            console.error('Make sure ALSA is available on your system');
            return false;
        }
    }

    stop() {
        if (this.output) {
            this.output.close();
            this.output = null;
            console.log('MIDI Bridge: Closed');
        }
    }

    /**
     * Send MIDI CC message
     * @param {number} cc - CC number (0-127)
     * @param {number} value - CC value (0-127)
     * @param {number} channel - MIDI channel (0-15, default 0)
     */
    sendCC(cc, value, channel = 0) {
        if (!this.output) {
            console.warn('MIDI Bridge: Output not initialized');
            return false;
        }

        try {
            this.output.send('cc', {
                controller: cc & 0x7F,
                value: value & 0x7F,
                channel: channel
            });
            return true;
        } catch (err) {
            console.error('MIDI Bridge: Failed to send CC:', err);
            return false;
        }
    }

    /**
     * Send MIDI Note On message
     * @param {number} note - Note number (0-127)
     * @param {number} velocity - Velocity (0-127)
     * @param {number} channel - MIDI channel (0-15, default 0)
     */
    sendNoteOn(note, velocity, channel = 0) {
        if (!this.output) return false;

        try {
            this.output.send('noteon', {
                note: note & 0x7F,
                velocity: velocity & 0x7F,
                channel: channel
            });
            return true;
        } catch (err) {
            console.error('MIDI Bridge: Failed to send note on:', err);
            return false;
        }
    }

    /**
     * Send MIDI Note Off message
     * @param {number} note - Note number (0-127)
     * @param {number} channel - MIDI channel (0-15, default 0)
     */
    sendNoteOff(note, channel = 0) {
        if (!this.output) return false;

        try {
            this.output.send('noteoff', {
                note: note & 0x7F,
                velocity: 0,
                channel: channel
            });
            return true;
        } catch (err) {
            console.error('MIDI Bridge: Failed to send note off:', err);
            return false;
        }
    }

    /**
     * Send All Notes Off (CC 123)
     * @param {number} channel - MIDI channel (0-15, default 0)
     */
    allNotesOff(channel = 0) {
        return this.sendCC(123, 0, channel);
    }
}
