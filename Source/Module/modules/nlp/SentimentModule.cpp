/*
  ==============================================================================

    SentimentModule.cpp
    Chataigne module for local Apple NaturalLanguage sentiment analysis

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

SentimentModule::SentimentModule(const String& name) :
	Module(name)
{
	setupIOConfiguration(true, false);

	language = moduleParams.addEnumParameter("Language", "Language for sentiment analysis (Auto detects dominant language)");
	language->addOption("Auto", "auto")
		->addOption("English", "en")
		->addOption("Spanish", "es")
		->addOption("French", "fr")
		->addOption("German", "de")
		->addOption("Italian", "it")
		->addOption("Portuguese", "pt")
		->addOption("Japanese", "ja")
		->addOption("Chinese (Simplified)", "zh-Hans");

	positiveThreshold = moduleParams.addFloatParameter("Positive Threshold", "Score >= this threshold is labeled positive", 0.2f, 0.0f, 1.0f);
	negativeThreshold = moduleParams.addFloatParameter("Negative Threshold", "Score <= this threshold is labeled negative", -0.2f, -1.0f, 0.0f);
	inputText = moduleParams.addStringParameter("Input Text", "Text to analyze (trigger Analyze or update directly)", "");
	analyzeTrigger = moduleParams.addTrigger("Analyze", "Analyze the current Input Text");

	sentimentScore = valuesCC.addFloatParameter("Sentiment Score", "Sentiment score from -1.0 (negative) to +1.0 (positive)", 0.0f, -1.0f, 1.0f);
	sentimentScore->setControllableFeedbackOnly(true);
	sentimentLabel = valuesCC.addStringParameter("Sentiment Label", "Classification label (positive, neutral, negative)", "neutral");
	sentimentLabel->setControllableFeedbackOnly(true);
	detectedLanguage = valuesCC.addStringParameter("Detected Language", "Detected or selected BCP-47 language code", "");
	detectedLanguage->setControllableFeedbackOnly(true);
	jsonOutput = valuesCC.addStringParameter("JSON", "JSON payload of the sentiment analysis result", "{}");
	jsonOutput->setControllableFeedbackOnly(true);
	onAnalyzed = valuesCC.addTrigger("On Analyzed", "Fires whenever text is analyzed");

	defManager->add(CommandDefinition::createDef(this, "", "Analyze Text", &SentimentCommand::create));
}

SentimentModule::~SentimentModule()
{
}

void SentimentModule::analyzeText(const String& text)
{
	if (!enabled->boolValue() || text.isEmpty())
		return;

	String langCode = language->getValueData().toString();
	if (langCode.isEmpty()) langCode = "auto";

	SentimentResult res = SentimentBridge::analyze(text.toStdString(), langCode.toStdString());

	String label = "neutral";
	if (res.score >= (double)positiveThreshold->floatValue())
		label = "positive";
	else if (res.score <= (double)negativeThreshold->floatValue())
		label = "negative";

	String langOut = res.language.empty() ? langCode : String(res.language);

	var obj(new DynamicObject());
	obj.getDynamicObject()->setProperty("text", text);
	obj.getDynamicObject()->setProperty("score", res.score);
	obj.getDynamicObject()->setProperty("label", label);
	obj.getDynamicObject()->setProperty("language", langOut);
	obj.getDynamicObject()->setProperty("valid", res.valid);

	sentimentScore->setValue((float)res.score);
	sentimentLabel->setValue(label);
	detectedLanguage->setValue(langOut);
	jsonOutput->setValue(JSON::toString(obj, true));

	onAnalyzed->trigger();
	inActivityTrigger->trigger();
}

void SentimentModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	Module::onControllableFeedbackUpdateInternal(cc, c);

	if (c == analyzeTrigger || c == inputText)
	{
		analyzeText(inputText->stringValue());
	}
}
