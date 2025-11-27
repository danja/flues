// JoystickControl.js
// 2D canvas joystick for controlling formant frequencies
// Maps to IPA vowel quadrilateral (X→F2, Y→F1)

export class JoystickControl {
  constructor({
    canvas,
    width = 400,
    height = 300,
    onInput = (x, y) => {},
    onStart = () => {},
    onEnd = () => {},
  }) {
    this.canvas = canvas;
    this.width = width;
    this.height = height;
    this.onInput = onInput;
    this.onStart = onStart;
    this.onEnd = onEnd;

    // Current position (normalized 0-1)
    this.x = 0.5;
    this.y = 0.5;

    // Interaction state
    this.isDragging = false;

    // IPA vowel positions (approximate, for visual reference)
    this.vowels = [
      { label: 'i', x: 0.1, y: 0.1, color: '#4CAF50' },   // High front
      { label: 'e', x: 0.2, y: 0.35, color: '#4CAF50' },  // Mid front
      { label: 'a', x: 0.4, y: 0.85, color: '#FFC107' },  // Low central
      { label: 'o', x: 0.75, y: 0.4, color: '#FF5722' },  // Mid back
      { label: 'u', x: 0.85, y: 0.15, color: '#FF5722' }, // High back
    ];

    this.setupCanvas();
    this.setupEventListeners();
    this.render();
  }

  setupCanvas() {
    this.canvas.width = this.width;
    this.canvas.height = this.height;
    this.ctx = this.canvas.getContext('2d');
  }

  setupEventListeners() {
    // Mouse events
    this.canvas.addEventListener('mousedown', (e) => this.handleStart(e));
    window.addEventListener('mousemove', (e) => this.handleMove(e));
    window.addEventListener('mouseup', () => this.handleEnd());

    // Touch events
    this.canvas.addEventListener('touchstart', (e) => {
      e.preventDefault();
      this.handleStart(e.touches[0]);
    });
    window.addEventListener('touchmove', (e) => {
      if (this.isDragging) {
        e.preventDefault();
        this.handleMove(e.touches[0]);
      }
    });
    window.addEventListener('touchend', () => this.handleEnd());
    window.addEventListener('touchcancel', () => this.handleEnd());
  }

  handleStart(event) {
    this.isDragging = true;
    this.updatePosition(event);
    this.onStart();
  }

  handleMove(event) {
    if (this.isDragging) {
      this.updatePosition(event);
    }
  }

  handleEnd() {
    this.isDragging = false;
    this.onEnd();
  }

  updatePosition(event) {
    const rect = this.canvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;

    // Normalize to 0-1
    this.x = Math.max(0, Math.min(1, x / this.width));
    this.y = Math.max(0, Math.min(1, y / this.height));

    this.render();
    this.onInput(this.x, this.y);
  }

  setPosition(x, y) {
    this.x = Math.max(0, Math.min(1, x));
    this.y = Math.max(0, Math.min(1, y));
    this.render();
  }

  render() {
    const ctx = this.ctx;
    const w = this.width;
    const h = this.height;

    // Clear canvas - lighter background when active
    ctx.fillStyle = this.isDragging ? '#1a2030' : '#1a1a1a';
    ctx.fillRect(0, 0, w, h);

    // Draw grid
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
      const x = (i / 4) * w;
      const y = (i / 4) * h;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // Draw vowel quadrilateral outline
    ctx.strokeStyle = '#555';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(0.1 * w, 0.1 * h); // i
    ctx.lineTo(0.1 * w, 0.85 * h); // to a (front)
    ctx.lineTo(0.4 * w, 0.85 * h); // a
    ctx.lineTo(0.85 * w, 0.15 * h); // to u
    ctx.lineTo(0.85 * w, 0.1 * h); // u
    ctx.closePath();
    ctx.stroke();

    // Draw vowel labels
    ctx.font = 'bold 16px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    this.vowels.forEach((vowel) => {
      const vx = vowel.x * w;
      const vy = vowel.y * h;

      // Draw vowel circle
      ctx.fillStyle = vowel.color;
      ctx.beginPath();
      ctx.arc(vx, vy, 8, 0, Math.PI * 2);
      ctx.fill();

      // Draw label
      ctx.fillStyle = '#fff';
      ctx.fillText(vowel.label, vx, vy);
    });

    // Draw axis labels
    ctx.font = '12px sans-serif';
    ctx.fillStyle = '#888';
    ctx.textAlign = 'left';
    ctx.fillText('Front (F2 high)', 10, h - 10);
    ctx.textAlign = 'right';
    ctx.fillText('Back (F2 low)', w - 10, h - 10);
    ctx.textAlign = 'center';
    ctx.fillText('High (F1 low)', w / 2, 15);
    ctx.fillText('Low (F1 high)', w / 2, h - 5);

    // Draw current position indicator
    const px = this.x * w;
    const py = this.y * h;

    // Crosshair
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1;
    ctx.setLineDash([5, 3]);
    ctx.beginPath();
    ctx.moveTo(px, 0);
    ctx.lineTo(px, h);
    ctx.moveTo(0, py);
    ctx.lineTo(w, py);
    ctx.stroke();
    ctx.setLineDash([]);

    // Position marker
    ctx.fillStyle = 'rgba(33, 150, 243, 0.7)';
    ctx.strokeStyle = '#2196F3';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(px, py, 12, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // Inner dot
    ctx.fillStyle = '#fff';
    ctx.beginPath();
    ctx.arc(px, py, 4, 0, Math.PI * 2);
    ctx.fill();
  }

  destroy() {
    // Cleanup if needed
    this.isDragging = false;
  }
}
