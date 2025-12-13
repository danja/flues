# Flues Project Scripts

This directory contains build, release, and utility scripts for the Flues Project.

## Release Scripts

### build-release.sh
**Purpose:** Build optimized release packages for GitHub distribution

**Usage:**
```bash
./scripts/build-release.sh [VERSION]
```

**Example:**
```bash
./scripts/build-release.sh 0.1.0
```

**What it does:**
- Builds all 7 LV2 plugins in release mode (-O3 -march=native)
- Builds flues-synth in release mode
- Runs all tests to verify build
- Creates release directory structure
- Packages everything into tarball
- Generates checksums (SHA256 + MD5)
- Creates install/uninstall scripts

**Output:**
- `releases/v{VERSION}/` - Release directory
- `releases/flues-v{VERSION}-{OS}-{ARCH}.tar.gz` - Distribution tarball
- `releases/flues-v{VERSION}-{OS}-{ARCH}.tar.gz.sha256` - SHA256 checksum
- `releases/flues-v{VERSION}-{OS}-{ARCH}.tar.gz.md5` - MD5 checksum

---

### create-github-release.sh
**Purpose:** Create GitHub release and upload artifacts

**Usage:**
```bash
./scripts/create-github-release.sh VERSION
```

**Example:**
```bash
./scripts/create-github-release.sh 0.1.0
```

**Prerequisites:**
- GitHub CLI installed: `sudo apt install gh`
- Authenticated: `gh auth login`
- Release built: `./scripts/build-release.sh 0.1.0`
- Git tag created: `git tag v0.1.0 && git push origin v0.1.0`

**What it does:**
- Verifies prerequisites (gh, authentication, tarball exists)
- Generates release notes from template
- Opens editor for customization
- Creates GitHub release
- Uploads tarball + checksums
- Opens release in browser

---

## Documentation

### RELEASE_PROCESS.md
Complete step-by-step guide for creating releases. Covers:
- Pre-release preparation (version updates, docs)
- Building releases
- Testing releases
- Publishing to GitHub
- Post-release tasks
- Troubleshooting

**Read this first** before creating a release.

---

### RELEASE_TESTING_CHECKLIST.md
Comprehensive testing checklist for releases. Includes:
- Build process verification
- Installation tests (system + user)
- All 7 LV2 plugin tests
- flues-synth tests
- New algorithm tests (param3)
- Documentation verification
- Platform-specific tests
- Integration tests (Ardour, Reaper, Carla)

**Use this** to verify release before publishing.

---

### release-notes-template.md
Template for GitHub release notes. Automatically filled by `create-github-release.sh`.

Includes:
- Overview
- What's new
- Component descriptions
- Installation instructions
- Usage examples
- Documentation links
- Known issues
- Changelog

Edit this template to customize release notes format.

---

### QUICKSTART.md
User-facing quick start guide for new users. Will be included in release packages.

Covers:
- Prerequisites and installation
- Verification steps
- Usage examples for each plugin
- Recommended DAW workflows
- Troubleshooting
- Next steps

Can be used standalone or linked in release notes.

---

## Workflow

### Typical Release Workflow

1. **Prepare release**
   ```bash
   # Update version in files
   vim CHANGELOG.md
   vim flues-synth/meson.build

   # Commit changes
   git add -A
   git commit -m "Prepare release v0.1.0"
   git push origin main
   ```

2. **Build release**
   ```bash
   ./scripts/build-release.sh 0.1.0
   ```

3. **Test release**
   ```bash
   # Follow RELEASE_TESTING_CHECKLIST.md
   cd releases/v0.1.0
   sudo ./install.sh
   # ... test plugins and flues-synth ...
   sudo ./uninstall.sh
   ```

4. **Create Git tag**
   ```bash
   git tag -a v0.1.0 -m "Release v0.1.0"
   git push origin v0.1.0
   ```

5. **Publish to GitHub**
   ```bash
   ./scripts/create-github-release.sh 0.1.0
   # Edit release notes in editor
   # Release created automatically
   ```

6. **Verify**
   ```bash
   # Check GitHub release page
   gh release view v0.1.0 --web

   # Download and test
   wget https://github.com/danja/flues/releases/download/v0.1.0/flues-v0.1.0-linux-x86_64.tar.gz
   tar xzf flues-v0.1.0-linux-x86_64.tar.gz
   cd flues-v0.1.0
   sudo ./install.sh
   ```

## File Descriptions

| File | Purpose | When to Use |
|------|---------|-------------|
| `build-release.sh` | Build and package release | Every release |
| `create-github-release.sh` | Publish to GitHub | After build + test |
| `RELEASE_PROCESS.md` | Complete release guide | Reference |
| `RELEASE_TESTING_CHECKLIST.md` | Testing checklist | Before publishing |
| `release-notes-template.md` | Release notes template | Auto-used by script |
| `QUICKSTART.md` | User quick start guide | Include in releases |

## Tips

### Testing Before Release

Always test on a **clean system** before publishing:
```bash
# Option 1: Fresh VM
# Create Ubuntu/Debian VM, install minimal deps, test

# Option 2: Clean user account
sudo useradd -m testuser
sudo su - testuser
# Install and test as testuser

# Option 3: Docker
docker run -it ubuntu:24.04
apt update && apt install ...
# Test installation
```

### Updating Documentation

Before each release, review:
- [ ] README.md (project overview, quick start)
- [ ] CHANGELOG.md (new version section)
- [ ] All plugin README files
- [ ] flues-synth/docs/ (algorithm, MIDI, program docs)

### Version Numbering

Follow [Semantic Versioning](https://semver.org/):
- **0.1.0** → **0.1.1** : Bug fixes (PATCH)
- **0.1.0** → **0.2.0** : New features (MINOR)
- **0.1.0** → **1.0.0** : Breaking changes (MAJOR)

### Pre-releases

For beta/RC releases:
```bash
./scripts/build-release.sh 0.2.0-beta.1
git tag v0.2.0-beta.1
./scripts/create-github-release.sh 0.2.0-beta.1
# Mark as pre-release on GitHub
```

## Troubleshooting

### "build-release.sh: command not found"
```bash
chmod +x scripts/build-release.sh
chmod +x scripts/create-github-release.sh
```

### Build fails
```bash
# Check dependencies
sudo apt install build-essential cmake meson ninja-build \
    libasound2-dev libx11-dev libcairo2-dev

# Clean old builds
rm -rf lv2/*/build flues-synth/builddir

# Try building manually
cd lv2/disyn
cmake -S . -B build
cmake --build build
```

### GitHub CLI issues
```bash
# Check installation
gh --version

# Install
sudo apt install gh

# Authenticate
gh auth login
```

## Support

- Documentation: https://danja.github.io/flues/
- Issues: https://github.com/danja/flues/issues
- Discussions: https://github.com/danja/flues/discussions
