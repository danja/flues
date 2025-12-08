// Discrete rotary switch. Builds its own DOM using the provided container.
export class RotarySwitchController {
    constructor(container, {
        options = [],
        value = 0,
        onChange = () => {}
    } = {}) {
        this.container = container;
        this.options = options;
        this.currentPosition = value;
        this.onChange = onChange;
        this.isDragging = false;
        this.startY = 0;
        this.startPosition = 0;

        this._buildDOM();
        this._attachEvents();
        this._render();
    }

    _buildDOM() {
        this.container.innerHTML = '';
        this.wrapper = document.createElement('div');
        this.wrapper.className = 'rotary-switch-container';

        this.labelEl = document.createElement('div');
        this.labelEl.className = 'rotary-switch-label';

        this.svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        this.svg.setAttribute('viewBox', '0 0 100 100');
        this.svg.classList.add('rotary-switch-svg');

        const base = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        base.setAttribute('cx', '50');
        base.setAttribute('cy', '50');
        base.setAttribute('r', '42');
        base.setAttribute('fill', '#1e2430');
        base.setAttribute('stroke', '#3a465a');
        base.setAttribute('stroke-width', '4');
        this.svg.appendChild(base);

        this.pointer = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        this.pointer.setAttribute('x1', '50');
        this.pointer.setAttribute('y1', '50');
        this.pointer.setAttribute('x2', '50');
        this.pointer.setAttribute('y2', '16');
        this.pointer.setAttribute('stroke', '#4a9eff');
        this.pointer.setAttribute('stroke-width', '4');
        this.pointer.setAttribute('stroke-linecap', 'round');
        this.svg.appendChild(this.pointer);

        this.wrapper.appendChild(this.svg);
        this.wrapper.appendChild(this.labelEl);
        this.container.appendChild(this.wrapper);
    }

    _attachEvents() {
        const startDrag = (clientY) => {
            this.isDragging = true;
            this.startY = clientY;
            this.startPosition = this.currentPosition;
        };

        const moveDrag = (clientY) => {
            if (!this.isDragging) return;
            const delta = Math.floor((this.startY - clientY) / 40);
            const next = Math.max(0, Math.min(this.options.length - 1, this.startPosition + delta));
            if (next !== this.currentPosition) {
                this.currentPosition = next;
                this._render();
                this.onChange(this.currentPosition);
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

        // Click to advance
        this.svg.addEventListener('click', () => {
            if (!this.isDragging) {
                this.currentPosition = (this.currentPosition + 1) % this.options.length;
                this._render();
                this.onChange(this.currentPosition);
            }
        });
    }

    _render() {
        const anglePerPosition = 270 / Math.max(1, this.options.length - 1);
        const rotation = -135 + (this.currentPosition * anglePerPosition);
        this.pointer.setAttribute('transform', `rotate(${rotation} 50 50)`);
        this.labelEl.textContent = this.options[this.currentPosition] || '';
    }
}
