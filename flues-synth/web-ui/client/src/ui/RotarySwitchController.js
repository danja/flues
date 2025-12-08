// RotarySwitchController.js
// Handles rotary switch interactions (discrete positions)

export class RotarySwitchController {
    constructor(element, labelElement, positions, onChange, defaultPosition = 0) {
        this.element = element;
        this.labelElement = labelElement;
        this.positions = positions;  // Array of position labels
        this.onChange = onChange;
        this.currentPosition = defaultPosition;
        this.isDragging = false;
        this.startY = 0;
        this.startPosition = 0;

        this.setupListeners();
        this.updateDisplay();
    }

    setupListeners() {
        this.element.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.startY = e.clientY;
            this.startPosition = this.currentPosition;
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => {
            if (this.isDragging) {
                const delta = Math.floor((this.startY - e.clientY) / 30);
                const newPosition = Math.max(0, Math.min(
                    this.positions.length - 1,
                    this.startPosition + delta
                ));

                if (newPosition !== this.currentPosition) {
                    this.currentPosition = newPosition;
                    this.updateDisplay();
                    this.onChange(this.currentPosition);
                }
            }
        });

        document.addEventListener('mouseup', () => {
            this.isDragging = false;
        });

        // Touch support
        this.element.addEventListener('touchstart', (e) => {
            this.isDragging = true;
            this.startY = e.touches[0].clientY;
            this.startPosition = this.currentPosition;
            e.preventDefault();
        });

        document.addEventListener('touchmove', (e) => {
            if (this.isDragging) {
                const delta = Math.floor((this.startY - e.touches[0].clientY) / 30);
                const newPosition = Math.max(0, Math.min(
                    this.positions.length - 1,
                    this.startPosition + delta
                ));

                if (newPosition !== this.currentPosition) {
                    this.currentPosition = newPosition;
                    this.updateDisplay();
                    this.onChange(this.currentPosition);
                }
            }
        });

        document.addEventListener('touchend', () => {
            this.isDragging = false;
        });

        // Click to advance
        this.element.addEventListener('click', (e) => {
            if (!this.isDragging) {
                this.currentPosition = (this.currentPosition + 1) % this.positions.length;
                this.updateDisplay();
                this.onChange(this.currentPosition);
            }
        });
    }

    updateDisplay() {
        // Rotation for 5 positions: -90, -45, 0, 45, 90 degrees
        const anglePerPosition = 180 / (this.positions.length - 1);
        const rotation = (this.currentPosition * anglePerPosition) - 90;
        this.element.style.transform = `rotate(${rotation}deg)`;
        this.labelElement.textContent = this.positions[this.currentPosition];
    }

    setPosition(position) {
        this.currentPosition = Math.max(0, Math.min(this.positions.length - 1, position));
        this.updateDisplay();
    }
}
