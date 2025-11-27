// joystick.spec.js
// Integration test for joystick interaction

import { describe, it, expect, beforeEach, vi } from 'vitest';
import { JoystickControl } from '../src/ui/JoystickControl.js';

describe('JoystickControl - Random Click Simulation', () => {
  let canvas;
  let joystick;
  let mockOnInput;
  let mockOnStart;
  let mockOnEnd;

  beforeEach(() => {
    // Create a mock canvas element
    canvas = document.createElement('canvas');
    canvas.width = 500;
    canvas.height = 350;

    // Mock canvas context
    const mockCtx = {
      fillStyle: '',
      strokeStyle: '',
      lineWidth: 0,
      font: '',
      textAlign: '',
      textBaseline: '',
      fillRect: vi.fn(),
      strokeRect: vi.fn(),
      beginPath: vi.fn(),
      moveTo: vi.fn(),
      lineTo: vi.fn(),
      arc: vi.fn(),
      fill: vi.fn(),
      stroke: vi.fn(),
      closePath: vi.fn(),
      fillText: vi.fn(),
      setLineDash: vi.fn(),
    };

    canvas.getContext = vi.fn(() => mockCtx);

    // Mock getBoundingClientRect
    canvas.getBoundingClientRect = vi.fn(() => ({
      left: 0,
      top: 0,
      width: 500,
      height: 350,
    }));

    // Mock callbacks
    mockOnInput = vi.fn();
    mockOnStart = vi.fn();
    mockOnEnd = vi.fn();

    joystick = new JoystickControl({
      canvas,
      width: 500,
      height: 350,
      onInput: mockOnInput,
      onStart: mockOnStart,
      onEnd: mockOnEnd,
    });
  });

  it('should trigger callbacks on simulated click', () => {
    const x = 250; // Center
    const y = 175; // Center

    // Simulate mousedown
    const mouseDownEvent = new MouseEvent('mousedown', {
      clientX: x,
      clientY: y,
      bubbles: true,
    });
    canvas.dispatchEvent(mouseDownEvent);

    expect(mockOnStart).toHaveBeenCalledTimes(1);
    expect(mockOnInput).toHaveBeenCalledWith(0.5, 0.5); // Center normalized
  });

  it('should handle random clicks across the canvas', () => {
    const numClicks = 10;

    for (let i = 0; i < numClicks; i++) {
      // Generate random position
      const x = Math.random() * 500;
      const y = Math.random() * 350;

      // Simulate mousedown
      const mouseDownEvent = new MouseEvent('mousedown', {
        clientX: x,
        clientY: y,
        bubbles: true,
      });
      canvas.dispatchEvent(mouseDownEvent);

      // Simulate mouseup
      const mouseUpEvent = new MouseEvent('mouseup', {
        bubbles: true,
      });
      window.dispatchEvent(mouseUpEvent);

      // Check normalized values are in range
      const lastCall = mockOnInput.mock.calls[mockOnInput.mock.calls.length - 1];
      expect(lastCall[0]).toBeGreaterThanOrEqual(0);
      expect(lastCall[0]).toBeLessThanOrEqual(1);
      expect(lastCall[1]).toBeGreaterThanOrEqual(0);
      expect(lastCall[1]).toBeLessThanOrEqual(1);
    }

    expect(mockOnStart).toHaveBeenCalledTimes(numClicks);
    expect(mockOnEnd).toHaveBeenCalledTimes(numClicks);
  });

  it('should map vowel positions correctly', () => {
    // Test vowel positions from the IPA quadrilateral
    const vowelPositions = [
      { name: 'i', x: 0.1, y: 0.1 },   // High front
      { name: 'e', x: 0.2, y: 0.35 },  // Mid front
      { name: 'a', x: 0.4, y: 0.85 },  // Low central
      { name: 'o', x: 0.75, y: 0.4 },  // Mid back
      { name: 'u', x: 0.85, y: 0.15 }, // High back
    ];

    vowelPositions.forEach((vowel) => {
      const x = vowel.x * 500;
      const y = vowel.y * 350;

      const mouseDownEvent = new MouseEvent('mousedown', {
        clientX: x,
        clientY: y,
        bubbles: true,
      });
      canvas.dispatchEvent(mouseDownEvent);

      const mouseUpEvent = new MouseEvent('mouseup', {
        bubbles: true,
      });
      window.dispatchEvent(mouseUpEvent);
    });

    expect(mockOnInput.mock.calls.length).toBeGreaterThanOrEqual(5);
  });

  it('should handle drag gestures', () => {
    // Start at top-left (i)
    const startX = 50;
    const startY = 35;

    const mouseDownEvent = new MouseEvent('mousedown', {
      clientX: startX,
      clientY: startY,
      bubbles: true,
    });
    canvas.dispatchEvent(mouseDownEvent);

    expect(mockOnStart).toHaveBeenCalledTimes(1);

    // Drag to bottom-center (a)
    const steps = 20;
    for (let i = 1; i <= steps; i++) {
      const x = startX + (200 - startX) * (i / steps);
      const y = startY + (297.5 - startY) * (i / steps);

      const mouseMoveEvent = new MouseEvent('mousemove', {
        clientX: x,
        clientY: y,
        bubbles: true,
      });
      window.dispatchEvent(mouseMoveEvent);
    }

    // Release
    const mouseUpEvent = new MouseEvent('mouseup', {
      bubbles: true,
    });
    window.dispatchEvent(mouseUpEvent);

    expect(mockOnEnd).toHaveBeenCalledTimes(1);
    expect(mockOnInput.mock.calls.length).toBeGreaterThan(steps);
  });
});
