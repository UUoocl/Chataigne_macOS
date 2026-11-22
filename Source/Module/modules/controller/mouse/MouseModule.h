/*
  ==============================================================================

    MouseModule.h
    Created: 12 Mar 2020 3:08:47pm
    Author:  bkupe

  ==============================================================================
*/

#pragma once

#if JUCE_MAC
namespace mousemac { class MacMouseButtonTap; }
#endif

class MouseModule :
	public Module,
#if JUCE_WINDOWS
	public MouseHooker::Listener,
#else
	public MouseListener,
#endif
	public Timer
{
public:
	MouseModule();
	~MouseModule();

	void clearItem() override;

	MouseInputSource* source;

	IntParameter* updateRate;

	IntParameter* currentScreen;
	Point2DParameter* absolutePosition;
	Point2DParameter* relativePosition;
	BoolParameter* leftButtonDown;
	BoolParameter* middleButtonDown;
	BoolParameter* rightButtonDown;
	FloatParameter* wheelYDelta;
	Trigger* wheelYData;
	FloatParameter* wheelXDelta;
	Trigger* wheelXData;
	BoolParameter* extraButton1;
	BoolParameter* extraButton2;
#if JUCE_MAC
	ControllableContainer macExtraButtonsCC;
	Array<BoolParameter*> macExtraButtons;
#endif


	void setCursorPosition(Point<float>& pos, bool isRelative);
	void setWheelData(float wheelDelta, int orientation);
	void setButtonDown(int buttonID);
	void setButtonUp(int buttonID);
	void setButtonClick(int buttonID);

	void sendButtonEvent(int buttonEvent, int buttonNumber = -1);


#if JUCE_WINDOWS
	void mouseButtonChanged(int button, bool pressed);
	void mouseWheelChanged(int wheelDelta, bool horizontal) override;
#else
#if JUCE_MAC
	void macMouseButtonChanged(int buttonNumber, bool pressed);
	std::unique_ptr<mousemac::MacMouseButtonTap> macButtonTap;
	bool macButtonTapActive = false;
#endif
	void mouseDown(const MouseEvent& e) override;
	void mouseUp(const MouseEvent& e) override;
	void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& d) override;
#endif



	void onContainerParameterChangedInternal(Parameter* p) override;

	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;
	
	void updateMouseInfos();

	void timerCallback() override;

	String getTypeString() const override { return getTypeStringStatic(); }
	const static String getTypeStringStatic() { return "Mouse"; }

	static MouseModule* create() { return new MouseModule(); }

	JUCE_DECLARE_WEAK_REFERENCEABLE(MouseModule)
};
