/*
  ==============================================================================

    VisionFaceModule.h
    Apple Vision Face & Landmarks module

  ==============================================================================
*/

#pragma once

class VisionFaceModule :
	public VisionBaseModule
{
public:
	VisionFaceModule(const String& name = "Vision Face");
	~VisionFaceModule() override = default;

	BoolParameter* includeLandmarks;

	VisionConfig buildVisionConfig() const override;
	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;

	static VisionFaceModule* create() { return new VisionFaceModule(); }
	virtual String getDefaultTypeString() const override { return "Vision Face"; }
};
