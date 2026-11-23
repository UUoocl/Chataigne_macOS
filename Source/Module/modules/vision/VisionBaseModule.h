/*
  ==============================================================================

    VisionBaseModule.h
    Shared base class for Apple Vision modules (Pose, Face, Hands)

  ==============================================================================
*/

#pragma once

class VisionBaseModule :
	public Module
{
public:
	VisionBaseModule(const String& name, const String& detectedCountName);
	~VisionBaseModule() override;

	BoolParameter* active;
	BoolParameter* isRunning;
	StringParameter* status;
	EnumParameter* cameraDevice;
	FloatParameter* minConfidence;
	IntParameter* targetFps;
	ControllableContainer roiCC;
	FloatParameter* roiX;
	FloatParameter* roiY;
	FloatParameter* roiWidth;
	FloatParameter* roiHeight;

	IntParameter* detectedCount;
	StringParameter* jsonOutput;
	Trigger* onDetected;

	VisionBridge::Handle engineHandle = nullptr;

	virtual VisionConfig buildVisionConfig() const = 0;
	void fillBaseConfig(VisionConfig& cfg) const;

	void rebuildCameras(const std::vector<VisionCameraDevice>& devices);
	void startVision();
	void stopVision();

	void handleResult(const String& json, int count);
	void handleStatus(const String& newStatus, bool running);

	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;
	void afterLoadJSONDataInternal() override;

	WeakReference<VisionBaseModule>::Master masterReference;
};
