/*
  ==============================================================================

    VisionBridge.h
    macOS AVFoundation + Vision.framework C++ bridge for Pose, Face, and Hands

  ==============================================================================
*/

#pragma once

#include <functional>
#include <string>
#include <vector>

enum class VisionTaskType
{
    Pose2D,
    Pose3D,
    Face,
    Hands
};

struct VisionCameraDevice
{
    std::string name;
    std::string uniqueId;
};

struct VisionConfig
{
    VisionTaskType task = VisionTaskType::Pose2D;
    std::string deviceId = "Default";
    float minConfidence = 0.1f;
    int maxDimension = 384;
    int targetFps = 30;
    float roiX = 0.0f;
    float roiY = 0.0f;
    float roiWidth = 1.0f;
    float roiHeight = 1.0f;
    bool includeLandmarks = true; // Face
    int maxHandCount = 2;         // Hands
};

struct VisionCallbacks
{
    std::function<void(const std::string& jsonPayload, int detectedCount)> onResult;
    std::function<void(const std::string& status, bool isRunning)> onStatus;
    std::function<void(const std::vector<VisionCameraDevice>& devices)> onDevicesChanged;
};

namespace VisionBridge
{
    using Handle = void*;

    Handle create(const VisionCallbacks& callbacks);
    void destroy(Handle handle);

    void start(Handle handle, const VisionConfig& config);
    void stop(Handle handle);
    void updateConfig(Handle handle, const VisionConfig& config);

    std::vector<VisionCameraDevice> getCameraDevices();
}
