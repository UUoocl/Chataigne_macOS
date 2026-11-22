/*
  ==============================================================================

    MouseMacFunctions.h
    Created: 13 Mar 2020 12:09:00pm
    Author:  Ben Kuper

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#if JUCE_MAC
    // Apple's legacy MacTypes.h declares a global Point. This file is included
    // by ModuleIncludes.cpp after JUCE has imported juce::Point, so rename the
    // legacy declaration while parsing the native headers.
    #define Point MacQuickDrawPoint
    #include <CoreGraphics/CGEventSource.h>
    #include <CoreGraphics/CoreGraphics.h>
    #undef Point
    #include <atomic>
    #include <condition_variable>
    #include <functional>
    #include <mutex>
    #include <thread>
#endif

namespace mousemac {
    
    inline void sendMouseEvent(int buttonEvent, float posX, float posY, int buttonNumber = -1)
    {
#if JUCE_MAC
        CGPoint pt;
        pt.x = posX;
        pt.y = posY;
        
        CGMouseButton button = kCGMouseButtonLeft;
        if (buttonNumber >= 3)
        {
            button = (CGMouseButton)buttonNumber;
        }
        else
        {
            button = (buttonEvent == kCGEventRightMouseDown || buttonEvent == kCGEventRightMouseUp)
                     ? kCGMouseButtonRight
                     : (buttonEvent == kCGEventOtherMouseDown || buttonEvent == kCGEventOtherMouseUp)
                       ? kCGMouseButtonCenter : kCGMouseButtonLeft;
        }

        CGEventRef mouseEv = CGEventCreateMouseEvent(NULL, (CGEventType)buttonEvent, pt, button);
        if (mouseEv != nullptr)
        {
            if (buttonNumber >= 3)
                CGEventSetIntegerValueField(mouseEv, kCGMouseEventButtonNumber, buttonNumber);
            CGEventPost(kCGHIDEventTap, mouseEv);
            CFRelease(mouseEv);
        }
#endif
    }

#if JUCE_MAC
    class MacMouseButtonTap
    {
    public:
        using ButtonCallback = std::function<void(int buttonNumber, bool pressed)>;

        MacMouseButtonTap() = default;
        ~MacMouseButtonTap() { stop(); }

        bool start(ButtonCallback cb)
        {
            stop();
            callback = std::move(cb);
            stopRequested = false;

            bool tapCreated = false;
            bool ready = false;
            std::mutex initMutex;
            std::condition_variable initCv;

            tapThread = std::thread([this, &tapCreated, &ready, &initMutex, &initCv]()
            {
                CGEventMask mask =
                    CGEventMaskBit(kCGEventLeftMouseDown) |
                    CGEventMaskBit(kCGEventLeftMouseUp) |
                    CGEventMaskBit(kCGEventRightMouseDown) |
                    CGEventMaskBit(kCGEventRightMouseUp) |
                    CGEventMaskBit(kCGEventOtherMouseDown) |
                    CGEventMaskBit(kCGEventOtherMouseUp);

                CFMachPortRef tap = CGEventTapCreate(
                    kCGHIDEventTap,
                    kCGHeadInsertEventTap,
                    kCGEventTapOptionListenOnly,
                    mask,
                    &MacMouseButtonTap::eventTapCallback,
                    this);

                CFRunLoopSourceRef source = nullptr;
                CFRunLoopRef rl = CFRunLoopGetCurrent();

                if (tap != nullptr)
                {
                    source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
                    if (source != nullptr)
                    {
                        CFRunLoopAddSource(rl, source, kCFRunLoopCommonModes);
                        CGEventTapEnable(tap, true);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    eventTap = tap;
                    runLoop = rl;
                }

                {
                    std::lock_guard<std::mutex> lock(initMutex);
                    tapCreated = (tap != nullptr && source != nullptr);
                    ready = true;
                }
                initCv.notify_one();

                if (tapCreated && !stopRequested.load())
                    CFRunLoopRun();

                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    runLoop = nullptr;
                    eventTap = nullptr;
                }

                if (source != nullptr)
                {
                    CFRunLoopRemoveSource(rl, source, kCFRunLoopCommonModes);
                    CFRelease(source);
                }
                if (tap != nullptr)
                {
                    CGEventTapEnable(tap, false);
                    CFRelease(tap);
                }
            });

            std::unique_lock<std::mutex> lock(initMutex);
            initCv.wait(lock, [&ready]() { return ready; });
            return tapCreated;
        }

        void stop()
        {
            stopRequested = true;
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                if (runLoop != nullptr)
                    CFRunLoopStop(runLoop);
            }
            if (tapThread.joinable())
                tapThread.join();
            callback = nullptr;
        }

    private:
        static CGEventRef eventTapCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* userInfo)
        {
            auto* self = static_cast<MacMouseButtonTap*>(userInfo);
            if (self == nullptr) return event;

            if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput)
            {
                std::lock_guard<std::mutex> lock(self->stateMutex);
                if (self->eventTap != nullptr)
                    CGEventTapEnable(self->eventTap, true);
                return event;
            }

            int buttonNumber = -1;
            bool pressed = false;

            switch (type)
            {
            case kCGEventLeftMouseDown:  buttonNumber = 0; pressed = true;  break;
            case kCGEventLeftMouseUp:    buttonNumber = 0; pressed = false; break;
            case kCGEventRightMouseDown: buttonNumber = 1; pressed = true;  break;
            case kCGEventRightMouseUp:   buttonNumber = 1; pressed = false; break;
            case kCGEventOtherMouseDown:
                buttonNumber = (int)CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
                pressed = true;
                break;
            case kCGEventOtherMouseUp:
                buttonNumber = (int)CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
                pressed = false;
                break;
            default:
                return event;
            }

            if (buttonNumber >= 0 && buttonNumber < 32 && self->callback)
                self->callback(buttonNumber, pressed);

            return event;
        }

        std::thread tapThread;
        std::mutex stateMutex;
        std::atomic<bool> stopRequested{ false };
        CFRunLoopRef runLoop = nullptr;
        CFMachPortRef eventTap = nullptr;
        ButtonCallback callback;
    };
#endif
    
    inline void setMousePos(float posX, float posY)
    {
#if JUCE_MAC
        CGPoint pt;
        pt.x = posX;
        pt.y = posY;
        
        CGEventRef moveEvent = CGEventCreateMouseEvent(
                                                       NULL,               // NULL to create a new event
                                                       kCGEventMouseMoved, // what type of event (move)
                                                       pt,                 // screen coordinate for the event
                                                       kCGMouseButtonLeft  // irrelevant for a move event
                                                       );
        
        // post the event and cleanup
        CGEventPost(kCGSessionEventTap, moveEvent);
        CFRelease(moveEvent);
#endif
    }

    inline void sendScrollWheelEvent(int32 scrollX, int32 scrollY)
    {
#if JUCE_MAC
        if (scrollX == 0 && scrollY == 0) return;
        CGEventRef scrollEvent = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 2, scrollY, scrollX);
        if (scrollEvent != nullptr)
        {
            CGEventPost(kCGHIDEventTap, scrollEvent);
            CFRelease(scrollEvent);
        }
#endif
    }
}
