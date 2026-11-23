/*
  ==============================================================================

    VisionHandsModule.h
    Apple Vision Human Hand Pose module

  ==============================================================================
*/

#pragma once

class VisionHandsModule :
	public VisionBaseModule
{
public:
	VisionHandsModule(const String& name = "Vision Hands");
	~VisionHandsModule() override = default;

	IntParameter* maxHandCount;

	VisionConfig buildVisionConfig() const override;
	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;

	static VisionHandsModule* create() { return new VisionHandsModule(); }
	virtual String getDefaultTypeString() const override { return "Vision Hands"; }
};
