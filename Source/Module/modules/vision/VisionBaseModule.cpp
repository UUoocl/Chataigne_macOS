/*
  ==============================================================================

    VisionBaseModule.cpp
    Shared base class for Apple Vision modules (Pose, Face, Hands)

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

VisionBaseModule::VisionBaseModule(const String& name, const String& detectedCountName) :
	Module(name),
	roiCC("Region of Interest")
{
	setupIOConfiguration(true, false);

	active = moduleParams.addBoolParameter("Active", "Start or stop local Vision camera processing", false);
	isRunning = moduleParams.addBoolParameter("Is Running", "Is Vision capture currently running?", false);
	isRunning->setControllableFeedbackOnly(true);
	connectionFeedbackRef = isRunning;

	status = moduleParams.addStringParameter("Status", "Current Vision status", "Stopped");
	status->setControllableFeedbackOnly(true);

	cameraDevice = moduleParams.addEnumParameter("Camera Device", "macOS video capture device");
	minConfidence = moduleParams.addFloatParameter("Min Confidence", "Minimum detection confidence threshold", 0.1f, 0.0f, 1.0f);
	targetFps = moduleParams.addIntParameter("Target FPS", "Maximum Vision inference rate (frames per second)", 30, 1, 60);

	roiCC.editorIsCollapsed = true;
	moduleParams.addChildControllableContainer(&roiCC);
	roiX = roiCC.addFloatParameter("ROI X", "Normalized ROI X (0..1)", 0.0f, 0.0f, 1.0f);
	roiY = roiCC.addFloatParameter("ROI Y", "Normalized ROI Y (0..1)", 0.0f, 0.0f, 1.0f);
	roiWidth = roiCC.addFloatParameter("ROI Width", "Normalized ROI Width (0..1)", 1.0f, 0.01f, 1.0f);
	roiHeight = roiCC.addFloatParameter("ROI Height", "Normalized ROI Height (0..1)", 1.0f, 0.01f, 1.0f);

	detectedCount = valuesCC.addIntParameter(detectedCountName, "Number of detected items in the latest frame", 0, 0, 100);
	detectedCount->setControllableFeedbackOnly(true);
	jsonOutput = valuesCC.addStringParameter("JSON", "JSON payload of detected Vision results", "{}");
	jsonOutput->setControllableFeedbackOnly(true);
	onDetected = valuesCC.addTrigger("On Detected", "Fires when a frame produces Vision results");

	rebuildCameras(VisionBridge::getCameraDevices());

	WeakReference<VisionBaseModule> weakThis(this);
	VisionCallbacks callbacks;
	callbacks.onResult = [weakThis](const std::string& json, int count)
	{
		String s(json);
		MessageManager::callAsync([weakThis, s, count]()
		{
			if (weakThis != nullptr)
				weakThis->handleResult(s, count);
		});
	};
	callbacks.onStatus = [weakThis](const std::string& st, bool running)
	{
		String s(st);
		MessageManager::callAsync([weakThis, s, running]()
		{
			if (weakThis != nullptr)
				weakThis->handleStatus(s, running);
		});
	};
	callbacks.onDevicesChanged = [weakThis](const std::vector<VisionCameraDevice>& devs)
	{
		MessageManager::callAsync([weakThis, devs]()
		{
			if (weakThis != nullptr)
				weakThis->rebuildCameras(devs);
		});
	};

	engineHandle = VisionBridge::create(callbacks);
}

VisionBaseModule::~VisionBaseModule()
{
	masterReference.clear();
	if (engineHandle != nullptr)
	{
		VisionBridge::destroy(engineHandle);
		engineHandle = nullptr;
	}
}

void VisionBaseModule::fillBaseConfig(VisionConfig& cfg) const
{
	String devId = cameraDevice->getValueData().toString();
	cfg.deviceId = devId.isEmpty() ? "Default" : devId.toStdString();
	cfg.minConfidence = minConfidence->floatValue();
	cfg.targetFps = targetFps->intValue();
	cfg.roiX = roiX->floatValue();
	cfg.roiY = roiY->floatValue();
	cfg.roiWidth = roiWidth->floatValue();
	cfg.roiHeight = roiHeight->floatValue();
}

void VisionBaseModule::rebuildCameras(const std::vector<VisionCameraDevice>& devices)
{
	var prev = cameraDevice->getValueData();
	cameraDevice->clearOptions();

	for (const auto& d : devices)
		cameraDevice->addOption(String(d.name), var(String(d.uniqueId)));

	if (!prev.isVoid() && prev.toString().isNotEmpty())
		cameraDevice->setValueWithData(prev);
	else
		cameraDevice->setValueWithData(var("Default"));
}

void VisionBaseModule::startVision()
{
	if (engineHandle == nullptr || !enabled->boolValue())
		return;
	VisionBridge::start(engineHandle, buildVisionConfig());
}

void VisionBaseModule::stopVision()
{
	if (engineHandle == nullptr)
		return;
	VisionBridge::stop(engineHandle);
}

void VisionBaseModule::handleResult(const String& json, int count)
{
	if (!enabled->boolValue())
		return;

	detectedCount->setValue(count);
	jsonOutput->setValue(json);
	if (count > 0)
		onDetected->trigger();
	inActivityTrigger->trigger();
}

void VisionBaseModule::handleStatus(const String& newStatus, bool running)
{
	status->setValue(newStatus);
	isRunning->setValue(running);
	if (!running && active->boolValue() && (newStatus.containsIgnoreCase("Denied") || newStatus.containsIgnoreCase("Failed") || newStatus.containsIgnoreCase("Missing") || newStatus.containsIgnoreCase("No Camera")))
		active->setValue(false);
}

void VisionBaseModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	Module::onControllableFeedbackUpdateInternal(cc, c);

	if (c == enabled)
	{
		if (!enabled->boolValue())
			stopVision();
		else if (active->boolValue())
			startVision();
		return;
	}

	if (c == active)
	{
		if (active->boolValue() && enabled->boolValue())
			startVision();
		else
			stopVision();
		return;
	}

	if (engineHandle != nullptr && active->boolValue() && enabled->boolValue())
	{
		if (c == cameraDevice || c == minConfidence || c == targetFps || c == roiX || c == roiY || c == roiWidth || c == roiHeight)
			VisionBridge::updateConfig(engineHandle, buildVisionConfig());
	}
}

void VisionBaseModule::afterLoadJSONDataInternal()
{
	Module::afterLoadJSONDataInternal();
	if (active->boolValue() && enabled->boolValue())
		startVision();
}
