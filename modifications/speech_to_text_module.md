# Speech to Text Module (macOS Apple Speech Framework)

## Summary

Added a built-in `Speech to Text` module (`Source/Module/modules/audio/speech/`) for local, on-device speech recognition on macOS using Apple's `Speech.framework` (`SFSpeechRecognizer`), `AVFoundation` (`AVAudioEngine`), and `CoreAudio`.

## Changes

1. **`Source/Module/modules/audio/speech/SpeechBridge.h` & `SpeechBridge_mac.mm`**
   - Pure C++ interface (`SpeechAudioDevice`, `SpeechCallbacks`) backed on macOS by `ChataigneSpeechEngine` (`#if defined(__APPLE__)`, compiled with `-fobjc-arc`).
   - Enumerates CoreAudio input devices with a hot-plug property listener.
   - Enforces on-device recognition (`requiresOnDeviceRecognition = YES` when supported) and performs RMS silence detection (`-45 dB` threshold) to finalize and reset segments cleanly.

2. **`Source/Module/modules/audio/speech/SpeechToTextModule.h` & `.cpp` & `commands/SpeechToTextCommand.h/.cpp`**
   - Registered as `Hardware -> Speech to Text` in `ModuleFactory.cpp`.
   - Parameters: `Active`, `Continuous Capture`, `Is Running`, `Status`, `Locale`, `Audio Input Device`, `Output Mode` (`Full Transcript`, `New Words Only`, `Chunk`), `Silence Duration`, and `Reset Text`.
   - Values: `Transcribed Text`, `Is Final`, and `Word Received` trigger.
   - Commands: `Start`, `Stop`, `Reset Text`, and `Set Locale`.

3. **Permissions, Frameworks & Project Integration**
   - Added `NSSpeechRecognitionUsageDescription` to `Builds/MacOSX/Info-App.plist`.
   - Linked `Speech.framework` and added the new files in `Chataigne.jucer`, `Builds/MacOSX/Chataigne.xcodeproj/project.pbxproj`, `Source/Module/ModuleIncludes.h`, and `Source/Module/ModuleIncludes.cpp`.

## Privacy & Local Processing Note

- **macOS Permission Prompt**: The first time `SFSpeechRecognizer` requests authorization, macOS displays a hardcoded system dialog (`"Speech data from this app will be sent to Apple to process your requests..."`). Apple does not provide a separate permission prompt for on-device-only recognition.
- **On-Device Enforcement**: `SpeechBridge_mac.mm` sets `recognitionRequest.requiresOnDeviceRecognition = YES` whenever `speechRecognizer.supportsOnDeviceRecognition` is `YES`, which keeps audio processing 100% local on-device without sending audio to Apple servers.
- **Offline Model Requirement**: If the offline dictation model for the selected locale is not downloaded (in *System Settings → Keyboard → Dictation*), `supportsOnDeviceRecognition` may be `NO`, in which case `requiresOnDeviceRecognition` is not forced.
