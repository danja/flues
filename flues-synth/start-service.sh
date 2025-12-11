# see flues-synth/docs/startup.md

loginctl enable-linger $USER

systemctl --user daemon-reload
systemctl --user enable flues-play.service
systemctl --user start flues-play.service