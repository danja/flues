// KnobControl.js
// Generic rotary knob interaction handler with mouse/touch support.

export class KnobControl {
    constructor({
        element,
        valueElement,
        onInput,
        min = 0,
        max = 100,
        initial = 50,
        bipolar = false,
        sensitivity = 0.5,
        resetValue = null,
        formatDisplay = null,
    }) {
        if (!element || typeof onInput !== 'function') {
            throw new Error('[KnobControl] element and onInput callback are required');
        }

        this.element = element;
        this.valueElement = valueElement || null;
        this.onInput = onInput;
        this.min = min;
        this.max = max;
        this.value = this._clamp(initial);
        this.bipolar = bipolar;
        this.sensitivity = sensitivity;
        this.resetValue = resetValue ?? (min + max) / 2;
        this.formatDisplay = typeof formatDisplay === 'function' ? formatDisplay : null;

        this._dragging = false;
        this._startY = 0;
        this._startValue = this.value;

        this._bindEvents();
        this._render();
    }

    _clamp(value) {
        return Math.min(Math.max(value, this.min), this.max);
    }

    _normalized() {
        if (this.max === this.min) return 0;
        return (this.value - this.min) / (this.max - this.min);
    }

    _bindEvents() {
        const startDrag = (clientY) => {
            this._dragging = true;
            this._startY = clientY;
            this._startValue = this.value;
        };

        const updateDrag = (clientY) => {
            if (!this._dragging) return;
            const delta = (this._startY - clientY) * this.sensitivity;
            this.value = this._clamp(this._startValue + delta);
            this._render();
            this.onInput(this._normalized(), this.value);
        };

        const endDrag = () => {
            this._dragging = false;
        };

        this.element.addEventListener('mousedown', (event) => {
            event.preventDefault();
            startDrag(event.clientY);
        });

        document.addEventListener('mousemove', (event) => {
            updateDrag(event.clientY);
        });

        document.addEventListener('mouseup', endDrag);

        this.element.addEventListener('touchstart', (event) => {
            if (event.touches.length === 0) return;
            const touch = event.touches[0];
            event.preventDefault();
            startDrag(touch.clientY);
        }, { passive: false });

        document.addEventListener('touchmove', (event) => {
            if (event.touches.length === 0) return;
            const touch = event.touches[0];
            updateDrag(touch.clientY);
        }, { passive: false });

        document.addEventListener('touchend', endDrag, { passive: true });

        this.element.addEventListener('dblclick', () => {
            this.setValue(this.resetValue);
        });
    }

    _render() {
        const rotation = this._normalized() * 270 - 135;
        this.element.style.transform = `rotate(${rotation}deg)`;

        if (this.valueElement) {
            if (this.formatDisplay) {
                this.valueElement.textContent = this.formatDisplay(this._normalized(), this.value);
            } else if (this.bipolar) {
                const midpoint = (this.min + this.max) / 2;
                const bipolarValue = Math.round(this.value - midpoint);
                this.valueElement.textContent = bipolarValue >= 0 ? `+${bipolarValue}` : `${bipolarValue}`;
            } else {
                this.valueElement.textContent = Math.round(this.value);
            }
        }
    }

    setValue(value, { emit = true } = {}) {
        this.value = this._clamp(value);
        this._render();
        if (emit) {
            this.onInput(this._normalized(), this.value);
        }
    }

    setNormalized(value, { emit = true } = {}) {
        const clamped = Math.min(Math.max(value, 0), 1);
        const absolute = this.min + clamped * (this.max - this.min);
        this.setValue(absolute, { emit });
    }
}
