# UVC Camera Module (macOS)

## Summary

Added a built-in `UVC Camera` module (`Source/Module/modules/controller/uvc/`) to monitor and control USB Video Class (UVC) cameras—including Pan, Tilt, Zoom (PTZ), Focus, Exposure, White Balance, and Image Processing controls—on macOS, following the `StreamDeck` module architecture and using `UVCControllerCore` (IOKit/USB).

## Changes

1. **`Source/Module/modules/controller/uvc/lib/UVCControllerCore/`**
   - Vendored `UVCController`, `UVCType`, and `UVCValue` (compiled with `-fno-objc-arc`).

2. **`Source/Module/modules/controller/uvc/UVCBridge.h` & `UVCBridge_mac.mm`**
   - Pure C++ interface (`UVCDeviceInfo`, `UVCControlInfo`, `UVCTelemetry`) backed on macOS by `UVCControllerCore` (`#if defined(__APPLE__)`) and stubbed on other platforms.

3. **`Source/Module/modules/controller/uvc/UVCDevice.h` & `UVCDevice.cpp`**
   - Per-camera device wrapper with a background polling thread (`1..120 Hz`, default `30 Hz`), thread-safe hardware lock, optional `[0..1]` normalization, and async message-thread telemetry dispatch.

4. **`Source/Module/modules/controller/uvc/UVCManager.h` & `UVCManager.cpp`**
   - Singleton manager (`juce_DeclareSingleton(UVCManager, true)`) with 1 Hz USB hot-plug detection keyed by `locationId`.

5. **`Source/Module/modules/controller/uvc/UVCModule.h` & `UVCModule.cpp` & `commands/UVCCommand.h/.cpp`**
   - Registered as `Hardware -> UVC Camera` in `ModuleFactory.cpp`.
   - Exposes `PTZ` (`Pan`, `Tilt`, `Zoom`) and dynamically discovered `Controls` parameters with feedback-loop guards, plus `Set Pan / Tilt`, `Set Pan`, `Set Tilt`, `Set Zoom`, `Set Control`, and `Reset Control` commands.

6. **Project & Engine Integration**
   - Updated `Source/Module/ModuleIncludes.h`, `Source/Module/ModuleIncludes.cpp`, `Source/Module/ModuleFactory.cpp`, `Source/ChataigneEngine.cpp`, `Chataigne.jucer`, and `Builds/MacOSX/Chataigne.xcodeproj/project.pbxproj`.
