# macOS Global Keyboard Listening

## Summary

Extended Chataigne's `Keyboard` module (`Source/Module/modules/controller/keyboard/`) on macOS (`#if JUCE_MAC`) to listen for keyboard events globally across all applications instead of only when Chataigne's main window is focused, while leaving Windows and Linux behavior unchanged.

## Changes

1. **`Source/Module/modules/controller/keyboard/KeyboardMacFunctions.h`**
   - Switched `#if JUCE_SUPPORT_CARBON` to `#if JUCE_MAC` (as `JUCE_SUPPORT_CARBON=0` in `Chataigne.xcodeproj`).
   - Added `keyboardmac::MacKeyboardTap`, a listen-only `CGEventTapCreate` (`kCGHIDEventTap`) running on a dedicated `CFRunLoop` thread for `kCGEventKeyDown`, `kCGEventKeyUp`, and `kCGEventFlagsChanged`.
   - Added `cgKeyCodeToAscii()` and `cgKeyCodeToName()` helpers for mapping non-printable/modified keycodes.

2. **`Source/Module/modules/controller/keyboard/KeyboardModule.h` & `KeyboardModule.cpp`**
   - Guarded all additions with `#if JUCE_MAC`.
   - Started `macKeyboardTap` in `KeyboardModule::KeyboardModule()`, dispatching events to `macKeyChanged()` on the JUCE message thread via `WeakReference<KeyboardModule>`, and falling back to `KeyListener` if tap creation fails (e.g. missing Input Monitoring permission).
   - Stopped and cleaned up `macKeyboardTap` in `KeyboardModule::~KeyboardModule()`.
   - Updated `ctrl`, `shift`, `command`, `alt`, `lastKey`, `lastKeyCode`, `combination`, and `keysCC` (`keyMap`) from global key events.
