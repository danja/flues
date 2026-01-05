const TWO_PI = Math.PI * 2;
const EPSILON = 1e-6;
const OUTPUT_GAIN = 1.0;

export class TrajectoryOscillator {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.sides = 6;
    this.startAngle = 0;
    this.startPositionAngle = 0;
    this.frequency = 440;
    this.speed = this.computeSpeed(this.frequency);
    this.vertices = [];
    this.edges = [];
    this.position = { x: 0, y: 0 };
    this.velocity = { x: this.speed, y: 0 };
    this.mixX = 0;
    this.mixY = 1;
    this.rebuildPolygon();
    this.reset();
  }

  reset() {
    this.resetPosition();
    this.updateVelocity();
  }

  configure(params = {}) {
    const sides = this.clampInt(params.sides?.mapped ?? 6, 3, 12);
    const startPositionAngle = this.degToRad(params.startPosition?.mapped ?? 0);
    const startAngle = this.degToRad(params.startAngle?.mapped ?? 0);
    this.mixX = params.mixX?.mapped ?? 0;
    this.mixY = params.mixY?.mapped ?? 1;

    const needsRebuild = sides !== this.sides;
    const needsReset =
      needsRebuild ||
      Math.abs(startPositionAngle - this.startPositionAngle) > EPSILON ||
      Math.abs(startAngle - this.startAngle) > EPSILON;

    this.sides = sides;
    this.startPositionAngle = startPositionAngle;
    this.startAngle = startAngle;

    if (needsRebuild) {
      this.rebuildPolygon();
    }
    if (needsReset) {
      this.resetPosition();
      this.updateVelocity();
    }
  }

  setFrequency(frequency) {
    this.frequency = frequency ?? this.frequency;
    this.speed = this.computeSpeed(this.frequency);
    this.updateVelocity();
  }

  process() {
    if (!this.edges.length) {
      return 0;
    }

    let position = this.position;
    let velocity = this.velocity;

    for (let bounce = 0; bounce < 2; bounce++) {
      const next = {
        x: position.x + velocity.x,
        y: position.y + velocity.y,
      };

      if (this.isInside(next)) {
        position = next;
        break;
      }

      const hit = this.findPenetrationEdge(next);
      if (!hit) {
        position = next;
        break;
      }

      const reflected = this.reflect(velocity, hit.normal);
      const nudge = 1e-4;
      position = {
        x: next.x - hit.normal.x * (hit.distance + nudge),
        y: next.y - hit.normal.y * (hit.distance + nudge),
      };
      velocity = reflected;
    }

    this.position = position;
    this.velocity = velocity;

    const output = this.position.x * this.mixX + this.position.y * this.mixY;
    return output * OUTPUT_GAIN;
  }

  computeSpeed(frequency) {
    return (frequency * 4) / this.sampleRate;
  }

  rebuildPolygon() {
    this.vertices = [];
    const rotation = Math.PI / this.sides;

    for (let i = 0; i < this.sides; i++) {
      const theta = (TWO_PI * i) / this.sides + rotation;
      this.vertices.push({
        x: Math.cos(theta),
        y: Math.sin(theta),
      });
    }

    this.edges = this.vertices.map((start, index) => {
      const end = this.vertices[(index + 1) % this.vertices.length];
      const edge = {
        x: end.x - start.x,
        y: end.y - start.y,
      };
      const normal = this.normalize({
        x: edge.y,
        y: -edge.x,
      });
      return { start, end, normal };
    });
  }

  resetPosition() {
    const dir = {
      x: Math.cos(this.startPositionAngle),
      y: Math.sin(this.startPositionAngle),
    };
    const hit = this.findRayIntersection(dir);
    if (hit) {
      this.position = {
        x: hit.x * 0.995,
        y: hit.y * 0.995,
      };
    } else {
      this.position = { x: 0, y: 0 };
    }
  }

  updateVelocity() {
    const dir = {
      x: Math.cos(this.startAngle),
      y: Math.sin(this.startAngle),
    };
    this.velocity = {
      x: dir.x * this.speed,
      y: dir.y * this.speed,
    };
  }

  findRayIntersection(direction) {
    let closest = null;

    for (const edge of this.edges) {
      const hit = this.intersectRaySegment(
        { x: 0, y: 0 },
        direction,
        edge.start,
        edge.end
      );
      if (!hit) continue;
      if (!closest || hit.t < closest.t) {
        closest = hit;
      }
    }

    return closest ? closest.point : null;
  }

  intersectRaySegment(origin, direction, start, end) {
    const segment = {
      x: end.x - start.x,
      y: end.y - start.y,
    };
    const denom = this.cross(direction, segment);
    if (Math.abs(denom) < EPSILON) {
      return null;
    }

    const toStart = {
      x: start.x - origin.x,
      y: start.y - origin.y,
    };
    const t = this.cross(toStart, segment) / denom;
    const u = this.cross(toStart, direction) / denom;

    if (t >= 0 && u >= 0 && u <= 1) {
      return {
        t,
        point: {
          x: origin.x + direction.x * t,
          y: origin.y + direction.y * t,
        },
      };
    }

    return null;
  }

  findSegmentIntersection(start, end) {
    const movement = {
      x: end.x - start.x,
      y: end.y - start.y,
    };
    let closest = null;

    for (const edge of this.edges) {
      const outwardDot = movement.x * edge.normal.x + movement.y * edge.normal.y;
      if (outwardDot <= 0) {
        continue;
      }

      const segment = {
        x: edge.end.x - edge.start.x,
        y: edge.end.y - edge.start.y,
      };
      const denom = this.cross(movement, segment);
      if (Math.abs(denom) < EPSILON) {
        continue;
      }

      const toStart = {
        x: edge.start.x - start.x,
        y: edge.start.y - start.y,
      };
      const t = this.cross(toStart, segment) / denom;
      const u = this.cross(toStart, movement) / denom;

      if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        if (!closest || t < closest.t) {
          closest = {
            t,
            point: {
              x: start.x + movement.x * t,
              y: start.y + movement.y * t,
            },
            normal: edge.normal,
          };
        }
      }
    }

    return closest;
  }

  findPenetrationEdge(point) {
    let worst = null;

    for (const edge of this.edges) {
      const toPoint = {
        x: point.x - edge.start.x,
        y: point.y - edge.start.y,
      };
      const distance = toPoint.x * edge.normal.x + toPoint.y * edge.normal.y;
      if (distance > 0 && (!worst || distance > worst.distance)) {
        worst = { distance, normal: edge.normal };
      }
    }

    return worst;
  }

  isInside(point) {
    for (const edge of this.edges) {
      const edgeVector = {
        x: edge.end.x - edge.start.x,
        y: edge.end.y - edge.start.y,
      };
      const toPoint = {
        x: point.x - edge.start.x,
        y: point.y - edge.start.y,
      };
      if (this.cross(edgeVector, toPoint) < -EPSILON) {
        return false;
      }
    }
    return true;
  }

  reflect(vector, normal) {
    const dot = vector.x * normal.x + vector.y * normal.y;
    return {
      x: vector.x - 2 * dot * normal.x,
      y: vector.y - 2 * dot * normal.y,
    };
  }

  normalize(vector) {
    const magnitude = Math.hypot(vector.x, vector.y);
    if (magnitude < EPSILON) {
      return { x: 0, y: 0 };
    }
    return {
      x: vector.x / magnitude,
      y: vector.y / magnitude,
    };
  }

  clampInt(value, min, max) {
    return Math.min(Math.max(Math.round(value), min), max);
  }

  degToRad(degrees) {
    return (degrees * Math.PI) / 180;
  }

  cross(a, b) {
    return a.x * b.y - a.y * b.x;
  }
}
