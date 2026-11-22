# macOS Mouse Buttons 4–32 Support

## Summary

Extended Chataigne's `Mouse` module (`Source/Module/modules/controller/mouse/`) on macOS (`#if JUCE_MAC`) to monitor and simulate all 32 CoreGraphics mouse buttons (`0..31`, including buttons 4–32), while leaving Windows and Linux behavior unchanged.

## Changes

1. **`Source/Module/modules/controller/mouse/MouseMacFunctions.h`**
   - Updated `mousemac::sendMouseEvent(...)` with an optional `buttonNumber` parameter; for `buttonNumber >= 3`, passes `(CGMouseButton)buttonNumber` and sets `kCGMouseEventButtonNumber` before posting via `CGEventPost(kCGHIDEventTap, ...)`.
   - Added `mousemac::MacMouseButtonTap`, a listen-only `CGEventTapCreate` (`kCGHIDEventTap`) running on a dedicated `CFRunLoop` thread for `kCGEventLeftMouseDown/Up`, `kCGEventRightMouseDown/Up`, and `kCGEventOtherMouseDown/Up`.

2. **`Source/Module/modules/controller/mouse/MouseModule.h` & `MouseModule.cpp`**
   - Guarded all additions with `#if JUCE_MAC`.
   - Added an `Extra Buttons` container (`macExtraButtonsCC`) with `Button 4`..`Button 32` `BoolParameter`s, and wired `macMouseButtonChanged(int buttonNumber, bool pressed)` to update `leftButtonDown`, `rightButtonDown`, `middleButtonDown`, `extraButton1`, `extraButton2`, and `macExtraButtons`.
   - Updated `setButtonDown()`, `setButtonUp()`, and `sendButtonEvent()` to route `buttonID` `3..31` to `kCGEventOtherMouseDown/Up` with `buttonNumber`.

3. **`Source/Module/modules/controller/mouse/commands/MouseModuleCommands.cpp`**
   - Added `Button 4`..`Button 32` options (values `3..31`) to `buttonID` under `#if JUCE_MAC`.
