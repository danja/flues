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

    const data = this.dataFunction ? this.dataFunction() : null;

    this.ctx.fillStyle = '#0b1120';
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    if (data) {
      const windowed = this.extractCycles(data, 2);
      this.ctx.lineWidth = 2;
      this.ctx.strokeStyle = '#22d3ee';
      this.ctx.beginPath();

      const sliceWidth = this.canvas.width / windowed.length;
      let x = 0;

      for (let i = 0; i < windowed.length; i++) {
        const v = windowed[i] / 128.0;
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

  extractCycles(data, cycles = 2) {
    if (!data || data.length < 4) return data;

    const crossings = [];
    for (let i = 1; i < data.length; i++) {
      const prev = data[i - 1] - 128;
      const curr = data[i] - 128;
      if (prev < 0 && curr >= 0) {
        crossings.push(i);
        if (crossings.length >= cycles + 1) {
          break;
        }
      }
    }

    if (crossings.length < cycles + 1) {
      return data;
    }

    const start = crossings[0];
    const end = crossings[crossings.length - 1];
    return data.slice(start, end);
  }
}
