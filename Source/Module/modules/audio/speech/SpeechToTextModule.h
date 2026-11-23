/*
  ==============================================================================

    SpeechToTextModule.h
    Chataigne module for local on-device Speech-to-Text on macOS

  ==============================================================================
*/

#pragma once

class SpeechToTextModule :
	public Module
{
public:
	enum OutputMode { FULL_TRANSCRIPT, NEW_WORDS_ONLY, CHUNK };

	SpeechToTextModule(const String& name = "Speech to Text");
	~SpeechToTextModule() override;

	BoolParameter* active;
	BoolParameter* continuousCapture;
	BoolParameter* isRunning;
	StringParameter* status;
	EnumParameter* locale;
	EnumParameter* audioDevice;
	EnumParameter* outputMode;
	FloatParameter* silenceDuration;
	Trigger* resetText;

	StringParameter* transcribedText;
	BoolParameter* isFinal;
	Trigger* wordReceived;

	SpeechBridge::Handle engineHandle = nullptr;
	String lastEmittedText;

	void rebuildAudioDevices(const std::vector<SpeechAudioDevice>& devices);
	void startRecognition();
	void stopRecognition();
	void resetRecognition();
	void setLocaleByCode(const String& localeCode);

	void handleTranscription(const String& text, bool finalSegment);
	void handleStatus(const String& newStatus, bool running);

	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;
	void afterLoadJSONDataInternal() override;

	static SpeechToTextModule* create() { return new SpeechToTextModule(); }
	virtual String getDefaultTypeString() const override { return "Speech to Text"; }

	WeakReference<SpeechToTextModule>::Master masterReference;
};
