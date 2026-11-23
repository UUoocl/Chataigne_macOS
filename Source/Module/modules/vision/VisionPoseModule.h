/*
  ==============================================================================

    VisionPoseModule.h
    Apple Vision Human Body Pose (2D / 3D) module

  ==============================================================================
*/

#pragma once

class VisionPoseModule :
	public VisionBaseModule
{
public:
	VisionPoseModule(const String& name = "Vision Pose");
	~VisionPoseModule() override = default;

	EnumParameter* mode;

	VisionConfig buildVisionConfig() const override;
	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;

	static VisionPoseModule* create() { return new VisionPoseModule(); }
	virtual String getDefaultTypeString() const override { return "Vision Pose"; }
};
