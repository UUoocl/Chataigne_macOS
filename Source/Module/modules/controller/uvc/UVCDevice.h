/*
  ==============================================================================

    UVCDevice.h
    Represents a connected UVC camera device with background telemetry polling

  ==============================================================================
*/

#pragma once

class UVCDevice :
	public Thread
{
public:
	UVCDevice(const UVCDeviceInfo& info);
	~UVCDevice() override;

	UVCDeviceInfo info;
	UVCBridge::Handle handle = nullptr;

	CriticalSection hardwareLock;
	std::vector<UVCControlInfo> controls;

	std::atomic<int> pollRateHz { 30 };
	std::atomic<bool> normalize { false };
	std::atomic<bool> pollingActive { false };

	bool open();
	void close();
	bool isOpen() const;

	void setPollRate(int hz);
	void setNormalize(bool norm);
	void startPolling();
	void stopPolling();

	std::vector<UVCControlInfo> getControlsSnapshot();
	bool setPanTilt(double pan, double tilt, bool normalized);
	bool setScalarControl(const String& controlName, double value, bool normalized);
	bool resetControl(const String& controlName);
	void resetAllControls();

	void run() override;

	class UVCDeviceListener
	{
	public:
		virtual ~UVCDeviceListener() {}
		virtual void uvcTelemetryUpdated(UVCDevice* device, const UVCTelemetry& telemetry) {}
		virtual void uvcDeviceDisconnected(UVCDevice* device) {}
	};

	ListenerList<UVCDeviceListener> listeners;
	void addUVCDeviceListener(UVCDeviceListener* l) { listeners.add(l); }
	void removeUVCDeviceListener(UVCDeviceListener* l) { listeners.remove(l); }

	WeakReference<UVCDevice>::Master masterReference;
};
