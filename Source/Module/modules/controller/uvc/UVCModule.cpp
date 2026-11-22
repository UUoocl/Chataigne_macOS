/*
  ==============================================================================

    UVCModule.cpp
    Chataigne module to monitor and control UVC USB cameras

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

UVCModule::UVCModule(const String& name) :
	Module(name),
	ptzCC("PTZ"),
	controlsCC("Controls")
{
	setupIOConfiguration(true, true);

	isConnected = moduleParams.addBoolParameter("Is Connected", "Is a UVC camera connected?", false);
	isConnected->setControllableFeedbackOnly(true);
	connectionFeedbackRef = isConnected;

	cameraTarget = moduleParams.addEnumParameter("Camera Target", "Target UVC camera device");
	pollRate = moduleParams.addIntParameter("Poll Rate", "Hardware telemetry polling frequency in Hz", 30, 1, 120);
	normalize = moduleParams.addBoolParameter("Normalize", "Normalize control values to 0.0 - 1.0 using hardware min/max", false);
	refreshDevices = moduleParams.addTrigger("Refresh Devices", "Rescan USB bus for connected UVC cameras");
	resetDefaults = moduleParams.addTrigger("Reset to Defaults", "Reset all supported camera controls to their hardware defaults");

	valuesCC.addChildControllableContainer(&ptzCC);
	panParam = ptzCC.addFloatParameter("Pan", "Camera Pan (absolute)", 0.0f, -648000.0f, 648000.0f);
	tiltParam = ptzCC.addFloatParameter("Tilt", "Camera Tilt (absolute)", 0.0f, -648000.0f, 648000.0f);
	zoomParam = ptzCC.addFloatParameter("Zoom", "Camera Zoom (absolute)", 0.0f, 0.0f, 1000.0f);

	valuesCC.addChildControllableContainer(&controlsCC);

	defManager->add(CommandDefinition::createDef(this, "", "Set Pan / Tilt", &UVCCommand::create)->addParam("action", UVCCommand::SET_PAN_TILT));
	defManager->add(CommandDefinition::createDef(this, "", "Set Pan", &UVCCommand::create)->addParam("action", UVCCommand::SET_PAN));
	defManager->add(CommandDefinition::createDef(this, "", "Set Tilt", &UVCCommand::create)->addParam("action", UVCCommand::SET_TILT));
	defManager->add(CommandDefinition::createDef(this, "", "Set Zoom", &UVCCommand::create)->addParam("action", UVCCommand::SET_ZOOM));
	defManager->add(CommandDefinition::createDef(this, "", "Set Control", &UVCCommand::create)->addParam("action", UVCCommand::SET_CONTROL));
	defManager->add(CommandDefinition::createDef(this, "", "Reset Control", &UVCCommand::create)->addParam("action", UVCCommand::RESET_CONTROL));

	rebuildDeviceOptions();
	selectTargetDevice();

	UVCManager::getInstance()->addUVCManagerListener(this);
}

UVCModule::~UVCModule()
{
	if (UVCManager::getInstanceWithoutCreating() != nullptr)
		UVCManager::getInstance()->removeUVCManagerListener(this);
	setDevice(nullptr);
}

void UVCModule::rebuildDeviceOptions()
{
	var currentData = cameraTarget->getValueData();
	cameraTarget->clearOptions();
	cameraTarget->addOption("Auto (First Available)", var("auto"));

	for (auto* d : UVCManager::getInstance()->devices)
	{
		String label = String(d->info.name) + " (" + String::toHexString((int)d->info.locationId) + ")";
		cameraTarget->addOption(label, var(String((uint32)d->info.locationId)));
	}

	if (!currentData.isVoid() && currentData.toString().isNotEmpty())
		cameraTarget->setValueWithData(currentData);
	else
		cameraTarget->setValueWithData(var("auto"));
}

void UVCModule::selectTargetDevice()
{
	auto* mgr = UVCManager::getInstance();
	String targetData = cameraTarget->getValueData().toString();

	UVCDevice* chosen = nullptr;
	if (targetData == "auto" || targetData.isEmpty())
	{
		if (mgr->devices.size() > 0)
			chosen = mgr->devices[0];
	}
	else
	{
		uint32_t locId = (uint32_t)targetData.getLargeIntValue();
		chosen = mgr->getDeviceByLocationId(locId);
	}

	setDevice(chosen);
}

void UVCModule::setDevice(UVCDevice* newDevice)
{
	if (device == newDevice)
		return;

	if (device != nullptr)
	{
		device->removeUVCDeviceListener(this);
		device->stopPolling();
		device->close();
	}

	device = newDevice;

	if (device != nullptr)
	{
		if (device->open())
		{
			device->setPollRate(pollRate->intValue());
			device->setNormalize(normalize->boolValue());
			device->addUVCDeviceListener(this);
			rebuildControls();
			if (enabled->boolValue())
				device->startPolling();
			isConnected->setValue(true);
		}
		else
		{
			device = nullptr;
			isConnected->setValue(false);
		}
	}
	else
	{
		isConnected->setValue(false);
	}
}

void UVCModule::rebuildControls()
{
	controlsCC.clear();
	controlParams.clear();

	if (device == nullptr)
		return;

	updateControlRanges();

	bool norm = normalize->boolValue();
	std::vector<UVCControlInfo> ctrls = device->getControlsSnapshot();

	for (const auto& c : ctrls)
	{
		if (c.name == "pan-tilt-abs" || c.name == "pan-tilt-rel" || c.name == "zoom-abs" || c.name == "zoom-rel")
			continue;

		if (!c.supportsGet && !c.supportsSet)
			continue;

		if (c.isBool)
		{
			BoolParameter* bp = controlsCC.addBoolParameter(c.name, "UVC control: " + String(c.name), c.currentValue >= 0.5);
			bp->setControllableFeedbackOnly(!c.supportsSet);
			controlParams[c.name] = bp;
		}
		else
		{
			float minV = c.hasRange ? (norm ? 0.0f : (float)c.minValue) : -100000.0f;
			float maxV = c.hasRange ? (norm ? 1.0f : (float)c.maxValue) : 100000.0f;
			if (maxV <= minV) maxV = minV + 1.0f;

			float curV = (float)c.currentValue;
			if (norm && c.hasRange && c.maxValue > c.minValue)
				curV = (float)((c.currentValue - c.minValue) / (c.maxValue - c.minValue));

			FloatParameter* fp = controlsCC.addFloatParameter(c.name, "UVC control: " + String(c.name), jlimit(minV, maxV, curV), minV, maxV);
			fp->setControllableFeedbackOnly(!c.supportsSet);
			controlParams[c.name] = fp;
		}
	}
}

void UVCModule::updateControlRanges()
{
	if (device == nullptr)
		return;

	bool norm = normalize->boolValue();
	std::vector<UVCControlInfo> ctrls = device->getControlsSnapshot();

	isUpdatingFromHardware = true;
	for (const auto& c : ctrls)
	{
		if (c.name == "pan-tilt-abs")
		{
			if (norm)
			{
				panParam->setRange(0.0f, 1.0f);
				tiltParam->setRange(0.0f, 1.0f);
			}
			else if (c.hasRange)
			{
				panParam->setRange((float)c.panMin, (float)jmax(c.panMin + 1.0, c.panMax));
				tiltParam->setRange((float)c.tiltMin, (float)jmax(c.tiltMin + 1.0, c.tiltMax));
			}
			panParam->setControllableFeedbackOnly(!c.supportsSet);
			tiltParam->setControllableFeedbackOnly(!c.supportsSet);
		}
		else if (c.name == "zoom-abs")
		{
			if (norm)
			{
				zoomParam->setRange(0.0f, 1.0f);
			}
			else if (c.hasRange)
			{
				zoomParam->setRange((float)c.minValue, (float)jmax(c.minValue + 1.0, c.maxValue));
			}
			zoomParam->setControllableFeedbackOnly(!c.supportsSet);
		}
		else
		{
			auto it = controlParams.find(c.name);
			if (it != controlParams.end() && !c.isBool)
			{
				if (auto* fp = dynamic_cast<FloatParameter*>(it->second))
				{
					if (norm && c.hasRange)
						fp->setRange(0.0f, 1.0f);
					else if (c.hasRange)
						fp->setRange((float)c.minValue, (float)jmax(c.minValue + 1.0, c.maxValue));
				}
			}
		}
	}
	isUpdatingFromHardware = false;
}

void UVCModule::setPanTilt(double pan, double tilt)
{
	if (device == nullptr || !enabled->boolValue()) return;
	device->setPanTilt(pan, tilt, normalize->boolValue());
	outActivityTrigger->trigger();
}

void UVCModule::setPan(double pan)
{
	setPanTilt(pan, tiltParam->doubleValue());
}

void UVCModule::setTilt(double tilt)
{
	setPanTilt(panParam->doubleValue(), tilt);
}

void UVCModule::setZoom(double zoom)
{
	if (device == nullptr || !enabled->boolValue()) return;
	device->setScalarControl("zoom-abs", zoom, normalize->boolValue());
	outActivityTrigger->trigger();
}

void UVCModule::setControlValue(const String& controlName, double value)
{
	if (device == nullptr || !enabled->boolValue()) return;
	device->setScalarControl(controlName, value, normalize->boolValue());
	outActivityTrigger->trigger();
}

void UVCModule::resetControl(const String& controlName)
{
	if (device == nullptr || !enabled->boolValue()) return;
	if (controlName.isEmpty() || controlName == "all")
		device->resetAllControls();
	else
		device->resetControl(controlName);
	outActivityTrigger->trigger();
}

void UVCModule::uvcTelemetryUpdated(UVCDevice* dev, const UVCTelemetry& telemetry)
{
	if (dev != device || !enabled->boolValue())
		return;

	isUpdatingFromHardware = true;

	if (telemetry.hasPan)
		panParam->setValue((float)telemetry.pan);
	if (telemetry.hasTilt)
		tiltParam->setValue((float)telemetry.tilt);
	if (telemetry.hasZoom)
		zoomParam->setValue((float)telemetry.zoom);

	for (const auto& kv : telemetry.scalarValues)
	{
		auto it = controlParams.find(kv.first);
		if (it != controlParams.end() && it->second != nullptr)
		{
			if (auto* bp = dynamic_cast<BoolParameter*>(it->second))
				bp->setValue(kv.second >= 0.5);
			else
				it->second->setValue((float)kv.second);
		}
	}

	isUpdatingFromHardware = false;
	inActivityTrigger->trigger();
}

void UVCModule::uvcDeviceDisconnected(UVCDevice* dev)
{
	if (dev == device)
	{
		setDevice(nullptr);
	}
}

void UVCModule::uvcDeviceAdded(UVCDevice* dev)
{
	rebuildDeviceOptions();
	if (device == nullptr)
		selectTargetDevice();
}

void UVCModule::uvcDeviceRemoved(UVCDevice* dev)
{
	if (dev == device)
		setDevice(nullptr);
	rebuildDeviceOptions();
	if (device == nullptr)
		selectTargetDevice();
}

void UVCModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	Module::onControllableFeedbackUpdateInternal(cc, c);

	if (isUpdatingFromHardware)
		return;

	if (c == enabled)
	{
		if (device != nullptr)
		{
			if (enabled->boolValue())
				device->startPolling();
			else
				device->stopPolling();
		}
		return;
	}

	if (c == refreshDevices)
	{
		UVCManager::getInstance()->checkDevices();
		rebuildDeviceOptions();
		selectTargetDevice();
		return;
	}

	if (c == cameraTarget)
	{
		selectTargetDevice();
		return;
	}

	if (c == pollRate)
	{
		if (device != nullptr)
			device->setPollRate(pollRate->intValue());
		return;
	}

	if (c == normalize)
	{
		if (device != nullptr)
		{
			device->setNormalize(normalize->boolValue());
			updateControlRanges();
		}
		return;
	}

	if (!enabled->boolValue() || device == nullptr)
		return;

	if (c == resetDefaults)
	{
		device->resetAllControls();
		outActivityTrigger->trigger();
		return;
	}

	if (c == panParam || c == tiltParam)
	{
		setPanTilt(panParam->doubleValue(), tiltParam->doubleValue());
		return;
	}

	if (c == zoomParam)
	{
		setZoom(zoomParam->doubleValue());
		return;
	}

	if (c->parentContainer == &controlsCC)
	{
		if (auto* bp = dynamic_cast<BoolParameter*>(c))
			setControlValue(bp->niceName, bp->boolValue() ? 1.0 : 0.0);
		else if (auto* fp = dynamic_cast<FloatParameter*>(c))
			setControlValue(fp->niceName, fp->doubleValue());
	}
}

void UVCModule::afterLoadJSONDataInternal()
{
	Module::afterLoadJSONDataInternal();
	rebuildDeviceOptions();
	selectTargetDevice();
}
