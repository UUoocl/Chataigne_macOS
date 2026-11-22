/*
  ==============================================================================

    UVCManager.cpp
    Manages connected UVC camera devices and USB hot-plugging

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

juce_ImplementSingleton(UVCManager)

UVCManager::UVCManager() :
	queuedNotifier(100)
{
	checkDevices();
	startTimerHz(1);
}

UVCManager::~UVCManager()
{
	stopTimer();
	devices.clear();
}

void UVCManager::checkDevices()
{
	bool changed = false;
	std::vector<UVCDeviceInfo> foundInfos = UVCBridge::enumerateDevices();
	Array<uint32_t> foundLocations;

	for (const auto& info : foundInfos)
	{
		foundLocations.add(info.locationId);

		if (getDeviceByLocationId(info.locationId) == nullptr)
		{
			auto* dev = new UVCDevice(info);
			devices.add(dev);
			LOG("UVC Camera added : " << String(info.name)
				<< " (VID: " << String::toHexString((int)info.vendorId)
				<< ", PID: " << String::toHexString((int)info.productId)
				<< ", Loc: " << String::toHexString((int)info.locationId) << ")");
			deviceManagerListeners.call(&UVCManagerListener::uvcDeviceAdded, dev);
			changed = true;
		}
	}

	Array<UVCDevice*> devicesToRemove;
	for (auto* d : devices)
	{
		if (!foundLocations.contains(d->info.locationId))
		{
			LOG("UVC Camera removed : " << String(d->info.name));
			devicesToRemove.add(d);
		}
	}

	for (auto* d : devicesToRemove)
	{
		deviceManagerListeners.call(&UVCManagerListener::uvcDeviceRemoved, d);
		devices.removeObject(d);
		changed = true;
	}

	if (changed)
	{
		queuedNotifier.addMessage(new UVCManagerEvent(UVCManagerEvent::DEVICES_CHANGED));
	}
}

UVCDevice* UVCManager::getDeviceByLocationId(uint32_t locationId) const
{
	for (auto* d : devices)
		if (d->info.locationId == locationId)
			return d;
	return nullptr;
}

UVCDevice* UVCManager::getDeviceByName(const String& name) const
{
	for (auto* d : devices)
		if (String(d->info.name) == name)
			return d;
	return nullptr;
}

void UVCManager::timerCallback()
{
	checkDevices();
}
