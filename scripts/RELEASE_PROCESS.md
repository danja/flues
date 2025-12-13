# Flues Project Release Process

Complete guide for creating and publishing releases.

## Overview

The release process involves:
1. Building optimized binaries
2. Packaging for distribution
3. Testing the release
4. Publishing to GitHub

## Prerequisites

### Required Tools
```bash
# Build tools
sudo apt install build-essential cmake meson ninja-build

# Dependencies
sudo apt install libasound2-dev libx11-dev libcairo2-dev libgtk-4-dev

# GitHub CLI (for publishing)
sudo apt install gh
gh auth login
```

### Version Planning

Follow [Semantic Versioning](https://semver.org/):
- **MAJOR.MINOR.PATCH** (e.g., 0.1.0)
- **MAJOR**: Incompatible API changes
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, backward compatible

## Step-by-Step Release Process

### 1. Pre-Release Preparation

#### Update Version Numbers
```bash
# Update these files with new version:
- CHANGELOG.md (add new section at top)
- flues-synth/meson.build (version field)
- lv2/*/manifest.ttl (where applicable)
```

#### Update Documentation
```bash
# Review and update:
- README.md
- CHANGELOG.md
- All docs in flues-synth/docs/
- Plugin README files
```

#### Commit Changes
```bash
git add -A
git commit -m "Prepare release v0.1.0"
git push origin main
```

### 2. Build Release

```bash
# Run the build script
cd /home/danny/github/flues
./scripts/build-release.sh 0.1.0
```

**What this does:**
1. Creates `releases/v0.1.0/` directory
2. Builds all 7 LV2 plugins (release mode, -O3)
3. Builds flues-synth (release mode, -O3)
4. Runs all tests to verify
5. Copies binaries to release directory
6. Copies documentation
7. Creates install/uninstall scripts
8. Creates tarball: `releases/flues-v0.1.0-linux-x86_64.tar.gz`
9. Generates checksums (SHA256, MD5)

**Expected output:**
```
releases/
├── flues-v0.1.0-linux-x86_64.tar.gz
├── flues-v0.1.0-linux-x86_64.tar.gz.sha256
├── flues-v0.1.0-linux-x86_64.tar.gz.md5
└── v0.1.0/
    ├── lv2-plugins/
    │   ├── disyn.lv2/
    │   ├── floozy.lv2/
    │   ├── chatterbox.lv2/
    │   ├── chatgen.lv2/
    │   ├── drumkit.lv2/
    │   ├── pm-synth.lv2/
    │   └── flues-control.lv2/
    ├── flues-synth/
    │   └── flues-synth
    ├── docs/
    ├── install.sh
    ├── uninstall.sh
    └── README.md
```

### 3. Test Release

#### Quick Smoke Test
```bash
cd releases/v0.1.0

# Test installation
sudo ./install.sh

# Verify plugins
lv2ls | grep flues

# Test one plugin
jalv.gtk https://danja.github.io/flues/plugins/disyn

# Test flues-synth
flues-synth
# Press Ctrl+C to exit

# Uninstall
sudo ./uninstall.sh
```

#### Comprehensive Testing
Follow the complete testing checklist:
```bash
cat scripts/RELEASE_TESTING_CHECKLIST.md
```

**Critical tests:**
- [ ] All 7 LV2 plugins load
- [ ] Program 6 doesn't segfault (race fix verified)
- [ ] Programs 18-27 respond to param3
- [ ] flues-synth runs without crashes
- [ ] All 17 algorithms produce sound

### 4. Create Git Tag

```bash
# Tag the release
git tag -a v0.1.0 -m "Release v0.1.0 - Experimental Synthesis Plugins"

# Push tag
git push origin v0.1.0
```

### 5. Publish to GitHub

```bash
# Use the release script
./scripts/create-github-release.sh 0.1.0
```

**What this does:**
1. Checks for `gh` CLI and authentication
2. Verifies tarball exists
3. Generates release notes from template
4. Opens editor for you to customize notes
5. Creates GitHub release
6. Uploads tarball + checksums

**Manual alternative:**
```bash
# Create release manually
gh release create v0.1.0 \
    --title "Flues v0.1.0 - Experimental Synthesis Plugins" \
    --notes-file releases/v0.1.0/RELEASE_NOTES.md \
    releases/flues-v0.1.0-linux-x86_64.tar.gz \
    releases/flues-v0.1.0-linux-x86_64.tar.gz.sha256 \
    releases/flues-v0.1.0-linux-x86_64.tar.gz.md5
```

### 6. Post-Release Tasks

#### Verify Release
```bash
# View release
gh release view v0.1.0 --web

# Download and test
wget https://github.com/danja/flues/releases/download/v0.1.0/flues-v0.1.0-linux-x86_64.tar.gz
tar xzf flues-v0.1.0-linux-x86_64.tar.gz
cd flues-v0.1.0
sudo ./install.sh
```

#### Announce Release
- [ ] Update project README.md with download link
- [ ] Post to GitHub Discussions
- [ ] Update project website (if applicable)
- [ ] Notify users/community

#### Clean Up
```bash
# Optional: Remove old release builds
rm -rf releases/v0.0.1/  # Keep tarballs for archive
```

## Troubleshooting

### Build Fails

**Symptom:** `build-release.sh` exits with error

**Solutions:**
```bash
# Check dependencies
sudo apt install build-essential cmake meson libasound2-dev libx11-dev libcairo2-dev

# Clean previous builds
rm -rf lv2/*/build flues-synth/builddir

# Try building individually
cd lv2/disyn
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Tests Fail

**Symptom:** `meson test` reports failures

**Solutions:**
```bash
# Run specific test
cd flues-synth
meson test -C builddir program6-test --verbose

# Check for race conditions
meson test -C builddir --repeat=10

# View test logs
cat builddir/meson-logs/testlog.txt
```

### Plugin Not Found

**Symptom:** `lv2ls` doesn't show plugins after install

**Solutions:**
```bash
# Check installation path
ls -la /usr/local/lib/lv2/*.lv2

# Verify LV2_PATH
echo $LV2_PATH

# Manually set
export LV2_PATH=/usr/local/lib/lv2:~/.lv2

# Rebuild LV2 cache
lv2ls > /dev/null
```

### GitHub CLI Issues

**Symptom:** `gh` commands fail

**Solutions:**
```bash
# Check authentication
gh auth status

# Re-authenticate
gh auth login

# Manual upload via web UI
# Visit: https://github.com/danja/flues/releases/new
```

## Release Checklist Summary

Quick checklist before publishing:

- [ ] Version numbers updated
- [ ] CHANGELOG.md updated
- [ ] Documentation reviewed
- [ ] Changes committed and pushed
- [ ] Release built successfully (`./scripts/build-release.sh`)
- [ ] All tests passed
- [ ] Release tested on clean system
- [ ] Git tag created and pushed
- [ ] GitHub release created
- [ ] Assets uploaded
- [ ] Release notes reviewed
- [ ] Download links verified
- [ ] Announcement posted

## Common Release Scenarios

### Hotfix Release (Patch)

```bash
# Example: v0.1.0 → v0.1.1 (critical bug fix)

# 1. Fix the bug
git checkout main
# ... make fix ...
git commit -m "Fix critical issue in Program 6"

# 2. Update version
# Edit: CHANGELOG.md, meson.build
git commit -m "Bump version to 0.1.1"

# 3. Build and release
./scripts/build-release.sh 0.1.1
git tag -a v0.1.1 -m "Hotfix: Program 6 crash fix"
git push origin v0.1.1
./scripts/create-github-release.sh 0.1.1
```

### Minor Release (Features)

```bash
# Example: v0.1.0 → v0.2.0 (new features)

# 1. Develop features
git checkout -b feature/polyphony
# ... implement ...
git checkout main
git merge feature/polyphony

# 2. Update version and docs
# Edit: CHANGELOG.md, README.md, meson.build
git commit -m "Add polyphony support - bump to v0.2.0"

# 3. Build and release
./scripts/build-release.sh 0.2.0
git tag -a v0.2.0 -m "Feature release: 4-voice polyphony"
git push origin v0.2.0
./scripts/create-github-release.sh 0.2.0
```

### Pre-Release (Beta)

```bash
# Example: v0.2.0-beta.1

./scripts/build-release.sh 0.2.0-beta.1
git tag -a v0.2.0-beta.1 -m "Beta release for testing"
git push origin v0.2.0-beta.1

# Create pre-release on GitHub
gh release create v0.2.0-beta.1 \
    --prerelease \
    --title "Flues v0.2.0-beta.1 (Pre-release)" \
    --notes "Beta release for testing. Not for production use." \
    releases/flues-v0.2.0-beta.1-linux-x86_64.tar.gz
```

## Version History

- **v0.1.0** (2025-12-12) - Initial public release
  - 7 LV2 plugins
  - 17 distortion algorithms
  - flues-synth for Raspberry Pi
  - Program 6 race fix

## Resources

- **Semantic Versioning**: https://semver.org/
- **Keep a Changelog**: https://keepachangelog.com/
- **GitHub Releases**: https://docs.github.com/en/repositories/releasing-projects-on-github
- **LV2 Packaging**: https://lv2plug.in/pages/filesystem-hierarchy-standard.html

## Support

If you encounter issues with the release process:
1. Check this guide
2. Review error messages carefully
3. Test on clean system
4. File issue: https://github.com/danja/flues/issues
