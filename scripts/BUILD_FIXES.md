# Build Script Fixes (2025-12-13)

## Issue 1: Disyn Compilation Error

**Problem:**
```
/home/danny/github/flues/lv2/disyn/src/DisynEngine.hpp:52:51: error: no matching function for call to 'flues::disyn::OscillatorModule::process(flues::disyn::AlgorithmType&, float&, float&, float&)'
```

**Root Cause:**
OscillatorModule::process() signature was changed to require 5 parameters (adding param3), but DisynEngine was still calling it with only 4 parameters.

**Solution:**
Made `param3` and `frequency` optional with default values to maintain backwards compatibility:

**File:** `lv2/disyn/src/modules/OscillatorModule.hpp`
```cpp
// Before (breaking):
float process(AlgorithmType algorithm, float param1, float param2, float param3, float frequency)

// After (backwards compatible):
float process(AlgorithmType algorithm, float param1, float param2, float param3 = 0.5f, float frequency = 440.0f)
```

**Result:**
- ✅ Disyn plugin (algorithms 0-6) works without changes
- ✅ Floozy/Flues-Synth (algorithms 0-16) work with param3
- ✅ All 6 tests pass
- ✅ No breaking changes

---

## Issue 2: Build Script - No .lv2 Bundle Found

**Problem:**
```
ERROR: No .lv2 bundle found after build
```

**Root Cause:**
CMake doesn't create the `.lv2` bundle directory during the build step - it only creates individual `.so` files. The bundle is assembled during the install step.

**Solution:**
Updated `scripts/build-release.sh` to use `cmake --install` to a staging directory:

**File:** `scripts/build-release.sh`
```bash
# Before (looking in wrong place):
cmake --build build --config Release
local bundle=$(find build -name "*.lv2" -type d | head -1)

# After (install to staging, then copy):
cmake --build build --config Release
local staging_dir="$plugin_dir/build/staging"
cmake --install build --prefix "$staging_dir"
local bundle=$(find "$staging_dir" -name "*.lv2" -type d | head -1)
cp -r "$bundle" "$RELEASE_DIR/lv2-plugins/"
```

**What the install step does:**
1. Creates `build/staging/disyn.lv2/` directory
2. Copies `disyn.so` (plugin binary)
3. Copies `disyn_ui.so` (UI binary)
4. Copies `manifest.ttl` (LV2 manifest)
5. Copies `disyn.ttl` (plugin metadata)

**Result:**
- ✅ Complete .lv2 bundles created
- ✅ All files included (plugin, UI, TTL files)
- ✅ Ready for packaging and distribution

---

## Verification

### Build Test
```bash
cd /home/danny/github/flues/lv2/disyn
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix build/staging
ls -la build/staging/disyn.lv2/
```

**Expected output:**
```
disyn.so       (plugin binary)
disyn_ui.so    (UI binary)
disyn.ttl      (plugin metadata)
manifest.ttl   (LV2 manifest)
```

### Full Release Test
```bash
cd /home/danny/github/flues
./scripts/build-release.sh 0.1.0
```

**Expected output:**
- 7 LV2 plugins built and packaged
- flues-synth built and packaged
- Tarball created: `releases/flues-v0.1.0-linux-x86_64.tar.gz`
- All tests passing

---

## Impact on Release Process

**Before fixes:**
- Compilation failed on Disyn
- Build script couldn't find .lv2 bundles
- Release process blocked

**After fixes:**
- All plugins compile successfully
- All .lv2 bundles created correctly
- Full backwards compatibility maintained
- Ready for release

---

## Testing Checklist

- [x] Disyn plugin builds
- [x] Floozy plugin builds
- [x] Flues-synth builds
- [x] All 6 tests pass
- [x] Program 6 stable (no segfault)
- [x] Programs 18-27 respond to param3
- [x] .lv2 bundles complete with all files
- [x] Build script creates proper directory structure

---

## Notes

### Backwards Compatibility Strategy

The default value approach for optional parameters is preferred over:
- Function overloading (would require duplicate code)
- Wrapper functions (would add complexity)
- Conditional compilation (would fragment codebase)

Default parameters provide:
- Clean, single implementation
- No code duplication
- Clear upgrade path
- Minimal changes to existing code

### LV2 Bundle Structure

A complete LV2 bundle requires:
```
plugin.lv2/
├── plugin.so       (required - plugin binary)
├── plugin_ui.so    (optional - UI binary)
├── manifest.ttl    (required - LV2 manifest)
└── plugin.ttl      (required - plugin metadata)
```

CMake's install step ensures all required files are assembled into the bundle directory with correct permissions and paths.
