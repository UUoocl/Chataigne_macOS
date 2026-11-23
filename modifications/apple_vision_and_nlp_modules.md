# Apple Vision (Pose, Face, Hands) & NaturalLanguage (Sentiment) Modules

## Summary

Added four local, on-device Apple framework modules for macOS, each configurable in the Module Inspector and outputting structured JSON payloads:
- `Vision Pose` (`Hardware -> Vision Pose`)
- `Vision Face` (`Hardware -> Vision Face`)
- `Vision Hands` (`Hardware -> Vision Hands`)
- `Sentiment Analysis` (`System -> Sentiment Analysis`)

## Changes

1. **Apple Vision Modules (`Source/Module/modules/vision/`)**
   - `VisionBridge.h` & `VisionBridge_mac.mm` (`#if defined(__APPLE__)`, compiled with `-fobjc-arc`):
     - Shared `AVCaptureSession` + `AVCaptureVideoDataOutput` camera capture with device enumeration, hot-plug notifications, FPS throttling, normalized `Region of Interest` (ROI), and frame backpressure.
     - `VisionPoseModule`: `VNDetectHumanBodyPoseRequest` (2D, macOS 11+) and `VNDetectHumanBodyPose3DRequest` (3D, macOS 14+).
     - `VisionFaceModule`: `VNDetectFaceLandmarksRequest` / `VNDetectFaceRectanglesRequest` with bounding boxes, `roll`/`yaw`/`pitch`, and facial landmarks.
     - `VisionHandsModule`: `VNDetectHumanHandPoseRequest` with `chirality` and joint coordinates.
   - `VisionBaseModule.h/.cpp` shared base class for `Active`, `Is Running`, `Status`, `Camera Device`, `Min Confidence`, `Target FPS`, `Region of Interest`, `Detected ...`, `JSON`, and `On Detected`.

2. **Apple NaturalLanguage Sentiment Module (`Source/Module/modules/nlp/`)**
   - `SentimentBridge.h` & `SentimentBridge_mac.mm` (`#if defined(__APPLE__)`, compiled with `-fobjc-arc`):
     - Uses `NLLanguageRecognizer` (for `Auto` language detection) and `NLTagger` (`NLTagSchemeSentimentScore`).
   - `SentimentModule.h/.cpp` & `commands/SentimentCommand.h/.cpp`:
     - Parameters: `Language`, `Positive Threshold`, `Negative Threshold`, `Input Text`, `Analyze`.
     - Values: `Sentiment Score` (`-1.0..1.0`), `Sentiment Label`, `Detected Language`, `JSON`, `On Analyzed`.
     - Command: `Analyze Text`.

3. **Permissions, Frameworks & Project Integration**
   - Added `NSCameraUsageDescription` to `Builds/MacOSX/Info-App.plist`.
   - Weak-linked `Vision.framework` and `NaturalLanguage.framework` and updated `Chataigne.jucer`, `Builds/MacOSX/Chataigne.xcodeproj/project.pbxproj`, `Source/Module/ModuleFactory.cpp`, `Source/Module/ModuleIncludes.h`, and `Source/Module/ModuleIncludes.cpp`.
