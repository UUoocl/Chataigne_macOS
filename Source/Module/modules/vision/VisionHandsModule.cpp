/*
  ==============================================================================

    VisionHandsModule.cpp
    Apple Vision Human Hand Pose module

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

VisionHandsModule::VisionHandsModule(const String& name) :
	VisionBaseModule(name, "Detected Hands")
{
	maxHandCount = moduleParams.addIntParameter("Max Hand Count", "Maximum number of hands to detect", 2, 1, 4);
}

VisionConfig VisionHandsModule::buildVisionConfig() const
{
	VisionConfig cfg;
	fillBaseConfig(cfg);
	cfg.task = VisionTaskType::Hands;
	cfg.maxHandCount = maxHandCount->intValue();
	return cfg;
}

void VisionHandsModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	VisionBaseModule::onControllableFeedbackUpdateInternal(cc, c);
	if (c == maxHandCount && engineHandle != nullptr && active->boolValue() && enabled->boolValue())
		VisionBridge::updateConfig(engineHandle, buildVisionConfig());
}
