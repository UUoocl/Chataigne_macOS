/*
  ==============================================================================

    UVCModule.h
    Chataigne module to monitor and control UVC USB cameras

  ==============================================================================
*/

#pragma once

class UVCModule :
	public Module,
	public UVCManager::UVCManagerListener,
	public UVCDevice::UVCDeviceListener
{
public:
	UVCModule(const String& name = "UVC Camera");
	~UVCModule() override;

	BoolParameter* isConnected;
	EnumParameter* cameraTarget;
	IntParameter* pollRate;
	BoolParameter* normalize;
	Trigger* refreshDevices;
	Trigger* resetDefaults;

	UVCDevice* device = nullptr;
	bool isUpdatingFromHardware = false;

	ControllableContainer ptzCC;
	FloatParameter* panParam;
	FloatParameter* tiltParam;
	FloatParameter* zoomParam;

	ControllableContainer controlsCC;
	std::map<std::string, Parameter*> controlParams;

	void rebuildDeviceOptions();
	void selectTargetDevice();
	void setDevice(UVCDevice* newDevice);
	void rebuildControls();
	void updateControlRanges();

	void setPanTilt(double pan, double tilt);
	void setPan(double pan);
	void setTilt(double tilt);
	void setZoom(double zoom);
	void setControlValue(const String& controlName, double value);
	void resetControl(const String& controlName);

	void uvcTelemetryUpdated(UVCDevice* dev, const UVCTelemetry& telemetry) override;
	void uvcDeviceDisconnected(UVCDevice* dev) override;

	void uvcDeviceAdded(UVCDevice* dev) override;
	void uvcDeviceRemoved(UVCDevice* dev) override;

	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;
	void afterLoadJSONDataInternal() override;

	static UVCModule* create() { return new UVCModule(); }
	virtual String getDefaultTypeString() const override { return "UVC Camera"; }
};
