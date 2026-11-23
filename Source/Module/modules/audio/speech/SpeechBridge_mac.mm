/*
  ==============================================================================

    SpeechBridge_mac.mm
    macOS Objective-C++ implementation of SpeechBridge using Speech.framework

  ==============================================================================
*/

#include "SpeechBridge.h"

#if defined(__APPLE__)

#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#import <Speech/Speech.h>

#include <cmath>
#include <memory>
#include <mutex>

namespace
{
    std::vector<SpeechAudioDevice> enumerateInputDevicesInternal()
    {
        std::vector<SpeechAudioDevice> list;
        list.push_back({ "Default System Microphone", "Default" });

        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            0
        };

        UInt32 size = 0;
        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr || size == 0)
            return list;

        int count = (int)(size / sizeof(AudioDeviceID));
        std::vector<AudioDeviceID> devices((size_t)count);
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, devices.data()) != noErr)
            return list;

        for (AudioDeviceID device : devices)
        {
            AudioObjectPropertyAddress streamAddress = {
                kAudioDevicePropertyStreams,
                kAudioObjectPropertyScopeInput,
                0
            };

            UInt32 streamSize = 0;
            if (AudioObjectGetPropertyDataSize(device, &streamAddress, 0, nullptr, &streamSize) != noErr || streamSize == 0)
                continue;

            AudioObjectPropertyAddress nameAddress = {
                kAudioDevicePropertyDeviceNameCFString,
                kAudioObjectPropertyScopeGlobal,
                0
            };
            CFStringRef nameString = nullptr;
            UInt32 nameSize = sizeof(CFStringRef);
            std::string name = "Unknown Input";
            if (AudioObjectGetPropertyData(device, &nameAddress, 0, nullptr, &nameSize, &nameString) == noErr && nameString != nullptr)
            {
                const char* utf8 = [(__bridge NSString*)nameString UTF8String];
                if (utf8) name = utf8;
                CFRelease(nameString);
            }

            AudioObjectPropertyAddress uidAddress = {
                kAudioDevicePropertyDeviceUID,
                kAudioObjectPropertyScopeGlobal,
                0
            };
            CFStringRef uidString = nullptr;
            UInt32 uidSize = sizeof(CFStringRef);
            std::string uid = "Unknown UID";
            if (AudioObjectGetPropertyData(device, &uidAddress, 0, nullptr, &uidSize, &uidString) == noErr && uidString != nullptr)
            {
                const char* utf8 = [(__bridge NSString*)uidString UTF8String];
                if (utf8) uid = utf8;
                CFRelease(uidString);
            }

            list.push_back({ name, uid });
        }

        return list;
    }

    AudioDeviceID findAudioDeviceID(const std::string& uid)
    {
        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            0
        };

        UInt32 size = 0;
        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr || size == 0)
            return 0;

        int count = (int)(size / sizeof(AudioDeviceID));
        std::vector<AudioDeviceID> devices((size_t)count);
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, devices.data()) != noErr)
            return 0;

        for (AudioDeviceID device : devices)
        {
            AudioObjectPropertyAddress uidAddress = {
                kAudioDevicePropertyDeviceUID,
                kAudioObjectPropertyScopeGlobal,
                0
            };
            CFStringRef uidString = nullptr;
            UInt32 uidSize = sizeof(CFStringRef);
            if (AudioObjectGetPropertyData(device, &uidAddress, 0, nullptr, &uidSize, &uidString) == noErr && uidString != nullptr)
            {
                const char* utf8 = [(__bridge NSString*)uidString UTF8String];
                std::string uidStr = utf8 ? utf8 : "";
                CFRelease(uidString);
                if (uidStr == uid)
                    return device;
            }
        }
        return 0;
    }
}

@interface ChataigneSpeechEngine : NSObject
{
    SpeechCallbacks _callbacks;
    std::mutex _cbMutex;
    NSString* _currentLocale;
    NSString* _currentDeviceUID;
    double _silenceDuration;
    NSDate* _lastActivityTime;
    BOOL _isSilentState;
    NSString* _lastText;
    BOOL _isRecording;
    BOOL _continuous;
    BOOL _destroyed;
}

@property (nonatomic, strong) SFSpeechRecognizer* speechRecognizer;
@property (nonatomic, strong) SFSpeechAudioBufferRecognitionRequest* recognitionRequest;
@property (nonatomic, strong) SFSpeechRecognitionTask* recognitionTask;
@property (nonatomic, strong) AVAudioEngine* audioEngine;

- (instancetype)initWithCallbacks:(const SpeechCallbacks&)callbacks;
- (void)shutdown;
- (void)startWithLocale:(NSString*)locale deviceUID:(NSString*)deviceUID silenceDuration:(double)silenceDuration continuous:(BOOL)continuous;
- (void)stop;
- (void)setLocale:(NSString*)locale;
- (void)setAudioDevice:(NSString*)deviceUID;
- (void)setSilenceDuration:(double)seconds;
- (void)setContinuous:(BOOL)continuous;
- (void)resetTranscription;
- (void)publishDevices;

@end

static OSStatus ChataigneAudioDeviceChangedCallback(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData)
{
    ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)inClientData;
    [engine publishDevices];
    return noErr;
}

@implementation ChataigneSpeechEngine

- (instancetype)initWithCallbacks:(const SpeechCallbacks&)callbacks
{
    self = [super init];
    if (self)
    {
        _callbacks = callbacks;
        _audioEngine = [[AVAudioEngine alloc] init];
        _currentLocale = @"en-US";
        _currentDeviceUID = @"Default";
        _silenceDuration = 1.5;
        _isRecording = NO;
        _continuous = YES;
        _destroyed = NO;

        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            0
        };
        AudioObjectAddPropertyListener(kAudioObjectSystemObject, &address, ChataigneAudioDeviceChangedCallback, (__bridge void*)self);
    }
    return self;
}

- (void)shutdown
{
    {
        std::lock_guard<std::mutex> lock(_cbMutex);
        _destroyed = YES;
        _callbacks = {};
    }

    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        0
    };
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &address, ChataigneAudioDeviceChangedCallback, (__bridge void*)self);
    [self stop];
}

- (void)emitStatus:(NSString*)status isRunning:(BOOL)running
{
    std::lock_guard<std::mutex> lock(_cbMutex);
    if (_destroyed || !_callbacks.onStatus) return;
    const char* utf8 = [status UTF8String];
    _callbacks.onStatus(utf8 ? utf8 : "", running == YES);
}

- (void)emitTranscription:(NSString*)text isFinal:(BOOL)isFinal
{
    std::lock_guard<std::mutex> lock(_cbMutex);
    if (_destroyed || !_callbacks.onTranscription) return;
    const char* utf8 = [text UTF8String];
    _callbacks.onTranscription(utf8 ? utf8 : "", isFinal == YES);
}

- (void)publishDevices
{
    auto devs = enumerateInputDevicesInternal();
    std::lock_guard<std::mutex> lock(_cbMutex);
    if (_destroyed || !_callbacks.onDevicesChanged) return;
    _callbacks.onDevicesChanged(devs);
}

- (void)startWithLocale:(NSString*)locale deviceUID:(NSString*)deviceUID silenceDuration:(double)silenceDuration continuous:(BOOL)continuous
{
    @synchronized (self)
    {
        _currentLocale = locale ?: @"en-US";
        _currentDeviceUID = deviceUID ?: @"Default";
        _silenceDuration = silenceDuration > 0.1 ? silenceDuration : 1.5;
        _continuous = continuous;
        _isRecording = YES;

        NSString* speechDesc = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSSpeechRecognitionUsageDescription"];
        if (speechDesc == nil)
        {
            _isRecording = NO;
            [self emitStatus:@"Missing NSSpeechRecognitionUsageDescription in Info.plist" isRunning:NO];
            return;
        }

        SFSpeechRecognizerAuthorizationStatus authStatus = [SFSpeechRecognizer authorizationStatus];
        if (authStatus == SFSpeechRecognizerAuthorizationStatusNotDetermined)
        {
            [self emitStatus:@"Requesting Speech Permission..." isRunning:NO];
            __weak ChataigneSpeechEngine* weakSelf = self;
            [SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus status) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    ChataigneSpeechEngine* strongSelf = weakSelf;
                    if (!strongSelf) return;
                    @synchronized (strongSelf)
                    {
                        if (!strongSelf->_isRecording || strongSelf->_destroyed) return;
                        if (status == SFSpeechRecognizerAuthorizationStatusAuthorized)
                            [strongSelf startRecordingInternal];
                        else
                        {
                            strongSelf->_isRecording = NO;
                            [strongSelf emitStatus:@"Speech Permission Denied" isRunning:NO];
                        }
                    }
                });
            }];
            return;
        }
        else if (authStatus != SFSpeechRecognizerAuthorizationStatusAuthorized)
        {
            _isRecording = NO;
            [self emitStatus:@"Speech Permission Denied" isRunning:NO];
            return;
        }

        [self startRecordingInternal];
    }
}

- (void)startRecordingInternal
{
    [self.audioEngine stop];
    [self.audioEngine.inputNode removeTapOnBus:0];

    if (self.recognitionTask)
    {
        [self.recognitionTask cancel];
        self.recognitionTask = nil;
    }

    self.speechRecognizer = [[SFSpeechRecognizer alloc] initWithLocale:[NSLocale localeWithLocaleIdentifier:_currentLocale]];
    if (!self.speechRecognizer || !self.speechRecognizer.available)
    {
        _isRecording = NO;
        [self emitStatus:@"Recognizer Not Available" isRunning:NO];
        return;
    }

    self.recognitionRequest = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
    self.recognitionRequest.shouldReportPartialResults = YES;
    if (@available(macOS 10.15, *))
    {
        if (self.speechRecognizer.supportsOnDeviceRecognition)
            self.recognitionRequest.requiresOnDeviceRecognition = YES;
    }

    AVAudioInputNode* inputNode = self.audioEngine.inputNode;

    if (_currentDeviceUID && ![_currentDeviceUID isEqualToString:@"Default"])
    {
        AudioDeviceID deviceID = findAudioDeviceID([_currentDeviceUID UTF8String]);
        if (deviceID != 0)
        {
            AudioUnit inputAudioUnit = inputNode.audioUnit;
            if (inputAudioUnit)
            {
                AudioUnitSetProperty(
                    inputAudioUnit,
                    kAudioOutputUnitProperty_CurrentDevice,
                    kAudioUnitScope_Global,
                    0,
                    &deviceID,
                    sizeof(AudioDeviceID)
                );
            }
        }
    }

    AVAudioFormat* recordingFormat = [inputNode outputFormatForBus:0];
    if (recordingFormat.sampleRate <= 0 || recordingFormat.channelCount <= 0)
    {
        _isRecording = NO;
        [self emitStatus:@"Invalid Audio Input Format" isRunning:NO];
        return;
    }

    _lastActivityTime = [NSDate date];
    _isSilentState = YES;
    _lastText = @"";

    __weak ChataigneSpeechEngine* weakSelf = self;
    [inputNode installTapOnBus:0 bufferSize:4096 format:recordingFormat block:^(AVAudioPCMBuffer* _Nonnull buffer, AVAudioTime* _Nonnull) {
        ChataigneSpeechEngine* strongSelf = weakSelf;
        if (!strongSelf) return;
        @synchronized (strongSelf)
        {
            if (strongSelf.recognitionRequest)
            {
                float* channelData = buffer.floatChannelData ? buffer.floatChannelData[0] : nullptr;
                int frameLength = (int)buffer.frameLength;
                if (channelData && frameLength > 0)
                {
                    float sum = 0.0f;
                    for (int i = 0; i < frameLength; ++i)
                        sum += channelData[i] * channelData[i];

                    float rms = std::sqrt(sum / (float)frameLength);
                    float db = rms > 0.0001f ? 20.0f * std::log10(rms) : -100.0f;

                    BOOL wasSilent = strongSelf->_isSilentState;
                    if (db > -45.0f)
                    {
                        strongSelf->_lastActivityTime = [NSDate date];
                        strongSelf->_isSilentState = NO;
                    }

                    BOOL shouldReset = !wasSilent && ([[NSDate date] timeIntervalSinceDate:strongSelf->_lastActivityTime] > strongSelf->_silenceDuration);
                    if (shouldReset)
                    {
                        strongSelf->_isSilentState = YES;
                        BOOL cont = strongSelf->_continuous;
                        dispatch_async(dispatch_get_main_queue(), ^{
                            if (cont)
                                [strongSelf resetTranscription];
                            else
                                [strongSelf stop];
                        });
                    }
                }

                [strongSelf.recognitionRequest appendAudioPCMBuffer:buffer];
            }
        }
    }];

    [self.audioEngine prepare];
    NSError* startError = nil;
    [self.audioEngine startAndReturnError:&startError];
    if (startError)
    {
        _isRecording = NO;
        [self emitStatus:@"Audio Engine Start Failed" isRunning:NO];
        return;
    }

    [self attachRecognitionTask];
    [self emitStatus:@"Running" isRunning:YES];
}

- (void)attachRecognitionTask
{
    __weak ChataigneSpeechEngine* weakSelf = self;
    SFSpeechAudioBufferRecognitionRequest* reqForTask = self.recognitionRequest;
    self.recognitionTask = [self.speechRecognizer recognitionTaskWithRequest:reqForTask resultHandler:^(SFSpeechRecognitionResult* _Nullable result, NSError* _Nullable error) {
        ChataigneSpeechEngine* strongSelf = weakSelf;
        if (!strongSelf) return;

        @synchronized (strongSelf)
        {
            // Ignore late callbacks/errors from a superseded or cancelled recognition request
            if (strongSelf.recognitionRequest != reqForTask)
                return;
        }

        if (result)
        {
            NSString* text = result.bestTranscription.formattedString ?: @"";
            @synchronized (strongSelf)
            {
                if (strongSelf.recognitionRequest != reqForTask)
                    return;
                strongSelf->_lastText = text;
            }
            [strongSelf emitTranscription:text isFinal:result.isFinal];
        }

        if (error || (result && result.isFinal))
        {
            @synchronized (strongSelf)
            {
                if (strongSelf.recognitionRequest != reqForTask)
                    return;
                if (strongSelf->_isRecording && !strongSelf->_destroyed)
                {
                    BOOL cont = strongSelf->_continuous;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if (cont)
                            [strongSelf resetTranscription];
                        else
                            [strongSelf stop];
                    });
                }
            }
        }
    }];
}

- (void)stop
{
    @synchronized (self)
    {
        _isRecording = NO;

        if (self.audioEngine.isRunning)
        {
            [self.audioEngine stop];
            [self.audioEngine.inputNode removeTapOnBus:0];
        }

        if (_lastText && ![_lastText isEqualToString:@""])
        {
            [self emitTranscription:_lastText isFinal:YES];
            _lastText = @"";
        }

        [self.recognitionRequest endAudio];
        [self.recognitionTask cancel];
        self.recognitionRequest = nil;
        self.recognitionTask = nil;
        _isSilentState = YES;

        [self emitStatus:@"Stopped" isRunning:NO];
    }
}

- (void)setLocale:(NSString*)locale
{
    @synchronized (self)
    {
        if (!locale || [_currentLocale isEqualToString:locale]) return;
        _currentLocale = locale;
        if (_isRecording)
            [self startRecordingInternal];
    }
}

- (void)setAudioDevice:(NSString*)deviceUID
{
    @synchronized (self)
    {
        if (!deviceUID || [_currentDeviceUID isEqualToString:deviceUID]) return;
        _currentDeviceUID = deviceUID;
        if (_isRecording)
            [self startRecordingInternal];
    }
}

- (void)setSilenceDuration:(double)seconds
{
    @synchronized (self)
    {
        _silenceDuration = seconds > 0.1 ? seconds : 1.5;
    }
}

- (void)setContinuous:(BOOL)continuous
{
    @synchronized (self)
    {
        _continuous = continuous;
    }
}

- (void)resetTranscription
{
    @synchronized (self)
    {
        if (!_isRecording || _destroyed) return;

        if (_lastText && ![_lastText isEqualToString:@""])
        {
            [self emitTranscription:_lastText isFinal:YES];
            _lastText = @"";
        }

        _lastActivityTime = [NSDate date];
        _isSilentState = YES;

        [self.recognitionRequest endAudio];
        [self.recognitionTask cancel];
        self.recognitionTask = nil;

        self.speechRecognizer = [[SFSpeechRecognizer alloc] initWithLocale:[NSLocale localeWithLocaleIdentifier:_currentLocale]];
        if (!self.speechRecognizer || !self.speechRecognizer.available)
        {
            [self stop];
            [self emitStatus:@"Recognizer Not Available" isRunning:NO];
            return;
        }

        self.recognitionRequest = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
        self.recognitionRequest.shouldReportPartialResults = YES;
        if (@available(macOS 10.15, *))
        {
            if (self.speechRecognizer.supportsOnDeviceRecognition)
                self.recognitionRequest.requiresOnDeviceRecognition = YES;
        }

        [self attachRecognitionTask];
    }
}

@end

namespace SpeechBridge
{
    Handle create(const SpeechCallbacks& callbacks)
    {
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = [[ChataigneSpeechEngine alloc] initWithCallbacks:callbacks];
            return (__bridge_retained void*)engine;
        }
    }

    void destroy(Handle handle)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge_transfer ChataigneSpeechEngine*)handle;
            [engine shutdown];
            engine = nil;
        }
    }

    void start(Handle handle, const std::string& locale, const std::string& deviceUID, double silenceDurationSec, bool continuous)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine startWithLocale:[NSString stringWithUTF8String:locale.c_str()]
                          deviceUID:[NSString stringWithUTF8String:deviceUID.c_str()]
                    silenceDuration:silenceDurationSec
                         continuous:continuous ? YES : NO];
        }
    }

    void stop(Handle handle)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine stop];
        }
    }

    void setLocale(Handle handle, const std::string& locale)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine setLocale:[NSString stringWithUTF8String:locale.c_str()]];
        }
    }

    void setAudioDevice(Handle handle, const std::string& deviceUID)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine setAudioDevice:[NSString stringWithUTF8String:deviceUID.c_str()]];
        }
    }

    void setSilenceDuration(Handle handle, double silenceDurationSec)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine setSilenceDuration:silenceDurationSec];
        }
    }

    void setContinuous(Handle handle, bool continuous)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine setContinuous:continuous ? YES : NO];
        }
    }

    void reset(Handle handle)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneSpeechEngine* engine = (__bridge ChataigneSpeechEngine*)handle;
            [engine resetTranscription];
        }
    }

    std::vector<SpeechAudioDevice> getInputDevices()
    {
        @autoreleasepool
        {
            return enumerateInputDevicesInternal();
        }
    }
}

#else

namespace SpeechBridge
{
    Handle create(const SpeechCallbacks&) { return nullptr; }
    void destroy(Handle) {}
    void start(Handle, const std::string&, const std::string&, double, bool) {}
    void stop(Handle) {}
    void setLocale(Handle, const std::string&) {}
    void setAudioDevice(Handle, const std::string&) {}
    void setSilenceDuration(Handle, double) {}
    void setContinuous(Handle, bool) {}
    void reset(Handle) {}
    std::vector<SpeechAudioDevice> getInputDevices() { return { { "Default System Microphone", "Default" } }; }
}

#endif
