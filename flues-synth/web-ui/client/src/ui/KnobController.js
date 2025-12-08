// SVG knob used across the web UI. Accepts normalized 0-1 values and renders its
// own DOM into the provided container.
export class KnobController {
    constructor(container, {
        label = '',
        value = 0.5,
        min = 0,
        max = 1,
        step = 0.001,
        onChange = () => {}
    } = {}) {
        this.container = container;
        this.label = label;
        this.min = min;
        this.max = max;
        this.step = step;
        this.onChange = onChange;
        this.isDragging = false;
        this.startY = 0;
        this.startValue = value;
        this.value = value; // normalized 0-1

        this._buildDOM();
        this._attachEvents();
        this._render();
    }

    _buildDOM() {
        this.container.innerHTML = '';

        this.wrapper = document.createElement('div');
        this.wrapper.className = 'knob-container';

        this.labelEl = document.createElement('div');
        this.labelEl.className = 'knob-label';
        this.labelEl.textContent = this.label;

        this.valueEl = document.createElement('div');
        this.valueEl.className = 'knob-value';

        // SVG knob
        this.svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        this.svg.setAttribute('viewBox', '0 0 100 100');
        this.svg.classList.add('knob-svg');

        const bg = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        bg.setAttribute('cx', '50');
        bg.setAttribute('cy', '50');
        bg.setAttribute('r', '42');
        bg.setAttribute('fill', '#1e2430');
        bg.setAttribute('stroke', '#3a465a');
        bg.setAttribute('stroke-width', '4');
        this.svg.appendChild(bg);

        const ring = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        ring.setAttribute('cx', '50');
        ring.setAttribute('cy', '50');
        ring.setAttribute('r', '32');
        ring.setAttribute('fill', '#111723');
        ring.setAttribute('stroke', '#4a9eff');
        ring.setAttribute('stroke-width', '3');
        ring.setAttribute('opacity', '0.6');
        this.svg.appendChild(ring);

        this.pointer = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        this.pointer.setAttribute('x1', '50');
        this.pointer.setAttribute('y1', '50');
        this.pointer.setAttribute('x2', '50');
        this.pointer.setAttribute('y2', '16');
        this.pointer.setAttribute('stroke', '#4a9eff');
        this.pointer.setAttribute('stroke-width', '4');
        this.pointer.setAttribute('stroke-linecap', 'round');
        this.pointer.setAttribute('filter', 'drop-shadow(0 0 4px rgba(74,158,255,0.8))');
        this.svg.appendChild(this.pointer);

        this.wrapper.appendChild(this.labelEl);
        this.wrapper.appendChild(this.svg);
        this.wrapper.appendChild(this.valueEl);
        this.container.appendChild(this.wrapper);
    }

    _attachEvents() {
        const startDrag = (clientY) => {
            this.isDragging = true;
            this.startY = clientY;
            this.startValue = this.value;
        };

        const moveDrag = (clientY) => {
            if (!this.isDragging) return;
            const delta = (this.startY - clientY) * 0.004; // sensitivity
            const next = this._clamp(this.startValue + delta);
            if (next !== this.value) {
                this.value = next;
                this._render();
                this.onChange(this.value);
            }
        };

        const endDrag = () => {
            this.isDragging = false;
        };

        this.svg.addEventListener('mousedown', (e) => {
            startDrag(e.clientY);
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => moveDrag(e.clientY));
        document.addEventListener('mouseup', endDrag);

        this.svg.addEventListener('touchstart', (e) => {
            startDrag(e.touches[0].clientY);
            e.preventDefault();
        }, { passive: false });

        document.addEventListener('touchmove', (e) => moveDrag(e.touches[0].clientY), { passive: false });
        document.addEventListener('touchend', endDrag);

        this.svg.addEventListener('wheel', (e) => {
            e.preventDefault();
            const delta = -Math.sign(e.deltaY) * this.step * 5;
            const next = this._clamp(this.value + delta);
            if (next !== this.value) {
                this.value = next;
                this._render();
                this.onChange(this.value);
            }
        }, { passive: false });
    }

    _render() {
        const angle = -135 + (this.value * 270);
        this.pointer.setAttribute('transform', `rotate(${angle} 50 50)`);
    }

    _clamp(val) {
        return Math.min(Math.max(val, this.min), this.max);
    }

    /**
     * Update the text display under the knob (formatted by caller).
     * @param {string} text
     */
    updateValueDisplay(text) {
        this.valueEl.textContent = text;
    }

    /**
     * Programmatically set knob position (normalized 0-1) and redraw.
     * @param {number} value
     */
    setValue(value) {
        this.value = this._clamp(value);
        this._render();
    }
}
