# Systemd Service Files

These service files enable auto-start of flues-synth and the web server on boot.

## Installation

### 1. Copy Service Files

```bash
sudo cp flues-synth.service /etc/systemd/system/
sudo cp flues-web-server.service /etc/systemd/system/
```

### 2. Reload Systemd

```bash
sudo systemctl daemon-reload
```

### 3. Enable Services (Auto-start on Boot)

```bash
sudo systemctl enable flues-synth
sudo systemctl enable flues-web-server
```

### 4. Start Services

```bash
sudo systemctl start flues-synth
sudo systemctl start flues-web-server
```

## Management Commands

### Check Status

```bash
systemctl status flues-synth
systemctl status flues-web-server
```

### View Logs

```bash
# Real-time logs
journalctl -u flues-synth -f
journalctl -u flues-web-server -f

# Last 100 lines
journalctl -u flues-synth -n 100
journalctl -u flues-web-server -n 100
```

### Stop Services

```bash
sudo systemctl stop flues-synth
sudo systemctl stop flues-web-server
```

### Restart Services

```bash
sudo systemctl restart flues-synth
sudo systemctl restart flues-web-server
```

### Disable Auto-start

```bash
sudo systemctl disable flues-synth
sudo systemctl disable flues-web-server
```

## Configuration

### Edit Service Files

```bash
sudo systemctl edit --full flues-synth
sudo systemctl edit --full flues-web-server
```

### Modify User/Paths

If your username is not `pi` or files are in a different location, edit the service files:

```ini
User=YOUR_USERNAME
WorkingDirectory=/path/to/flues-synth
ExecStart=/path/to/flues-synth/builddir/flues-synth
```

### Environment Variables

You can add or modify environment variables in the service file:

```ini
Environment="FLUES_MIDI_DEBUG=1"
Environment="HTTP_PORT=9000"
Environment="WS_PORT=9001"
```

## Troubleshooting

### Service Fails to Start

```bash
# Check for errors
systemctl status flues-synth
journalctl -xe

# Common issues:
# 1. Wrong user/path
# 2. ALSA device not available
# 3. Permissions issues
```

### MIDI Not Working

```bash
# Check ALSA MIDI connections
aconnect -l

# Manually connect if needed
aconnect <web-server-port> <flues-synth-port>
```

### Audio Not Working

```bash
# Check ALSA devices
aplay -l

# Test audio
speaker-test -D hw:Headphones -c 1
```

### Web Server Not Accessible

```bash
# Check if port is listening
sudo netstat -tulpn | grep 8080

# Check firewall
sudo ufw status

# Allow ports if needed
sudo ufw allow 8080/tcp
sudo ufw allow 8081/tcp
```

## Monitoring

### System Resource Usage

```bash
# CPU/Memory usage
systemctl status flues-synth flues-web-server
```

### Auto-restart on Failure

Both services are configured with `Restart=on-failure` and `RestartSec=5`, so they will automatically restart if they crash.

## Network Access

To access the web UI from another device on the network:

1. Find your Raspberry Pi's IP address:
   ```bash
   hostname -I
   ```

2. Open browser on another device:
   ```
   http://<raspberry-pi-ip>:8080
   ```

Or use mDNS (if avahi-daemon is running):
```
http://raspberrypi.local:8080
```

## Security Notes

- Services run as user `pi` (not root) for security
- Only HTTP/WebSocket ports are exposed (8080, 8081)
- No authentication is included - use firewall rules if needed
- For production use, consider adding nginx reverse proxy with HTTPS
