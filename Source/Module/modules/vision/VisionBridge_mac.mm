/*
  ==============================================================================

    VisionBridge_mac.mm
    macOS AVFoundation + Vision.framework implementation for Pose, Face, and Hands

  ==============================================================================
*/

#include "VisionBridge.h"

#if defined(__APPLE__)

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>

#include <atomic>
#include <chrono>
#include <mutex>

namespace
{
    std::vector<VisionCameraDevice> enumerateCamerasInternal()
    {
        std::vector<VisionCameraDevice> list;
        list.push_back({ "Default Camera", "Default" });

        NSMutableArray<AVCaptureDeviceType>* types = [NSMutableArray arrayWithObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
        if (@available(macOS 13.0, *))
            [types addObject:AVCaptureDeviceTypeDeskViewCamera];
        if (@available(macOS 14.0, *))
            [types addObject:AVCaptureDeviceTypeContinuityCamera];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (@available(macOS 10.15, *))
        {
            [types addObject:AVCaptureDeviceTypeExternalUnknown];
        }
#pragma clang diagnostic pop

        AVCaptureDeviceDiscoverySession* session = [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:types
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];

        for (AVCaptureDevice* dev in session.devices)
        {
            const char* nameUtf8 = [dev.localizedName UTF8String];
            const char* uidUtf8 = [dev.uniqueID UTF8String];
            list.push_back({
                nameUtf8 ? nameUtf8 : "Camera",
                uidUtf8 ? uidUtf8 : ""
            });
        }
        return list;
    }

    NSString* serializeJson(id obj)
    {
        if (!obj || ![NSJSONSerialization isValidJSONObject:obj])
            return @"{}";
        NSError* err = nil;
        NSData* data = [NSJSONSerialization dataWithJSONObject:obj options:0 error:&err];
        if (!data || err)
            return @"{}";
        return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"{}";
    }

    CGRect makeClampedRoi(const VisionConfig& cfg)
    {
        double x = fmax(0.0, fmin(1.0, (double)cfg.roiX));
        double y = fmax(0.0, fmin(1.0, (double)cfg.roiY));
        double w = fmax(0.01, fmin(1.0 - x, (double)cfg.roiWidth));
        double h = fmax(0.01, fmin(1.0 - y, (double)cfg.roiHeight));
        return CGRectMake(x, y, w, h);
    }
}

@interface ChataigneVisionEngine : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    VisionCallbacks _callbacks;
    std::mutex _cbMutex;
    std::mutex _cfgMutex;
    VisionConfig _config;
    dispatch_queue_t _captureQueue;
    std::atomic<bool> _isProcessing;
    std::atomic<bool> _running;
    std::atomic<bool> _destroyed;
    std::chrono::steady_clock::time_point _lastFrameTime;
}

@property (nonatomic, strong) AVCaptureSession* captureSession;
@property (nonatomic, strong) AVCaptureDeviceInput* videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput* videoOutput;

- (instancetype)initWithCallbacks:(const VisionCallbacks&)callbacks;
- (void)shutdown;
- (void)startWithConfig:(const VisionConfig&)config;
- (void)stop;
- (void)updateConfig:(const VisionConfig&)config;

@end

@implementation ChataigneVisionEngine

- (instancetype)initWithCallbacks:(const VisionCallbacks&)callbacks
{
    self = [super init];
    if (self)
    {
        _callbacks = callbacks;
        _captureQueue = dispatch_queue_create("com.chataigne.vision.capture", DISPATCH_QUEUE_SERIAL);
        _isProcessing.store(false);
        _running.store(false);
        _destroyed.store(false);
        _lastFrameTime = std::chrono::steady_clock::now();

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(devicesChanged:)
                                                     name:AVCaptureDeviceWasConnectedNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(devicesChanged:)
                                                     name:AVCaptureDeviceWasDisconnectedNotification
                                                   object:nil];
    }
    return self;
}

- (void)devicesChanged:(NSNotification*)note
{
    auto devs = enumerateCamerasInternal();
    std::lock_guard<std::mutex> lock(_cbMutex);
    if (_destroyed.load() || !_callbacks.onDevicesChanged) return;
    _callbacks.onDevicesChanged(devs);
}

- (void)emitStatus:(NSString*)status isRunning:(BOOL)running
{
    std::lock_guard<std::mutex> lock(_cbMutex);
    if (_destroyed.load() || !_callbacks.onStatus) return;
    const char* utf8 = [status UTF8String];
    _callbacks.onStatus(utf8 ? utf8 : "", running == YES);
}

- (void)emitResult:(NSString*)jsonStr count:(int)count
{
    std::lock_guard<std::mutex> lock(_cbMutex);
    if (_destroyed.load() || !_callbacks.onResult) return;
    const char* utf8 = [jsonStr UTF8String];
    _callbacks.onResult(utf8 ? utf8 : "{}", count);
}

- (void)shutdown
{
    {
        std::lock_guard<std::mutex> lock(_cbMutex);
        _destroyed.store(true);
        _callbacks = {};
    }
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self stop];
}

- (void)startWithConfig:(const VisionConfig&)config
{
    std::string prevDeviceId;
    {
        std::lock_guard<std::mutex> lock(_cfgMutex);
        prevDeviceId = _config.deviceId;
        _config = config;
    }

    NSString* camDesc = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSCameraUsageDescription"];
    if (camDesc == nil)
    {
        _running.store(false);
        [self emitStatus:@"Missing NSCameraUsageDescription in Info.plist" isRunning:NO];
        return;
    }

    AVAuthorizationStatus auth = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (auth == AVAuthorizationStatusNotDetermined)
    {
        [self emitStatus:@"Requesting Camera Permission..." isRunning:NO];
        __weak ChataigneVisionEngine* weakSelf = self;
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
            dispatch_async(dispatch_get_main_queue(), ^{
                ChataigneVisionEngine* strongSelf = weakSelf;
                if (!strongSelf || strongSelf->_destroyed.load()) return;
                if (granted)
                {
                    VisionConfig cfgCopy;
                    {
                        std::lock_guard<std::mutex> lock(strongSelf->_cfgMutex);
                        cfgCopy = strongSelf->_config;
                    }
                    [strongSelf startCaptureInternal:cfgCopy];
                }
                else
                {
                    [strongSelf emitStatus:@"Camera Permission Denied" isRunning:NO];
                }
            });
        }];
        return;
    }
    else if (auth != AVAuthorizationStatusAuthorized)
    {
        _running.store(false);
        [self emitStatus:@"Camera Permission Denied" isRunning:NO];
        return;
    }

    [self startCaptureInternal:config];
}

- (void)startCaptureInternal:(const VisionConfig&)config
{
    [self stopCaptureOnly];

    AVCaptureDevice* device = nil;
    if (!config.deviceId.empty() && config.deviceId != "Default")
    {
        device = [AVCaptureDevice deviceWithUniqueID:[NSString stringWithUTF8String:config.deviceId.c_str()]];
    }
    if (!device)
    {
        device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }
    if (!device)
    {
        _running.store(false);
        [self emitStatus:@"No Camera Found" isRunning:NO];
        return;
    }

    NSError* err = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&err];
    if (!input || err)
    {
        _running.store(false);
        [self emitStatus:@"Camera Open Failed" isRunning:NO];
        return;
    }

    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    [session beginConfiguration];
    if ([session canSetSessionPreset:AVCaptureSessionPreset640x480])
        session.sessionPreset = AVCaptureSessionPreset640x480;

    if ([session canAddInput:input])
        [session addInput:input];

    AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
    output.alwaysDiscardsLateVideoFrames = YES;
    output.videoSettings = @{
        (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
    };
    [output setSampleBufferDelegate:self queue:_captureQueue];

    if ([session canAddOutput:output])
        [session addOutput:output];

    [session commitConfiguration];

    self.captureSession = session;
    self.videoInput = input;
    self.videoOutput = output;

    _running.store(true);
    [session startRunning];
    [self emitStatus:[NSString stringWithFormat:@"Running (%@)", device.localizedName ?: @"Camera"] isRunning:YES];
}

- (void)stopCaptureOnly
{
    if (self.captureSession)
    {
        if (self.videoOutput)
            [self.videoOutput setSampleBufferDelegate:nil queue:nil];
        if (self.captureSession.isRunning)
            [self.captureSession stopRunning];
    }
    self.captureSession = nil;
    self.videoInput = nil;
    self.videoOutput = nil;
    _running.store(false);
}

- (void)stop
{
    [self stopCaptureOnly];
    [self emitStatus:@"Stopped" isRunning:NO];
}

- (void)updateConfig:(const VisionConfig&)config
{
    bool deviceChanged = false;
    {
        std::lock_guard<std::mutex> lock(_cfgMutex);
        deviceChanged = (_config.deviceId != config.deviceId);
        _config = config;
    }

    if (deviceChanged && _running.load())
    {
        [self startCaptureInternal:config];
    }
}

- (void)captureOutput:(AVCaptureOutput*)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection*)connection
{
    if (_destroyed.load() || !_running.load())
        return;

    VisionConfig cfg;
    {
        std::lock_guard<std::mutex> lock(_cfgMutex);
        cfg = _config;
    }

    auto now = std::chrono::steady_clock::now();
    int fps = cfg.targetFps > 0 ? cfg.targetFps : 30;
    double minIntervalMs = 1000.0 / (double)fps;
    double elapsedMs = std::chrono::duration<double, std::milli>(now - _lastFrameTime).count();
    if (elapsedMs < minIntervalMs)
        return;

    bool expected = false;
    if (!_isProcessing.compare_exchange_strong(expected, true))
        return;

    _lastFrameTime = now;

    CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer)
    {
        _isProcessing.store(false);
        return;
    }

    @autoreleasepool
    {
        VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCVPixelBuffer:pixelBuffer options:@{}];
        NSError* reqErr = nil;
        int detectedCount = 0;
        NSString* jsonPayload = nil;

        if (cfg.task == VisionTaskType::Pose2D)
        {
            if (@available(macOS 11.0, *))
            {
                VNDetectHumanBodyPoseRequest* req = [[VNDetectHumanBodyPoseRequest alloc] init];
                req.regionOfInterest = makeClampedRoi(cfg);
                if ([handler performRequests:@[req] error:&reqErr] && !reqErr)
                {
                    NSMutableArray* poses = [NSMutableArray array];
                    for (VNHumanBodyPoseObservation* obs in req.results)
                    {
                        NSDictionary<VNHumanBodyPoseObservationJointName, VNRecognizedPoint*>* pts = [obs recognizedPointsForJointsGroupName:VNHumanBodyPoseObservationJointsGroupNameAll error:nil];
                        if (!pts) continue;
                        NSMutableDictionary* joints = [NSMutableDictionary dictionary];
                        for (VNHumanBodyPoseObservationJointName jName in pts)
                        {
                            VNRecognizedPoint* pt = pts[jName];
                            if (pt.confidence >= cfg.minConfidence)
                            {
                                [joints setObject:@{
                                    @"x": @(pt.location.x),
                                    @"y": @(pt.location.y),
                                    @"confidence": @(pt.confidence)
                                } forKey:jName];
                            }
                        }
                        [poses addObject:@{ @"joints": joints }];
                    }
                    detectedCount = (int)[poses count];
                    jsonPayload = serializeJson(@{
                        @"count": @(detectedCount),
                        @"mode": @"2D",
                        @"poses": poses
                    });
                }
            }
        }
        else if (cfg.task == VisionTaskType::Pose3D)
        {
            if (@available(macOS 14.0, *))
            {
                VNDetectHumanBodyPose3DRequest* req = [[VNDetectHumanBodyPose3DRequest alloc] init];
                req.regionOfInterest = makeClampedRoi(cfg);
                if ([handler performRequests:@[req] error:&reqErr] && !reqErr)
                {
                    NSMutableArray* poses = [NSMutableArray array];
                    for (VNHumanBodyPose3DObservation* obs in req.results)
                    {
                        NSDictionary<VNHumanBodyPose3DObservationJointName, VNHumanBodyRecognizedPoint3D*>* pts = [obs recognizedPointsForJointsGroupName:VNHumanBodyPose3DObservationJointsGroupNameAll error:nil];
                        if (!pts) continue;
                        NSMutableDictionary* joints = [NSMutableDictionary dictionary];
                        for (VNHumanBodyPose3DObservationJointName jName in pts)
                        {
                            VNHumanBodyRecognizedPoint3D* pt = pts[jName];
                            simd_float4x4 m = pt.localPosition;
                            [joints setObject:@{
                                @"x": @(m.columns[3].x),
                                @"y": @(m.columns[3].y),
                                @"z": @(m.columns[3].z)
                            } forKey:jName];
                        }
                        [poses addObject:@{
                            @"bodyHeight": @(obs.bodyHeight),
                            @"joints": joints
                        }];
                    }
                    detectedCount = (int)[poses count];
                    jsonPayload = serializeJson(@{
                        @"count": @(detectedCount),
                        @"mode": @"3D",
                        @"poses": poses
                    });
                }
            }
        }
        else if (cfg.task == VisionTaskType::Face)
        {
            VNImageBasedRequest* req = cfg.includeLandmarks
                ? (VNImageBasedRequest*)[[VNDetectFaceLandmarksRequest alloc] init]
                : (VNImageBasedRequest*)[[VNDetectFaceRectanglesRequest alloc] init];
            req.regionOfInterest = makeClampedRoi(cfg);

            if ([handler performRequests:@[req] error:&reqErr] && !reqErr)
            {
                NSMutableArray* faces = [NSMutableArray array];
                for (VNFaceObservation* obs in req.results)
                {
                    if (obs.confidence < cfg.minConfidence) continue;

                    NSMutableDictionary* faceObj = [NSMutableDictionary dictionary];
                    [faceObj setObject:@{
                        @"x": @(obs.boundingBox.origin.x),
                        @"y": @(obs.boundingBox.origin.y),
                        @"width": @(obs.boundingBox.size.width),
                        @"height": @(obs.boundingBox.size.height)
                    } forKey:@"boundingBox"];
                    [faceObj setObject:@(obs.confidence) forKey:@"confidence"];

                    if (obs.roll) [faceObj setObject:obs.roll forKey:@"roll"];
                    if (obs.yaw) [faceObj setObject:obs.yaw forKey:@"yaw"];
                    if (@available(macOS 12.0, *))
                    {
                        if (obs.pitch) [faceObj setObject:obs.pitch forKey:@"pitch"];
                    }

                    if (cfg.includeLandmarks && obs.landmarks)
                    {
                        NSMutableDictionary* lmDict = [NSMutableDictionary dictionary];
                        auto addRegion = ^(NSString* key, VNFaceLandmarkRegion2D* region) {
                            if (!region || region.pointCount == 0) return;
                            NSMutableArray* arr = [NSMutableArray arrayWithCapacity:region.pointCount];
                            const CGPoint* pts = region.normalizedPoints;
                            for (NSUInteger i = 0; i < region.pointCount; ++i)
                                [arr addObject:@{ @"x": @(pts[i].x), @"y": @(pts[i].y) }];
                            [lmDict setObject:arr forKey:key];
                        };
                        addRegion(@"faceContour", obs.landmarks.faceContour);
                        addRegion(@"leftEye", obs.landmarks.leftEye);
                        addRegion(@"rightEye", obs.landmarks.rightEye);
                        addRegion(@"leftEyebrow", obs.landmarks.leftEyebrow);
                        addRegion(@"rightEyebrow", obs.landmarks.rightEyebrow);
                        addRegion(@"nose", obs.landmarks.nose);
                        addRegion(@"noseCrest", obs.landmarks.noseCrest);
                        addRegion(@"outerLips", obs.landmarks.outerLips);
                        addRegion(@"innerLips", obs.landmarks.innerLips);
                        addRegion(@"leftPupil", obs.landmarks.leftPupil);
                        addRegion(@"rightPupil", obs.landmarks.rightPupil);
                        [faceObj setObject:lmDict forKey:@"landmarks"];
                    }
                    [faces addObject:faceObj];
                }
                detectedCount = (int)[faces count];
                jsonPayload = serializeJson(@{
                    @"count": @(detectedCount),
                    @"faces": faces
                });
            }
        }
        else if (cfg.task == VisionTaskType::Hands)
        {
            if (@available(macOS 11.0, *))
            {
                VNDetectHumanHandPoseRequest* req = [[VNDetectHumanHandPoseRequest alloc] init];
                req.maximumHandCount = (NSUInteger)(cfg.maxHandCount > 0 ? cfg.maxHandCount : 2);
                req.regionOfInterest = makeClampedRoi(cfg);

                if ([handler performRequests:@[req] error:&reqErr] && !reqErr)
                {
                    NSMutableArray* hands = [NSMutableArray array];
                    for (VNHumanHandPoseObservation* obs in req.results)
                    {
                        NSDictionary<VNHumanHandPoseObservationJointName, VNRecognizedPoint*>* pts = [obs recognizedPointsForJointsGroupName:VNHumanHandPoseObservationJointsGroupNameAll error:nil];
                        if (!pts) continue;
                        NSMutableDictionary* joints = [NSMutableDictionary dictionary];
                        for (VNHumanHandPoseObservationJointName jName in pts)
                        {
                            VNRecognizedPoint* pt = pts[jName];
                            if (pt.confidence >= cfg.minConfidence)
                            {
                                [joints setObject:@{
                                    @"x": @(pt.location.x),
                                    @"y": @(pt.location.y),
                                    @"confidence": @(pt.confidence)
                                } forKey:jName];
                            }
                        }
                        NSString* chirality = @"unknown";
                        if (@available(macOS 12.0, *))
                        {
                            if (obs.chirality == VNChiralityLeft) chirality = @"left";
                            else if (obs.chirality == VNChiralityRight) chirality = @"right";
                        }
                        [hands addObject:@{
                            @"chirality": chirality,
                            @"joints": joints
                        }];
                    }
                    detectedCount = (int)[hands count];
                    jsonPayload = serializeJson(@{
                        @"count": @(detectedCount),
                        @"hands": hands
                    });
                }
            }
        }

        if (jsonPayload)
            [self emitResult:jsonPayload count:detectedCount];
    }

    _isProcessing.store(false);
}

@end

namespace VisionBridge
{
    Handle create(const VisionCallbacks& callbacks)
    {
        @autoreleasepool
        {
            ChataigneVisionEngine* engine = [[ChataigneVisionEngine alloc] initWithCallbacks:callbacks];
            return (__bridge_retained void*)engine;
        }
    }

    void destroy(Handle handle)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneVisionEngine* engine = (__bridge_transfer ChataigneVisionEngine*)handle;
            [engine shutdown];
            engine = nil;
        }
    }

    void start(Handle handle, const VisionConfig& config)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneVisionEngine* engine = (__bridge ChataigneVisionEngine*)handle;
            [engine startWithConfig:config];
        }
    }

    void stop(Handle handle)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneVisionEngine* engine = (__bridge ChataigneVisionEngine*)handle;
            [engine stop];
        }
    }

    void updateConfig(Handle handle, const VisionConfig& config)
    {
        if (!handle) return;
        @autoreleasepool
        {
            ChataigneVisionEngine* engine = (__bridge ChataigneVisionEngine*)handle;
            [engine updateConfig:config];
        }
    }

    std::vector<VisionCameraDevice> getCameraDevices()
    {
        @autoreleasepool
        {
            return enumerateCamerasInternal();
        }
    }
}

#else

namespace VisionBridge
{
    Handle create(const VisionCallbacks&) { return nullptr; }
    void destroy(Handle) {}
    void start(Handle, const VisionConfig&) {}
    void stop(Handle) {}
    void updateConfig(Handle, const VisionConfig&) {}
    std::vector<VisionCameraDevice> getCameraDevices() { return { { "Default Camera", "Default" } }; }
}

#endif
