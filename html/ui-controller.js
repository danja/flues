// ui-controller.js
// Handles all UI interactions for the clarinet synthesizer

class KnobController {
    constructor(element, valueElement, onChange, min = 0, max = 100, defaultValue = 50) {
        this.element = element;
        this.valueElement = valueElement;
        this.onChange = onChange;
        this.min = min;
        this.max = max;
        this.value = defaultValue;
        this.isDragging = false;
        this.startY = 0;
        this.startValue = 0;
        
        this.setupListeners();
        this.updateDisplay();
    }
    
    setupListeners() {
        this.element.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.startY = e.clientY;
            this.startValue = this.value;
            e.preventDefault();
        });
        
        document.addEventListener('mousemove', (e) => {
            if (this.isDragging) {
                const delta = (this.startY - e.clientY) * 0.5;
                this.value = Math.max(this.min, Math.min(this.max, this.startValue + delta));
                this.updateDisplay();
                this.onChange(this.value / 100);
            }
        });
        
        document.addEventListener('mouseup', () => {
            this.isDragging = false;
        });
        
        // Touch support
        this.element.addEventListener('touchstart', (e) => {
            this.isDragging = true;
            this.startY = e.touches[0].clientY;
            this.startValue = this.value;
            e.preventDefault();
        });
        
        document.addEventListener('touchmove', (e) => {
            if (this.isDragging) {
                const delta = (this.startY - e.touches[0].clientY) * 0.5;
                this.value = Math.max(this.min, Math.min(this.max, this.startValue + delta));
                this.updateDisplay();
                this.onChange(this.value / 100);
            }
        });
        
        document.addEventListener('touchend', () => {
            this.isDragging = false;
        });
        
        // Double-click to reset
        this.element.addEventListener('dblclick', () => {
            this.value = (this.min + this.max) / 2;
            this.updateDisplay();
            this.onChange(this.value / 100);
        });
    }
    
    updateDisplay() {
        const rotation = (this.value / 100) * 270 - 135;
        this.element.style.transform = `rotate(${rotation}deg)`;
        this.valueElement.textContent = Math.round(this.value);
    }
    
    setValue(value) {
        this.value = Math.max(this.min, Math.min(this.max, value));
        this.updateDisplay();
    }
}

class KeyboardController {
    constructor(containerElement, onNoteOn, onNoteOff) {
        this.container = containerElement;
        this.onNoteOn = onNoteOn;
        this.onNoteOff = onNoteOff;
        this.activeKeys = new Set();
        this.keyMap = this.createKeyMap();
        
        this.setupListeners();
    }
    
    createKeyMap() {
        // Map computer keyboard to musical notes
        return {
            'a': 'C4',
            'w': 'C#4',
            's': 'D4',
            'e': 'D#4',
            'd': 'E4',
            'f': 'F4',
            't': 'F#4',
            'g': 'G4',
            'y': 'G#4',
            'h': 'A4',
            'u': 'A#4',
            'j': 'B4',
            'k': 'C5'
        };
    }
    
    noteToFrequency(note) {
        const notes = {
            'C4': 261.63, 'C#4': 277.18, 'D4': 293.66, 'D#4': 311.13,
            'E4': 329.63, 'F4': 349.23, 'F#4': 369.99, 'G4': 392.00,
            'G#4': 415.30, 'A4': 440.00, 'A#4': 466.16, 'B4': 493.88,
            'C5': 523.25
        };
        return notes[note] || 440;
    }
    
    setupListeners() {
        // Mouse/touch events on visual keyboard
        const keys = this.container.querySelectorAll('.key');
        
        keys.forEach(key => {
            const note = key.dataset.note;
            
            key.addEventListener('mousedown', (e) => {
                e.preventDefault();
                this.pressKey(note);
            });
            
            key.addEventListener('mouseup', () => {
                this.releaseKey(note);
            });
            
            key.addEventListener('mouseleave', () => {
                if (this.activeKeys.has(note)) {
                    this.releaseKey(note);
                }
            });
            
            // Touch events
            key.addEventListener('touchstart', (e) => {
                e.preventDefault();
                this.pressKey(note);
            });
            
            key.addEventListener('touchend', (e) => {
                e.preventDefault();
                this.releaseKey(note);
            });
        });
        
        // Computer keyboard events
        document.addEventListener('keydown', (e) => {
            const note = this.keyMap[e.key.toLowerCase()];
            if (note && !this.activeKeys.has(note)) {
                this.pressKey(note);
            }
        });
        
        document.addEventListener('keyup', (e) => {
            const note = this.keyMap[e.key.toLowerCase()];
            if (note) {
                this.releaseKey(note);
            }
        });
    }
    
    pressKey(note) {
        if (this.activeKeys.has(note)) return;
        
        this.activeKeys.add(note);
        const keyElement = this.container.querySelector(`[data-note="${note}"]`);
        if (keyElement) {
            keyElement.classList.add('active');
        }
        
        const frequency = this.noteToFrequency(note);
        this.onNoteOn(note, frequency);
    }
    
    releaseKey(note) {
        if (!this.activeKeys.has(note)) return;
        
        this.activeKeys.delete(note);
        const keyElement = this.container.querySelector(`[data-note="${note}"]`);
        if (keyElement) {
            keyElement.classList.remove('active');
        }
        
        this.onNoteOff(note);
    }
    
    releaseAllKeys() {
        this.activeKeys.forEach(note => {
            this.releaseKey(note);
        });
    }
}

class Visualizer {
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