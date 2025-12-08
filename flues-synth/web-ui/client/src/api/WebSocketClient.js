/**
 * WebSocket Client - Connects to flues-web-server for MIDI control and audio visualization
 */

export class WebSocketClient {
    constructor(url = 'ws://localhost:8081') {
        this.url = url;
        this.ws = null;
        this.audioCallbacks = [];
        this.connectCallback = null;
        this.disconnectCallback = null;
        this.reconnectTimeout = null;
    }

    connect() {
        console.log(`WebSocket: Connecting to ${this.url}...`);
        this.ws = new WebSocket(this.url);
        this.ws.binaryType = 'arraybuffer';

        this.ws.onopen = () => {
            console.log('WebSocket: Connected');
            if (this.reconnectTimeout) {
                clearTimeout(this.reconnectTimeout);
                this.reconnectTimeout = null;
            }
            if (this.connectCallback) {
                this.connectCallback();
            }
        };

        this.ws.onclose = () => {
            console.warn('WebSocket: Disconnected');
            if (this.disconnectCallback) {
                this.disconnectCallback();
            }
            // Auto-reconnect after 2 seconds
            this.reconnectTimeout = setTimeout(() => this.connect(), 2000);
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket: Error:', error);
        };

        this.ws.onmessage = (event) => {
            if (typeof event.data === 'string') {
                // JSON control messages (future: preset sync)
                try {
                    const message = JSON.parse(event.data);
                    console.log('WebSocket: Received message:', message);
                } catch (err) {
                    console.error('WebSocket: Invalid JSON:', err);
                }
            } else {
                // Binary audio data
                this.handleAudioData(event.data);
            }
        };
    }

    disconnect() {
        if (this.reconnectTimeout) {
            clearTimeout(this.reconnectTimeout);
            this.reconnectTimeout = null;
        }
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    /**
     * Send MIDI CC message
     * @param {number} cc - CC number (0-127)
     * @param {number} value - CC value (0-127)
     */
    sendCC(cc, value) {
        this.send({ type: 'cc', cc, value: Math.round(value) });
    }

    /**
     * Send MIDI Note On
     * @param {number} note - Note number (0-127)
     * @param {number} velocity - Velocity (0-127)
     */
    sendNoteOn(note, velocity) {
        this.send({ type: 'note_on', note, velocity });
    }

    /**
     * Send MIDI Note Off
     * @param {number} note - Note number (0-127)
     */
    sendNoteOff(note) {
        this.send({ type: 'note_off', note });
    }

    /**
     * Send All Notes Off
     */
    allNotesOff() {
        this.send({ type: 'all_notes_off' });
    }

    send(obj) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(obj));
        } else {
            console.warn('WebSocket: Cannot send, not connected');
        }
    }

    /**
     * Register callback for audio data updates
     * @param {Function} callback - Called with { timeDomain, freqDomain, rms, peak, voiceCount }
     */
    onAudio(callback) {
        this.audioCallbacks.push(callback);
    }

    /**
     * Register callback for connection event
     * @param {Function} callback
     */
    onConnect(callback) {
        this.connectCallback = callback;
    }

    /**
     * Register callback for disconnection event
     * @param {Function} callback
     */
    onDisconnect(callback) {
        this.disconnectCallback = callback;
    }

    /**
     * Parse binary audio frame from server
     * Frame format (2100 bytes):
     * - 0-2047: timeDomain (512 × float32)
     * - 2048-2087: freqDomain (40 × uint8)
     * - 2088-2091: rms (float32)
     * - 2092-2095: peak (float32)
     * - 2096-2099: voiceCount (uint32)
     */
    handleAudioData(buffer) {
        if (buffer.byteLength !== 2100) {
            console.warn(`WebSocket: Invalid audio frame size: ${buffer.byteLength}`);
            return;
        }

        const view = new DataView(buffer);

        // Parse time-domain samples (Float32)
        const timeDomain = new Float32Array(512);
        for (let i = 0; i < 512; i++) {
            timeDomain[i] = view.getFloat32(i * 4, true);  // little-endian
        }

        // Parse frequency bins (Uint8)
        const freqDomain = new Uint8Array(40);
        for (let i = 0; i < 40; i++) {
            freqDomain[i] = view.getUint8(2048 + i);
        }

        // Parse stats
        const rms = view.getFloat32(2088, true);
        const peak = view.getFloat32(2092, true);
        const voiceCount = view.getUint32(2096, true);

        // Notify all audio callbacks
        const audioData = { timeDomain, freqDomain, rms, peak, voiceCount };
        this.audioCallbacks.forEach(cb => cb(audioData));
    }
}
