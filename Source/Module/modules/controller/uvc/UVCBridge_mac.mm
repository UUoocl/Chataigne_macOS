/*
  ==============================================================================

    UVCBridge_mac.mm
    macOS Objective-C++ implementation of UVCBridge using UVCControllerCore

  ==============================================================================
*/

#include "UVCBridge.h"

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import "UVCController.h"
#import "UVCType.h"
#import "UVCValue.h"

#include <cmath>
#include <sstream>

namespace
{
    id parseUvcValue(UVCValue* val)
    {
        if (!val) return nil;
        NSString* str = [val stringValue];
        if (!str) return nil;

        NSString* trimmed = [str stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([trimmed hasPrefix:@"{"] && [trimmed hasSuffix:@"}"])
        {
            NSMutableDictionary* dict = [NSMutableDictionary dictionary];
            NSString* inner = [trimmed substringWithRange:NSMakeRange(1, [trimmed length] - 2)];
            NSArray* parts = [inner componentsSeparatedByString:@","];
            for (NSString* part in parts)
            {
                NSArray* kv = [part componentsSeparatedByString:@"="];
                if ([kv count] == 2)
                {
                    NSString* k = [[kv objectAtIndex:0] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                    NSString* vStr = [[kv objectAtIndex:1] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                    [dict setObject:@([vStr doubleValue]) forKey:k];
                }
                else if ([kv count] == 1)
                {
                    NSString* vStr = [[kv objectAtIndex:0] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                    [dict setObject:@([vStr doubleValue]) forKey:[NSString stringWithFormat:@"val%lu", (unsigned long)[dict count]]];
                }
            }
            return dict;
        }

        NSScanner* scanner = [NSScanner scannerWithString:trimmed];
        double doubleVal = 0.0;
        if ([scanner scanDouble:&doubleVal] && [scanner isAtEnd])
        {
            return @(doubleVal);
        }

        return trimmed;
    }

    double lerpNorm(double val, double minV, double maxV)
    {
        if (maxV == minV) return 0.0;
        double res = (val - minV) / (maxV - minV);
        if (res < 0.0) res = 0.0;
        if (res > 1.0) res = 1.0;
        return std::round(res * 10000.0) / 10000.0;
    }

    double denormLerp(double normVal, double minV, double maxV)
    {
        if (normVal < 0.0) normVal = 0.0;
        if (normVal > 1.0) normVal = 1.0;
        return minV + normVal * (maxV - minV);
    }

    bool isBooleanControlName(NSString* name)
    {
        return ([name hasPrefix:@"auto-"] && ![name isEqualToString:@"auto-exposure-mode"])
            || [name isEqualToString:@"privacy"]
            || [name isEqualToString:@"scanning-mode"];
    }
}

namespace UVCBridge
{
    std::vector<UVCDeviceInfo> enumerateDevices()
    {
        std::vector<UVCDeviceInfo> result;
        @autoreleasepool
        {
            NSArray* controllers = [UVCController uvcControllers];
            if (!controllers) return result;

            for (UVCController* ctrl in controllers)
            {
                UVCDeviceInfo info;
                NSString* name = [ctrl deviceName] ?: @"Unknown Camera";
                info.name = [name UTF8String] ?: "Unknown Camera";
                info.locationId = (uint32_t)[ctrl locationId];
                info.vendorId = (uint16_t)[ctrl vendorId];
                info.productId = (uint16_t)[ctrl productId];
                info.uvcVersion = (uint16_t)[ctrl uvcVersion];
                result.push_back(info);
            }
        }
        return result;
    }

    Handle openDevice(uint32_t locationId)
    {
        @autoreleasepool
        {
            UVCController* ctrl = [UVCController uvcControllerWithLocationId:(UInt32)locationId];
            if (!ctrl) return nullptr;

            [ctrl retain];
            [ctrl setIsInterfaceOpen:YES];
            if (![ctrl isInterfaceOpen])
            {
                [ctrl release];
                return nullptr;
            }
            return (Handle)ctrl;
        }
    }

    void closeDevice(Handle handle)
    {
        if (!handle) return;
        @autoreleasepool
        {
            UVCController* ctrl = (UVCController*)handle;
            if ([ctrl isInterfaceOpen])
                [ctrl setIsInterfaceOpen:NO];
            [ctrl release];
        }
    }

    bool isDeviceOpen(Handle handle)
    {
        if (!handle) return false;
        @autoreleasepool
        {
            UVCController* ctrl = (UVCController*)handle;
            return [ctrl isInterfaceOpen] == YES;
        }
    }

    std::vector<UVCControlInfo> getControls(Handle handle)
    {
        std::vector<UVCControlInfo> result;
        if (!handle) return result;

        @autoreleasepool
        {
            UVCController* controller = (UVCController*)handle;
            if (![controller isInterfaceOpen]) return result;

            NSArray* controlNames = [UVCController controlStrings];
            for (NSString* cName in controlNames)
            {
                UVCControl* ctrl = [controller controlWithName:cName];
                if (!ctrl) continue;

                UVCControlInfo info;
                info.name = [cName UTF8String] ?: "";
                info.supportsGet = [ctrl supportsGetValue] == YES;
                info.supportsSet = [ctrl supportsSetValue] == YES;
                info.hasRange = [ctrl hasRange] == YES;
                info.hasStepSize = [ctrl hasStepSize] == YES;
                info.hasDefaultValue = [ctrl hasDefaultValue] == YES;
                info.isPanTilt = [cName isEqualToString:@"pan-tilt-abs"] || [cName isEqualToString:@"pan-tilt-rel"];
                info.isBool = isBooleanControlName(cName);

                if (info.supportsGet)
                {
                    id cur = parseUvcValue([ctrl currentValue]);
                    if ([cur isKindOfClass:[NSNumber class]])
                    {
                        info.currentValue = [(NSNumber*)cur doubleValue];
                    }
                    else if ([cur isKindOfClass:[NSDictionary class]])
                    {
                        NSDictionary* d = (NSDictionary*)cur;
                        id p = [d objectForKey:@"pan"] ?: [d objectForKey:@"pan-relative"];
                        id t = [d objectForKey:@"tilt"] ?: [d objectForKey:@"tilt-relative"];
                        if (p) info.panValue = [p doubleValue];
                        if (t) info.tiltValue = [t doubleValue];
                    }
                }

                if (info.hasRange)
                {
                    id minP = parseUvcValue([ctrl minimum]);
                    id maxP = parseUvcValue([ctrl maximum]);
                    if ([minP isKindOfClass:[NSNumber class]] && [maxP isKindOfClass:[NSNumber class]])
                    {
                        info.minValue = [(NSNumber*)minP doubleValue];
                        info.maxValue = [(NSNumber*)maxP doubleValue];
                    }
                    else if ([minP isKindOfClass:[NSDictionary class]] && [maxP isKindOfClass:[NSDictionary class]])
                    {
                        NSDictionary* dMin = (NSDictionary*)minP;
                        NSDictionary* dMax = (NSDictionary*)maxP;
                        id pMin = [dMin objectForKey:@"pan"];
                        id pMax = [dMax objectForKey:@"pan"];
                        id tMin = [dMin objectForKey:@"tilt"];
                        id tMax = [dMax objectForKey:@"tilt"];
                        if (pMin) info.panMin = [pMin doubleValue];
                        if (pMax) info.panMax = [pMax doubleValue];
                        if (tMin) info.tiltMin = [tMin doubleValue];
                        if (tMax) info.tiltMax = [tMax doubleValue];
                    }
                }

                if (info.hasStepSize)
                {
                    id stepP = parseUvcValue([ctrl stepSize]);
                    if ([stepP isKindOfClass:[NSNumber class]])
                        info.stepSize = [(NSNumber*)stepP doubleValue];
                }

                if (info.hasDefaultValue)
                {
                    id defP = parseUvcValue([ctrl defaultValue]);
                    if ([defP isKindOfClass:[NSNumber class]])
                    {
                        info.defaultValue = [(NSNumber*)defP doubleValue];
                    }
                    else if ([defP isKindOfClass:[NSDictionary class]])
                    {
                        NSDictionary* d = (NSDictionary*)defP;
                        id p = [d objectForKey:@"pan"];
                        id t = [d objectForKey:@"tilt"];
                        if (p) info.panDefault = [p doubleValue];
                        if (t) info.tiltDefault = [t doubleValue];
                    }
                }

                result.push_back(info);
            }
        }
        return result;
    }

    UVCTelemetry pollTelemetry(Handle handle, bool normalize)
    {
        UVCTelemetry telemetry;
        if (!handle) return telemetry;

        @autoreleasepool
        {
            UVCController* controller = (UVCController*)handle;
            if (![controller isInterfaceOpen]) return telemetry;

            telemetry.valid = true;
            NSArray* controlNames = [UVCController controlStrings];

            for (NSString* cName in controlNames)
            {
                UVCControl* ctrl = [controller controlWithName:cName];
                if (!ctrl || ![ctrl supportsGetValue]) continue;

                UVCValue* curVal = [ctrl currentValue];
                if (!curVal) continue;

                id parsedVal = parseUvcValue(curVal);
                if (!parsedVal) continue;

                if ([cName isEqualToString:@"pan-tilt-abs"] && [parsedVal isKindOfClass:[NSDictionary class]])
                {
                    NSDictionary* ptDict = (NSDictionary*)parsedVal;
                    id p = [ptDict objectForKey:@"pan"];
                    id t = [ptDict objectForKey:@"tilt"];
                    if (p) { telemetry.pan = [p doubleValue]; telemetry.hasPan = true; }
                    if (t) { telemetry.tilt = [t doubleValue]; telemetry.hasTilt = true; }

                    if (normalize && [ctrl hasRange])
                    {
                        id minP = parseUvcValue([ctrl minimum]);
                        id maxP = parseUvcValue([ctrl maximum]);
                        if ([minP isKindOfClass:[NSDictionary class]] && [maxP isKindOfClass:[NSDictionary class]])
                        {
                            double pMin = [[(NSDictionary*)minP objectForKey:@"pan"] doubleValue];
                            double pMax = [[(NSDictionary*)maxP objectForKey:@"pan"] doubleValue];
                            double tMin = [[(NSDictionary*)minP objectForKey:@"tilt"] doubleValue];
                            double tMax = [[(NSDictionary*)maxP objectForKey:@"tilt"] doubleValue];
                            telemetry.pan = lerpNorm(telemetry.pan, pMin, pMax);
                            telemetry.tilt = lerpNorm(telemetry.tilt, tMin, tMax);
                        }
                    }
                }
                else if ([cName isEqualToString:@"zoom-abs"] && [parsedVal isKindOfClass:[NSNumber class]])
                {
                    double z = [(NSNumber*)parsedVal doubleValue];
                    if (normalize && [ctrl hasRange])
                    {
                        id minP = parseUvcValue([ctrl minimum]);
                        id maxP = parseUvcValue([ctrl maximum]);
                        if ([minP isKindOfClass:[NSNumber class]] && [maxP isKindOfClass:[NSNumber class]])
                            z = lerpNorm(z, [(NSNumber*)minP doubleValue], [(NSNumber*)maxP doubleValue]);
                    }
                    telemetry.zoom = z;
                    telemetry.hasZoom = true;
                }
                else if ([parsedVal isKindOfClass:[NSNumber class]])
                {
                    double v = [(NSNumber*)parsedVal doubleValue];
                    if (normalize && !isBooleanControlName(cName) && [ctrl hasRange])
                    {
                        id minP = parseUvcValue([ctrl minimum]);
                        id maxP = parseUvcValue([ctrl maximum]);
                        if ([minP isKindOfClass:[NSNumber class]] && [maxP isKindOfClass:[NSNumber class]])
                            v = lerpNorm(v, [(NSNumber*)minP doubleValue], [(NSNumber*)maxP doubleValue]);
                    }
                    std::string key = [cName UTF8String] ?: "";
                    if (!key.empty())
                        telemetry.scalarValues[key] = v;
                }
            }
        }
        return telemetry;
    }

    bool setControlValueString(Handle handle, const std::string& controlName, const std::string& valueStr)
    {
        if (!handle || controlName.empty()) return false;

        @autoreleasepool
        {
            UVCController* controller = (UVCController*)handle;
            if (![controller isInterfaceOpen]) return false;

            NSString* objcName = [NSString stringWithUTF8String:controlName.c_str()];
            UVCControl* ctrl = [controller controlWithName:objcName];
            if (!ctrl || ![ctrl supportsSetValue]) return false;

            BOOL ok = [ctrl setCurrentValueFromCString:valueStr.c_str() flags:kUVCTypeScanFlagShowWarnings];
            if (ok) ok = [ctrl writeFromCurrentValue];
            return ok == YES;
        }
    }

    bool setPanTilt(Handle handle, double pan, double tilt, bool normalize)
    {
        if (!handle) return false;

        @autoreleasepool
        {
            UVCController* controller = (UVCController*)handle;
            if (![controller isInterfaceOpen]) return false;

            UVCControl* ctrl = [controller controlWithName:@"pan-tilt-abs"];
            if (!ctrl || ![ctrl supportsSetValue]) return false;

            double targetPan = pan;
            double targetTilt = tilt;

            if (normalize && [ctrl hasRange])
            {
                id minP = parseUvcValue([ctrl minimum]);
                id maxP = parseUvcValue([ctrl maximum]);
                if ([minP isKindOfClass:[NSDictionary class]] && [maxP isKindOfClass:[NSDictionary class]])
                {
                    double pMin = [[(NSDictionary*)minP objectForKey:@"pan"] doubleValue];
                    double pMax = [[(NSDictionary*)maxP objectForKey:@"pan"] doubleValue];
                    double tMin = [[(NSDictionary*)minP objectForKey:@"tilt"] doubleValue];
                    double tMax = [[(NSDictionary*)maxP objectForKey:@"tilt"] doubleValue];
                    targetPan = denormLerp(pan, pMin, pMax);
                    targetTilt = denormLerp(tilt, tMin, tMax);
                }
            }

            std::ostringstream ss;
            ss << "{pan=" << std::llround(targetPan) << ", tilt=" << std::llround(targetTilt) << "}";
            BOOL ok = [ctrl setCurrentValueFromCString:ss.str().c_str() flags:kUVCTypeScanFlagShowWarnings];
            if (ok) ok = [ctrl writeFromCurrentValue];
            return ok == YES;
        }
    }

    bool setScalarControl(Handle handle, const std::string& controlName, double value, bool normalize)
    {
        if (!handle || controlName.empty()) return false;

        @autoreleasepool
        {
            UVCController* controller = (UVCController*)handle;
            if (![controller isInterfaceOpen]) return false;

            NSString* objcName = [NSString stringWithUTF8String:controlName.c_str()];
            UVCControl* ctrl = [controller controlWithName:objcName];
            if (!ctrl || ![ctrl supportsSetValue]) return false;

            double targetVal = value;
            if (normalize && !isBooleanControlName(objcName) && [ctrl hasRange])
            {
                id minP = parseUvcValue([ctrl minimum]);
                id maxP = parseUvcValue([ctrl maximum]);
                if ([minP isKindOfClass:[NSNumber class]] && [maxP isKindOfClass:[NSNumber class]])
                {
                    targetVal = denormLerp(value, [(NSNumber*)minP doubleValue], [(NSNumber*)maxP doubleValue]);
                }
            }

            std::ostringstream ss;
            if (isBooleanControlName(objcName))
                ss << (targetVal >= 0.5 ? 1 : 0);
            else
                ss << std::llround(targetVal);

            BOOL ok = [ctrl setCurrentValueFromCString:ss.str().c_str() flags:kUVCTypeScanFlagShowWarnings];
            if (ok) ok = [ctrl writeFromCurrentValue];
            return ok == YES;
        }
    }

    bool resetControlToDefault(Handle handle, const std::string& controlName)
    {
        if (!handle || controlName.empty()) return false;

        @autoreleasepool
        {
            UVCController* controller = (UVCController*)handle;
            if (![controller isInterfaceOpen]) return false;

            NSString* objcName = [NSString stringWithUTF8String:controlName.c_str()];
            UVCControl* ctrl = [controller controlWithName:objcName];
            if (!ctrl || ![ctrl supportsSetValue] || ![ctrl hasDefaultValue]) return false;

            return [ctrl resetToDefaultValue] == YES;
        }
    }
}

#else

namespace UVCBridge
{
    std::vector<UVCDeviceInfo> enumerateDevices() { return {}; }
    Handle openDevice(uint32_t) { return nullptr; }
    void closeDevice(Handle) {}
    bool isDeviceOpen(Handle) { return false; }
    std::vector<UVCControlInfo> getControls(Handle) { return {}; }
    UVCTelemetry pollTelemetry(Handle, bool) { return {}; }
    bool setControlValueString(Handle, const std::string&, const std::string&) { return false; }
    bool setPanTilt(Handle, double, double, bool) { return false; }
    bool setScalarControl(Handle, const std::string&, double, bool) { return false; }
    bool resetControlToDefault(Handle, const std::string&) { return false; }
}

#endif
