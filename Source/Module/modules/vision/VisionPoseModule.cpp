/*
  ==============================================================================

    VisionPoseModule.cpp
    Apple Vision Human Body Pose (2D / 3D) module

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

VisionPoseModule::VisionPoseModule(const String& name) :
	VisionBaseModule(name, "Detected Poses")
{
	mode = moduleParams.addEnumParameter("Mode", "2D Body Pose (macOS 11+) or 3D Body Pose (macOS 14+)");
	mode->addOption("2D", (int)VisionTaskType::Pose2D)
		->addOption("3D", (int)VisionTaskType::Pose3D);
}

VisionConfig VisionPoseModule::buildVisionConfig() const
{
	VisionConfig cfg;
	fillBaseConfig(cfg);
	cfg.task = mode->getValueDataAsEnum<VisionTaskType>();
	return cfg;
}

void VisionPoseModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	VisionBaseModule::onControllableFeedbackUpdateInternal(cc, c);
	if (c == mode && engineHandle != nullptr && active->boolValue() && enabled->boolValue())
		VisionBridge::updateConfig(engineHandle, buildVisionConfig());
}
