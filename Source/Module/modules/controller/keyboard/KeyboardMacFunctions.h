/*
  ==============================================================================

    KeyboardMacFunctions.h
    Created: 13 Mar 2020 12:09:11pm
    Author:  Ben Kuper

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#if JUCE_MAC
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

namespace keyboardmac {

    inline void sendMacKeyEvent(int keyID, bool isDown)
    {
#if JUCE_MAC
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
        CGEventRef keyEvent = CGEventCreateKeyboardEvent(source, (CGKeyCode)keyID, isDown);
        if (keyEvent != nullptr)
        {
            CGEventPost(kCGAnnotatedSessionEventTap, keyEvent);
            CFRelease(keyEvent);
        }
        if (source != nullptr)
            CFRelease(source);
#endif
    }

#if JUCE_MAC
    struct KeyEvent
    {
        int type = 0; // 0 = keyDown, 1 = keyUp, 2 = flagsChanged
        uint16_t cgKeyCode = 0;
        uint64_t flags = 0;
        UniChar unicodeChar = 0;
        int unicodeLen = 0;
    };

    inline int cgKeyCodeToAscii(uint16_t keyCode)
    {
        switch (keyCode)
        {
        case 0x00: return 'A'; case 0x0B: return 'B'; case 0x08: return 'C'; case 0x02: return 'D';
        case 0x0E: return 'E'; case 0x03: return 'F'; case 0x05: return 'G'; case 0x04: return 'H';
        case 0x22: return 'I'; case 0x26: return 'J'; case 0x28: return 'K'; case 0x25: return 'L';
        case 0x2E: return 'M'; case 0x2D: return 'N'; case 0x1F: return 'O'; case 0x23: return 'P';
        case 0x0C: return 'Q'; case 0x0F: return 'R'; case 0x01: return 'S'; case 0x11: return 'T';
        case 0x20: return 'U'; case 0x09: return 'V'; case 0x0D: return 'W'; case 0x07: return 'X';
        case 0x10: return 'Y'; case 0x06: return 'Z';
        case 0x1D: return '0'; case 0x12: return '1'; case 0x13: return '2'; case 0x14: return '3';
        case 0x15: return '4'; case 0x17: return '5'; case 0x16: return '6'; case 0x1A: return '7';
        case 0x1C: return '8'; case 0x19: return '9';
        case 0x31: return ' ';
        case 0x1B: return '-'; case 0x18: return '='; case 0x21: return '['; case 0x1E: return ']';
        case 0x2A: return '\\'; case 0x29: return ';'; case 0x27: return '\''; case 0x2B: return ',';
        case 0x2F: return '.'; case 0x2C: return '/'; case 0x32: return '`';
        default: return 0;
        }
    }

    inline String cgKeyCodeToName(uint16_t keyCode)
    {
        switch (keyCode)
        {
        case 0x24: return "return";
        case 0x30: return "tab";
        case 0x31: return "space";
        case 0x33: return "backspace";
        case 0x35: return "escape";
        case 0x75: return "delete";
        case 0x7B: return "cursor left";
        case 0x7C: return "cursor right";
        case 0x7D: return "cursor down";
        case 0x7E: return "cursor up";
        case 0x73: return "home";
        case 0x77: return "end";
        case 0x74: return "page up";
        case 0x79: return "page down";
        case 0x7A: return "F1";  case 0x78: return "F2";  case 0x63: return "F3";  case 0x76: return "F4";
        case 0x60: return "F5";  case 0x61: return "F6";  case 0x62: return "F7";  case 0x64: return "F8";
        case 0x65: return "F9";  case 0x6D: return "F10"; case 0x67: return "F11"; case 0x6F: return "F12";
        default:
        {
            int ascii = cgKeyCodeToAscii(keyCode);
            if (ascii > 0) return String::charToString((juce_wchar)ascii);
            return "key " + String((int)keyCode);
        }
        }
    }

    class MacKeyboardTap
    {
    public:
        using KeyCallback = std::function<void(const KeyEvent&)>;

        MacKeyboardTap() = default;
        ~MacKeyboardTap() { stop(); }

        bool start(KeyCallback cb)
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
                    CGEventMaskBit(kCGEventKeyDown) |
                    CGEventMaskBit(kCGEventKeyUp) |
                    CGEventMaskBit(kCGEventFlagsChanged);

                CFMachPortRef tap = CGEventTapCreate(
                    kCGHIDEventTap,
                    kCGHeadInsertEventTap,
                    kCGEventTapOptionListenOnly,
                    mask,
                    &MacKeyboardTap::eventTapCallback,
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
            auto* self = static_cast<MacKeyboardTap*>(userInfo);
            if (self == nullptr) return event;

            if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput)
            {
                std::lock_guard<std::mutex> lock(self->stateMutex);
                if (self->eventTap != nullptr)
                    CGEventTapEnable(self->eventTap, true);
                return event;
            }

            KeyEvent ev;
            if (type == kCGEventKeyDown) ev.type = 0;
            else if (type == kCGEventKeyUp) ev.type = 1;
            else if (type == kCGEventFlagsChanged) ev.type = 2;
            else return event;

            ev.cgKeyCode = (uint16_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            ev.flags = (uint64_t)CGEventGetFlags(event);

            if (ev.type != 2)
            {
                UniChar chars[4] = {};
                UniCharCount actualLen = 0;
                CGEventKeyboardGetUnicodeString(event, 4, &actualLen, chars);
                if (actualLen > 0)
                {
                    ev.unicodeChar = chars[0];
                    ev.unicodeLen = (int)actualLen;
                }
            }

            if (self->callback)
                self->callback(ev);

            return event;
        }

        std::thread tapThread;
        std::mutex stateMutex;
        std::atomic<bool> stopRequested{ false };
        CFRunLoopRef runLoop = nullptr;
        CFMachPortRef eventTap = nullptr;
        KeyCallback callback;
    };
#endif
}
