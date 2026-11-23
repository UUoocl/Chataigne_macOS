/*
  ==============================================================================

    SentimentModule.h
    Chataigne module for local Apple NaturalLanguage sentiment analysis

  ==============================================================================
*/

#pragma once

class SentimentModule :
	public Module
{
public:
	SentimentModule(const String& name = "Sentiment Analysis");
	~SentimentModule() override;

	EnumParameter* language;
	FloatParameter* positiveThreshold;
	FloatParameter* negativeThreshold;
	StringParameter* inputText;
	Trigger* analyzeTrigger;

	FloatParameter* sentimentScore;
	StringParameter* sentimentLabel;
	StringParameter* detectedLanguage;
	StringParameter* jsonOutput;
	Trigger* onAnalyzed;

	void analyzeText(const String& text);

	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;

	static SentimentModule* create() { return new SentimentModule(); }
	virtual String getDefaultTypeString() const override { return "Sentiment Analysis"; }
};
