/*
  ==============================================================================

    UVCManager.h
    Manages connected UVC camera devices and USB hot-plugging

  ==============================================================================
*/

#pragma once

class UVCManager :
	public Timer
{
public:
	juce_DeclareSingleton(UVCManager, true)

	UVCManager();
	~UVCManager() override;

	OwnedArray<UVCDevice> devices;

	void checkDevices();
	UVCDevice* getDeviceByLocationId(uint32_t locationId) const;
	UVCDevice* getDeviceByName(const String& name) const;

	void timerCallback() override;

	class UVCManagerListener
	{
	public:
		virtual ~UVCManagerListener() {}
		virtual void uvcDeviceAdded(UVCDevice*) = 0;
		virtual void uvcDeviceRemoved(UVCDevice*) = 0;
	};

	ListenerList<UVCManagerListener> deviceManagerListeners;
	void addUVCManagerListener(UVCManagerListener* newListener) { deviceManagerListeners.add(newListener); }
	void removeUVCManagerListener(UVCManagerListener* listener) { deviceManagerListeners.remove(listener); }

	class UVCManagerEvent
	{
	public:
		enum Type { DEVICES_CHANGED };
		UVCManagerEvent(Type type) : type(type) {}
		Type type;
	};

	QueuedNotifier<UVCManagerEvent> queuedNotifier;
	typedef QueuedNotifier<UVCManagerEvent>::Listener AsyncListener;
	void addAsyncManagerListener(AsyncListener* newListener) { queuedNotifier.addListener(newListener); }
	void addAsyncCoalescedManagerListener(AsyncListener* newListener) { queuedNotifier.addAsyncCoalescedListener(newListener); }
	void removeAsyncManagerListener(AsyncListener* listener) { queuedNotifier.removeListener(listener); }
};
