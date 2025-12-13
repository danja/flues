# Autostart on Raspberry Pi (systemd user service)

Create a user-level systemd service that runs `/home/danny/github/flues/play` at boot and restarts it on crash.

1) Make the script executable
```
chmod +x /home/danny/github/flues/play
```

2) Create the user service at `~/.config/systemd/user/flues-play.service`
```
[Unit]
Description=Flues play runner
After=network.target sound.target

[Service]
Type=simple
WorkingDirectory=/home/danny/github/flues
ExecStart=/home/danny/github/flues/play
Restart=always
RestartSec=2
# Optional: set env vars or log to a file
# Environment=FLUES_CONTROL_NOTES=1 FLUES_MIDI_DEBUG=0
# StandardOutput=append:/home/danny/github/flues/play.log
# StandardError=append:/home/danny/github/flues/play.log

[Install]
WantedBy=default.target
```

3) (Optional) enable lingering so the user service runs without an active login
```
loginctl enable-linger $USER
```

4) Enable and start the service
```
systemctl --user daemon-reload
systemctl --user enable flues-play.service
systemctl --user start flues-play.service
```

5) Check status and logs
```
systemctl --user status flues-play.service
journalctl --user -u flues-play.service -f
```

Notes
- Use a system-wide service instead if you prefer: drop `--user`, place the unit in `/etc/systemd/system/`, and run `sudo systemctl enable flues-play.service`.
- Adjust `ExecStart` or environment variables if the path or options differ on your device.
