/*
  ==============================================================================

    SpeechBridge.h
    macOS Apple Speech.framework + AVAudioEngine + CoreAudio C++ bridge

  ==============================================================================
*/

#pragma once

#include <functional>
#include <string>
#include <vector>

struct SpeechAudioDevice
{
    std::string name;
    std::string uid;
};

struct SpeechCallbacks
{
    std::function<void(const std::string& text, bool isFinal)> onTranscription;
    std::function<void(const std::string& status, bool isRunning)> onStatus;
    std::function<void(const std::vector<SpeechAudioDevice>& devices)> onDevicesChanged;
};

namespace SpeechBridge
{
    using Handle = void*;

    Handle create(const SpeechCallbacks& callbacks);
    void destroy(Handle handle);

    void start(Handle handle, const std::string& locale, const std::string& deviceUID, double silenceDurationSec, bool continuous);
    void stop(Handle handle);
    void setLocale(Handle handle, const std::string& locale);
    void setAudioDevice(Handle handle, const std::string& deviceUID);
    void setSilenceDuration(Handle handle, double silenceDurationSec);
    void setContinuous(Handle handle, bool continuous);
    void reset(Handle handle);

    std::vector<SpeechAudioDevice> getInputDevices();
}
