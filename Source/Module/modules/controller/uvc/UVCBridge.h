/*
  ==============================================================================

    UVCBridge.h
    macOS IOKit/UVCControllerCore C++ bridge

  ==============================================================================
*/

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct UVCDeviceInfo
{
    std::string name;
    uint32_t locationId = 0;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t uvcVersion = 0;
};

struct UVCControlInfo
{
    std::string name;
    bool supportsGet = false;
    bool supportsSet = false;
    bool hasRange = false;
    bool hasStepSize = false;
    bool hasDefaultValue = false;
    bool isPanTilt = false;
    bool isBool = false;

    double currentValue = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
    double stepSize = 1.0;
    double defaultValue = 0.0;

    // Secondary fields for pan-tilt-absolute
    double panValue = 0.0;
    double tiltValue = 0.0;
    double panMin = 0.0;
    double panMax = 0.0;
    double tiltMin = 0.0;
    double tiltMax = 0.0;
    double panDefault = 0.0;
    double tiltDefault = 0.0;
};

struct UVCTelemetry
{
    bool valid = false;
    bool hasPan = false;
    bool hasTilt = false;
    bool hasZoom = false;
    double pan = 0.0;
    double tilt = 0.0;
    double zoom = 0.0;
    std::map<std::string, double> scalarValues;
};

namespace UVCBridge
{
    using Handle = void*;

    std::vector<UVCDeviceInfo> enumerateDevices();
    Handle openDevice(uint32_t locationId);
    void closeDevice(Handle handle);
    bool isDeviceOpen(Handle handle);

    std::vector<UVCControlInfo> getControls(Handle handle);
    UVCTelemetry pollTelemetry(Handle handle, bool normalize);

    bool setControlValueString(Handle handle, const std::string& controlName, const std::string& valueStr);
    bool setPanTilt(Handle handle, double pan, double tilt, bool normalize);
    bool setScalarControl(Handle handle, const std::string& controlName, double value, bool normalize);
    bool resetControlToDefault(Handle handle, const std::string& controlName);
}
