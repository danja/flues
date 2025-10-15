Problem
The clarinet synthesizer was silent on iOS Safari and Chrome (WebKit), even though the UI appeared to work correctly. This is a common issue with Web Audio API on iOS, which requires audio contexts to be "unlocked" during a user gesture.

Root Cause
While the codebase had an iOS unlock mechanism (_installIOSUnlock()), it only attached event listeners for future interactions. The unlock happened like this:

User taps power button → initialize() called
AudioContext created → _installIOSUnlock() called
Event listeners attached for unlock
Power button gesture completes ❌
User would need a second interaction to trigger unlock
The first user gesture (power button click) was being wasted because the unlock listeners were attached after the click event had already propagated.

Solution
Modified the iOS unlock mechanism to trigger immediately during initialization:

experiments/clarinet-synth/src/audio/ClarinetProcessor.js:

async _installIOSUnlock(ctx) {
    // ... setup listeners for future interactions ...
    
    // NEW: Try to unlock immediately if we're in a user gesture
    await unlock();
}
This ensures that when the power button is clicked:

AudioContext is created
iOS unlock listeners are attached for future needs
Unlock is immediately attempted during the same gesture ✅
A 1-frame silent buffer is created and played, satisfying iOS requirements
AudioContext is fully unlocked and ready for audio
Changes
Made _installIOSUnlock() async to allow awaiting the unlock operation
Added immediate await unlock() call at the end of _installIOSUnlock()
Updated initialize() to properly await the iOS unlock setup
Fixed vite.config.js by removing watch: {} which caused builds to hang
Testing
The fix has been built and deployed to www/clarinet-synth/ for GitHub Pages. On iOS devices:

Tap the power button (PWR) to initialize audio
The AudioContext will unlock during this first tap
Subsequent keyboard presses will produce audio normally