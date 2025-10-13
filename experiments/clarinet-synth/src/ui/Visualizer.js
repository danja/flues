// Visualizer.js
// Handles waveform visualization

export class Visualizer {
    constructor(canvasElement) {
        this.canvas = canvasElement;
        this.ctx = this.canvas.getContext('2d');
        this.isRunning = false;

        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width;
        this.canvas.height = rect.height;
    }

    start(getDataFunction) {
        this.isRunning = true;
        this.dataFunction = getDataFunction;
        this.draw();
    }

    stop() {
        this.isRunning = false;
    }

    draw() {
        if (!this.isRunning) return;

        const data = this.dataFunction();

        this.ctx.fillStyle = '#1a1a1a';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

        if (data) {
            this.ctx.lineWidth = 2;
            this.ctx.strokeStyle = '#4a9eff';
            this.ctx.beginPath();

            const sliceWidth = this.canvas.width / data.length;
            let x = 0;

            for (let i = 0; i < data.length; i++) {
                const v = data[i] / 128.0;
                const y = v * this.canvas.height / 2;

                if (i === 0) {
                    this.ctx.moveTo(x, y);
                } else {
                    this.ctx.lineTo(x, y);
                }

                x += sliceWidth;
            }

            this.ctx.stroke();
        }

        requestAnimationFrame(() => this.draw());
    }
}
