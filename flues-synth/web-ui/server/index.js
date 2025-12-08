/**
 * Flues Web Server - Main entry point
 * Serves static web UI and provides WebSocket bridge for MIDI control + audio visualization
 */

import express from 'express';
import { WebSocketServer } from 'ws';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { MidiBridge } from './midi-bridge.js';
import { AudioTap } from './audio-tap.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const HTTP_PORT = process.env.HTTP_PORT || 8082;
const WS_PORT = process.env.WS_PORT || 8081;
const UPDATE_RATE_MS = 16;  // 60 Hz

// Initialize Express for static file serving
const app = express();
app.use(express.static(join(__dirname, '../client/dist')));

app.get('/', (req, res) => {
    res.sendFile(join(__dirname, '../client/dist/index.html'));
});

const httpServer = app.listen(HTTP_PORT, () => {
    console.log(`HTTP Server: Listening on port ${HTTP_PORT}`);
    console.log(`  → Open http://localhost:${HTTP_PORT} in your browser`);
});

// Initialize WebSocket server
const wss = new WebSocketServer({ port: WS_PORT });

// Initialize MIDI bridge and audio tap
const midiBridge = new MidiBridge('flues-web-midi');
const audioTap = new AudioTap('hw:Loopback,1');

// Track connected clients
const clients = new Set();

wss.on('connection', (ws) => {
    console.log('WebSocket: Client connected');
    clients.add(ws);

    ws.on('message', async (data) => {
        try {
            const message = JSON.parse(data.toString());
            handleClientMessage(message, ws);
        } catch (err) {
            console.error('WebSocket: Invalid message:', err);
        }
    });

    ws.on('close', () => {
        console.log('WebSocket: Client disconnected');
        clients.delete(ws);
    });

    ws.on('error', (err) => {
        console.error('WebSocket: Client error:', err);
        clients.delete(ws);
    });
});

function handleClientMessage(message, ws) {
    switch (message.type) {
        case 'cc':
            // MIDI CC: { type: 'cc', cc: 71, value: 63 }
            if (typeof message.cc === 'number' && typeof message.value === 'number') {
                midiBridge.sendCC(message.cc, message.value);
            }
            break;

        case 'note_on':
            // Note On: { type: 'note_on', note: 60, velocity: 96 }
            if (typeof message.note === 'number' && typeof message.velocity === 'number') {
                midiBridge.sendNoteOn(message.note, message.velocity);
            }
            break;

        case 'note_off':
            // Note Off: { type: 'note_off', note: 60 }
            if (typeof message.note === 'number') {
                midiBridge.sendNoteOff(message.note);
            }
            break;

        case 'all_notes_off':
            // All Notes Off: { type: 'all_notes_off' }
            midiBridge.allNotesOff();
            break;

        default:
            console.warn('WebSocket: Unknown message type:', message.type);
    }
}

// Broadcast audio data to all connected clients at 60Hz
function broadcastAudioData() {
    if (clients.size === 0) return;

    const audioData = audioTap.getData();

    // Encode binary frame:
    // - timeDomain: Float32Array (512 samples × 4 bytes = 2048 bytes)
    // - freqDomain: Uint8Array (40 bins = 40 bytes)
    // - rms: Float32 (4 bytes)
    // - peak: Float32 (4 bytes)
    // - voiceCount: Uint32 (4 bytes)
    // Total: 2100 bytes

    const buffer = new ArrayBuffer(2100);
    const view = new DataView(buffer);

    // Copy time-domain samples (Float32)
    for (let i = 0; i < 512; i++) {
        view.setFloat32(i * 4, audioData.timeDomain[i], true);  // little-endian
    }

    // Copy frequency bins (Uint8)
    for (let i = 0; i < 40; i++) {
        view.setUint8(2048 + i, audioData.freqDomain[i]);
    }

    // Copy stats
    view.setFloat32(2088, audioData.rms, true);
    view.setFloat32(2092, audioData.peak, true);
    view.setUint32(2096, audioData.voiceCount, true);

    // Broadcast to all clients
    clients.forEach((client) => {
        if (client.readyState === 1) {  // WebSocket.OPEN
            client.send(buffer);
        }
    });
}

// Start services
async function start() {
    console.log('=== Flues Web Server v0.1.0 ===\n');

    // Start MIDI bridge
    const midiOk = await midiBridge.start();
    if (!midiOk) {
        console.error('Failed to start MIDI bridge');
        process.exit(1);
    }

    // Start audio tap
    const audioOk = await audioTap.start();
    if (!audioOk) {
        console.error('Failed to start audio tap');
        process.exit(1);
    }

    // Start audio broadcast loop
    setInterval(broadcastAudioData, UPDATE_RATE_MS);

    console.log(`\nWebSocket Server: Listening on port ${WS_PORT}`);
    console.log('\nReady! Connect your browser to start controlling flues-synth.\n');
}

// Cleanup on exit
function cleanup() {
    console.log('\nShutting down...');
    audioTap.stop();
    midiBridge.stop();
    httpServer.close();
    wss.close();
    process.exit(0);
}

process.on('SIGINT', cleanup);
process.on('SIGTERM', cleanup);

// Start server
start().catch((err) => {
    console.error('Fatal error:', err);
    process.exit(1);
});
