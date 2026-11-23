/*
  ==============================================================================

    VisionFaceModule.cpp
    Apple Vision Face & Landmarks module

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

VisionFaceModule::VisionFaceModule(const String& name) :
	VisionBaseModule(name, "Detected Faces")
{
	includeLandmarks = moduleParams.addBoolParameter("Include Landmarks", "Extract detailed facial landmarks (eyes, nose, lips, pupils)", true);
}

VisionConfig VisionFaceModule::buildVisionConfig() const
{
	VisionConfig cfg;
	fillBaseConfig(cfg);
	cfg.task = VisionTaskType::Face;
	cfg.includeLandmarks = includeLandmarks->boolValue();
	return cfg;
}

void VisionFaceModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	VisionBaseModule::onControllableFeedbackUpdateInternal(cc, c);
	if (c == includeLandmarks && engineHandle != nullptr && active->boolValue() && enabled->boolValue())
		VisionBridge::updateConfig(engineHandle, buildVisionConfig());
}
