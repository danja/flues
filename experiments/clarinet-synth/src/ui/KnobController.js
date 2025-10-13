// KnobController.js
// Handles rotary knob interactions

export class KnobController {
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
