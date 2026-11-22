/*
  ==============================================================================

    UVCDevice.cpp
    Represents a connected UVC camera device with background telemetry polling

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

UVCDevice::UVCDevice(const UVCDeviceInfo& _info) :
	Thread("UVC_" + String(_info.locationId)),
	info(_info)
{
}

UVCDevice::~UVCDevice()
{
	masterReference.clear();
	stopPolling();
	close();
}

bool UVCDevice::open()
{
	const ScopedLock lock(hardwareLock);
	if (handle != nullptr && UVCBridge::isDeviceOpen(handle))
		return true;

	if (handle != nullptr)
	{
		UVCBridge::closeDevice(handle);
		handle = nullptr;
	}

	handle = UVCBridge::openDevice(info.locationId);
	if (handle == nullptr)
		return false;

	controls = UVCBridge::getControls(handle);
	return true;
}

void UVCDevice::close()
{
	const ScopedLock lock(hardwareLock);
	if (handle != nullptr)
	{
		UVCBridge::closeDevice(handle);
		handle = nullptr;
	}
	controls.clear();
}

bool UVCDevice::isOpen() const
{
	return handle != nullptr && UVCBridge::isDeviceOpen(handle);
}

void UVCDevice::setPollRate(int hz)
{
	pollRateHz.store(jlimit(1, 120, hz));
}

void UVCDevice::setNormalize(bool norm)
{
	normalize.store(norm);
}

void UVCDevice::startPolling()
{
	if (!isOpen() && !open())
		return;

	if (!isThreadRunning())
	{
		pollingActive.store(true);
		startThread();
	}
}

void UVCDevice::stopPolling()
{
	pollingActive.store(false);
	signalThreadShouldExit();
	if (isThreadRunning())
		stopThread(1000);
}

std::vector<UVCControlInfo> UVCDevice::getControlsSnapshot()
{
	const ScopedLock lock(hardwareLock);
	if (controls.empty() && handle != nullptr)
		controls = UVCBridge::getControls(handle);
	return controls;
}

bool UVCDevice::setPanTilt(double pan, double tilt, bool normalized)
{
	const ScopedLock lock(hardwareLock);
	if (handle == nullptr) return false;
	return UVCBridge::setPanTilt(handle, pan, tilt, normalized);
}

bool UVCDevice::setScalarControl(const String& controlName, double value, bool normalized)
{
	const ScopedLock lock(hardwareLock);
	if (handle == nullptr) return false;
	return UVCBridge::setScalarControl(handle, controlName.toStdString(), value, normalized);
}

bool UVCDevice::resetControl(const String& controlName)
{
	const ScopedLock lock(hardwareLock);
	if (handle == nullptr) return false;
	return UVCBridge::resetControlToDefault(handle, controlName.toStdString());
}

void UVCDevice::resetAllControls()
{
	const ScopedLock lock(hardwareLock);
	if (handle == nullptr) return;
	for (const auto& c : controls)
	{
		if (c.supportsSet && c.hasDefaultValue)
			UVCBridge::resetControlToDefault(handle, c.name);
	}
}

void UVCDevice::run()
{
	while (!threadShouldExit() && pollingActive.load())
	{
		UVCTelemetry telemetry;
		bool deviceLost = false;

		{
			const ScopedLock lock(hardwareLock);
			if (handle == nullptr || !UVCBridge::isDeviceOpen(handle))
			{
				deviceLost = true;
			}
			else
			{
				telemetry = UVCBridge::pollTelemetry(handle, normalize.load());
			}
		}

		if (deviceLost)
		{
			WeakReference<UVCDevice> weakThis(this);
			MessageManager::callAsync([weakThis]()
			{
				if (weakThis != nullptr)
					weakThis->listeners.call(&UVCDeviceListener::uvcDeviceDisconnected, weakThis.get());
			});
			break;
		}

		if (telemetry.valid)
		{
			WeakReference<UVCDevice> weakThis(this);
			MessageManager::callAsync([weakThis, telemetry]()
			{
				if (weakThis != nullptr)
					weakThis->listeners.call(&UVCDeviceListener::uvcTelemetryUpdated, weakThis.get(), telemetry);
			});
		}

		int hz = jlimit(1, 120, pollRateHz.load());
		int waitMs = jmax(5, 1000 / hz);
		wait(waitMs);
	}
}
