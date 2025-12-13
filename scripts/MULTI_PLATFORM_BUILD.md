# Multi-Platform Build Process

Guide for building Flues releases across Ubuntu desktop and Raspberry Pi.

## Overview

The Flues project consists of two main components with different target platforms:

- **LV2 Plugins** → Ubuntu desktop (x86_64) - for use in DAWs
- **Flues-Synth** → Raspberry Pi (aarch64/armv7l) - headless synthesizer

This requires a two-stage build process across different machines.

## Architecture

```
┌─────────────────────────┐
│   Ubuntu Desktop        │
│   (x86_64)              │
│                         │
│   Build:                │
│   - 7 LV2 plugins       │
│   - Documentation       │
│                         │
│   Output:               │
│   → plugins tarball     │
└─────────────────────────┘
           │
           │ transfer
           ▼
┌─────────────────────────┐
│   Raspberry Pi 4        │
│   (aarch64)             │
│                         │
│   Build:                │
│   - flues-synth binary  │
│                         │
│   Output:               │
│   → synth tarball       │
└─────────────────────────┘
           │
           │ transfer back
           ▼
┌─────────────────────────┐
│   Ubuntu Desktop        │
│                         │
│   Assemble:             │
│   - Combine both builds │
│   - Create final release│
│                         │
│   Output:               │
│   → multi-arch tarball  │
└─────────────────────────┘
```

## Step-by-Step Process

### Prerequisites

**Ubuntu Desktop:**
```bash
sudo apt install build-essential cmake meson ninja-build \
    libasound2-dev libx11-dev libcairo2-dev gh
```

**Raspberry Pi:**
```bash
sudo apt install build-essential meson ninja-build \
    libasound2-dev
```

---

### Step 1: Build Plugins on Desktop

**Location:** Ubuntu desktop

```bash
cd /home/danny/github/flues
./scripts/build-plugins.sh 0.1.0
```

**What this does:**
- Builds all 7 LV2 plugins with desktop optimization
- Creates `build-output/plugins-v0.1.0/` with .lv2 bundles
- Creates `build-output/flues-plugins-v0.1.0-linux-x86_64.tar.gz`

**Output:**
```
build-output/
├── plugins-v0.1.0/
│   ├── disyn.lv2/
│   ├── floozy.lv2/
│   ├── chatterbox.lv2/
│   ├── chatgen.lv2/
│   ├── drumkit.lv2/
│   ├── pm-synth.lv2/
│   └── flues-control.lv2/
└── flues-plugins-v0.1.0-linux-x86_64.tar.gz
```

---

### Step 2: Transfer to Raspberry Pi

**Location:** Ubuntu desktop

**Option A: Transfer whole repo**
```bash
# Sync entire repo (recommended for first time)
rsync -avz --exclude 'build*' --exclude '.git' \
    /home/danny/github/flues/ \
    pi@raspberrypi.local:~/flues/
```

**Option B: Transfer just source code**
```bash
# Create source tarball
cd /home/danny/github/flues
git archive --format=tar.gz --output=flues-src.tar.gz HEAD

# Transfer
scp flues-src.tar.gz pi@raspberrypi.local:~/

# On Pi, extract
ssh pi@raspberrypi.local
tar xzf flues-src.tar.gz
```

---

### Step 3: Build Synth on Raspberry Pi

**Location:** Raspberry Pi

```bash
ssh pi@raspberrypi.local
cd ~/flues
./scripts/build-synth-pi.sh 0.1.0
```

**What this does:**
- Builds flues-synth with Pi-specific optimization (-mcpu=cortex-a72)
- Runs all 6 tests on Pi hardware
- Strips debug symbols
- Creates `build-output/synth-v0.1.0/` with binary + docs
- Creates `build-output/flues-synth-v0.1.0-linux-aarch64.tar.gz`

**Output:**
```
build-output/
├── synth-v0.1.0/
│   ├── flues-synth (ARM binary, stripped)
│   └── docs/
└── flues-synth-v0.1.0-linux-aarch64.tar.gz
```

**Optimization flags:**
```
-O3                        # Maximum optimization
-mcpu=cortex-a72          # Raspberry Pi 4 CPU
-mfpu=neon-fp-armv8       # NEON SIMD instructions
-DNDEBUG                  # Remove debug code
```

---

### Step 4: Transfer Build Back to Desktop

**Location:** Ubuntu desktop

```bash
# Download synth tarball from Pi
scp pi@raspberrypi.local:~/flues/build-output/flues-synth-v0.1.0-linux-aarch64.tar.gz \
    /home/danny/github/flues/build-output/
```

**Verify transfer:**
```bash
cd /home/danny/github/flues/build-output
ls -lh flues-synth-v0.1.0-linux-aarch64.tar.gz
# Should be ~500KB - 2MB
```

---

### Step 5: Assemble Final Release

**Location:** Ubuntu desktop

```bash
cd /home/danny/github/flues
./scripts/assemble-release.sh 0.1.0
```

**What this does:**
- Verifies both builds are present
- Creates unified release directory structure
- Combines plugins (desktop) + synth (Pi)
- Generates install/uninstall scripts
- Creates 3 tarballs:
  1. **Full multi-arch** - plugins + synth
  2. **Plugins only** - desktop users
  3. **Synth only** - Pi users
- Generates checksums for all tarballs

**Output:**
```
releases/
├── v0.1.0/
│   ├── lv2-plugins/      (from desktop build)
│   ├── flues-synth/      (from Pi build)
│   ├── docs/
│   ├── install.sh
│   ├── uninstall.sh
│   └── README.md
├── flues-v0.1.0-multi-arch.tar.gz          (14MB)
├── flues-plugins-v0.1.0-x86_64.tar.gz      (12MB)
├── flues-synth-v0.1.0-aarch64.tar.gz       (2MB)
└── *.sha256, *.md5 checksums
```

---

### Step 6: Publish to GitHub

**Location:** Ubuntu desktop

```bash
# Create git tag
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0

# Create GitHub release
./scripts/create-github-release.sh 0.1.0
```

**Manually upload additional tarballs:**
```bash
gh release upload v0.1.0 \
    releases/flues-plugins-v0.1.0-x86_64.tar.gz \
    releases/flues-synth-v0.1.0-aarch64.tar.gz
```

---

## Quick Reference

### Desktop Commands (Ubuntu)
```bash
# 1. Build plugins
./scripts/build-plugins.sh 0.1.0

# 2. Transfer to Pi
rsync -avz --exclude 'build*' flues/ pi@raspberrypi.local:~/flues/

# 3. Wait for Pi build...

# 4. Download Pi build
scp pi@raspberrypi.local:~/flues/build-output/flues-synth-*.tar.gz build-output/

# 5. Assemble release
./scripts/assemble-release.sh 0.1.0

# 6. Publish
git tag v0.1.0 && git push origin v0.1.0
./scripts/create-github-release.sh 0.1.0
```

### Raspberry Pi Commands
```bash
# 1. Build synth (after receiving code from desktop)
cd ~/flues
./scripts/build-synth-pi.sh 0.1.0

# 2. Verify build
./build-output/synth-v0.1.0/flues-synth --help

# Done - desktop will download the tarball
```

---

## Optimization Details

### Desktop (x86_64)
```bash
-O3               # Aggressive optimization
-march=native     # Use all CPU features (AVX, SSE, etc.)
-DNDEBUG          # Remove assertions
```

**Why -march=native?**
- Plugins run on same machine where built
- Can use latest CPU instructions
- ~10-20% performance improvement

### Raspberry Pi (aarch64)
```bash
-O3                      # Aggressive optimization
-mcpu=cortex-a72        # Pi 4 specific
-mfpu=neon-fp-armv8     # SIMD vectorization
-DNDEBUG                # Remove assertions
```

**Why cortex-a72?**
- Raspberry Pi 4 specific CPU model
- Enables ARM-specific optimizations
- Critical for real-time audio (48kHz, <10ms latency)

---

## Troubleshooting

### "No synth tarball found"

**Problem:** assemble-release.sh can't find Pi build

**Solution:**
```bash
# Check what's in build-output
ls -la build-output/

# Should have both:
# - plugins-v0.1.0/
# - flues-synth-v0.1.0-linux-aarch64.tar.gz

# If missing synth tarball, rebuild on Pi
ssh pi@raspberrypi.local
cd ~/flues && ./scripts/build-synth-pi.sh 0.1.0
exit

# Download again
scp pi@raspberrypi.local:~/flues/build-output/flues-synth-*.tar.gz build-output/
```

### "Wrong architecture" warning

**Problem:** Running Pi script on desktop (or vice versa)

**Solution:** Scripts check architecture automatically:
- `build-plugins.sh` - any architecture (builds for host)
- `build-synth-pi.sh` - warns if not ARM
- `assemble-release.sh` - works on any (just assembles)

### SSH/rsync issues

**Setup passwordless SSH:**
```bash
# On desktop
ssh-keygen -t ed25519
ssh-copy-id pi@raspberrypi.local

# Test
ssh pi@raspberrypi.local hostname
# Should print: raspberrypi (without password prompt)
```

### Build failures on Pi

**Out of memory:**
```bash
# Check memory
free -h

# Reduce Meson jobs
meson setup builddir -Dmax-jobs=2
```

**Missing dependencies:**
```bash
sudo apt install build-essential meson libasound2-dev
```

---

## Alternative: Cross-Compilation (Advanced)

For faster iteration, you can cross-compile on desktop:

```bash
# Install cross-compiler
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Cross-compile (experimental)
cd flues-synth
meson setup builddir-pi --cross-file=../scripts/aarch64-cross.ini
meson compile -C builddir-pi
```

**Note:** This is experimental and may have issues. Native Pi build is recommended.

---

## File Transfer Optimization

### Using rsync (recommended)
```bash
# Only transfer changed files
rsync -avz --delete \
    --exclude 'build*' \
    --exclude '.git' \
    --exclude '*.tar.gz' \
    flues/ pi@raspberrypi.local:~/flues/
```

### Using git (if Pi has internet)
```bash
# On desktop: push to GitHub
git push origin main

# On Pi: pull updates
ssh pi@raspberrypi.local
cd ~/flues
git pull origin main
```

---

## Testing Checklist

**After desktop build:**
- [ ] All 7 .lv2 bundles created
- [ ] Each bundle has .so + _ui.so + .ttl files
- [ ] Plugins load in jalv/Ardour

**After Pi build:**
- [ ] flues-synth binary is ARM architecture
- [ ] All 6 tests pass
- [ ] Binary runs without errors
- [ ] MIDI input works
- [ ] Audio output works

**After assembly:**
- [ ] 3 tarballs created with checksums
- [ ] install.sh/uninstall.sh scripts present
- [ ] README.md reflects both architectures
- [ ] Documentation complete

---

## Continuous Integration (Future)

For automated builds, consider:

1. **GitHub Actions** for desktop plugins (x86_64)
2. **Self-hosted runner** on Pi for synth builds
3. **Artifact upload** to combine builds

Example workflow:
```yaml
# .github/workflows/release.yml
jobs:
  build-plugins:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - run: ./scripts/build-plugins.sh $VERSION
      - uses: actions/upload-artifact@v3
        with:
          name: plugins
          path: build-output/plugins-*

  build-synth:
    runs-on: [self-hosted, ARM64, pi]
    steps:
      - uses: actions/checkout@v3
      - run: ./scripts/build-synth-pi.sh $VERSION
      - uses: actions/upload-artifact@v3
        with:
          name: synth
          path: build-output/synth-*

  assemble:
    needs: [build-plugins, build-synth]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v3
      - run: ./scripts/assemble-release.sh $VERSION
```

---

## Summary

**Build time:**
- Desktop plugins: ~5 minutes
- Pi synth: ~10 minutes
- Assembly: <1 minute
- **Total: ~15 minutes**

**Transfer size:**
- Source code: ~2MB
- Pi build result: ~2MB
- **Total transfer: ~4MB**

**Final deliverables:**
- Full release: ~14MB (all components)
- Plugins only: ~12MB (desktop users)
- Synth only: ~2MB (Pi users)

This multi-platform approach ensures:
✓ Optimal performance on each platform
✓ Correct architecture binaries
✓ Native testing on target hardware
✓ Flexible distribution options
