/**
 * Voice Meters - Displays active voice count (0-4 voices)
 */

export class VoiceMeters {
    constructor(containerElement) {
        this.container = containerElement;
        this.meters = [];

        // Create 4 voice indicators
        for (let i = 0; i < 4; i++) {
            const meter = document.createElement('div');
            meter.className = 'voice-meter';
            meter.dataset.voice = i + 1;
            meter.textContent = `V${i + 1}`;
            this.container.appendChild(meter);
            this.meters.push(meter);
        }
    }

    /**
     * Update voice meters based on active voice count
     * @param {number} voiceCount - Number of active voices (0-4)
     */
    update(voiceCount) {
        this.meters.forEach((meter, i) => {
            if (i < voiceCount) {
                meter.classList.add('active');
            } else {
                meter.classList.remove('active');
            }
        });
    }
}
