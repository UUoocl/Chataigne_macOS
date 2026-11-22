/*
  ==============================================================================

    KeyboardModule.h
    Created: 15 Mar 2018 9:36:44am
    Author:  Ben

  ==============================================================================
*/

#pragma once

using namespace juce;

#if JUCE_MAC
namespace keyboardmac { struct KeyEvent; class MacKeyboardTap; }
#endif

class KeyboardModule :
	public Module
#if JUCE_WINDOWS
	,public KeyboardHooker::Listener
#else
	,public KeyListener
#endif
{
public:
	KeyboardModule();
	~KeyboardModule();

	Component::SafePointer<TopLevelWindow> window;

	StringParameter* lastKey;
	IntParameter* lastKeyCode;
	StringParameter * combination;
	BoolParameter * ctrl;
	BoolParameter * shift;
	BoolParameter * command;
	BoolParameter * alt;

	ControllableContainer keysCC;
	HashMap<int, BoolParameter*> keyMap;

	void sendKeyDown(int keyID);
	void sendKeyUp(int keyID);
	void sendKeyHit(int keyID, bool ctrlPressed, bool altPressed, bool shiftPressed);


#if JUCE_WINDOWS
	void keyChanged(int keyCode, bool pressed);

#else
#if JUCE_MAC
	void macKeyChanged(const keyboardmac::KeyEvent& ev);
	std::unique_ptr<keyboardmac::MacKeyboardTap> macKeyboardTap;
	bool macKeyboardTapActive = false;
#endif
	virtual bool keyPressed(const KeyPress& key, Component* originatingComponent) override;
	virtual bool keyStateChanged(bool isKeyDown, Component* originatingComponent) override;
#endif

	static KeyboardModule * create() { return new KeyboardModule(); }
	virtual String getDefaultTypeString() const override { return "Keyboard"; }

	JUCE_DECLARE_WEAK_REFERENCEABLE(KeyboardModule)
};
