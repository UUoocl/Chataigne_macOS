/*
  ==============================================================================

    SpeechToTextModule.cpp
    Chataigne module for local on-device Speech-to-Text on macOS

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

SpeechToTextModule::SpeechToTextModule(const String& name) :
	Module(name)
{
	setupIOConfiguration(true, false);

	active = moduleParams.addBoolParameter("Active", "Start or stop live speech recognition", false);
	continuousCapture = moduleParams.addBoolParameter("Continuous Capture", "Keep listening continuously across finalized speech segments (when off, stops after one finalized segment)", true);
	isRunning = moduleParams.addBoolParameter("Is Running", "Is speech recognition currently active?", false);
	isRunning->setControllableFeedbackOnly(true);
	connectionFeedbackRef = isRunning;

	status = moduleParams.addStringParameter("Status", "Current recognizer status", "Stopped");
	status->setControllableFeedbackOnly(true);

	locale = moduleParams.addEnumParameter("Locale", "Speech recognition language locale");
	locale->addOption("English (US)", "en-US")
		->addOption("English (UK)", "en-GB")
		->addOption("Spanish (ES)", "es-ES")
		->addOption("French (FR)", "fr-FR")
		->addOption("German (DE)", "de-DE")
		->addOption("Italian (IT)", "it-IT")
		->addOption("Japanese (JP)", "ja-JP")
		->addOption("Chinese (CN)", "zh-CN")
		->addOption("Portuguese (BR)", "pt-BR");

	audioDevice = moduleParams.addEnumParameter("Audio Input Device", "CoreAudio microphone input device");
	outputMode = moduleParams.addEnumParameter("Output Mode", "How recognized text is emitted");
	outputMode->addOption("Full Transcript", FULL_TRANSCRIPT)
		->addOption("New Words Only", NEW_WORDS_ONLY)
		->addOption("Chunk", CHUNK);

	silenceDuration = moduleParams.addFloatParameter("Silence Duration", "Seconds of silence before finalizing and resetting a segment", 1.5f, 0.2f, 10.0f);
	resetText = moduleParams.addTrigger("Reset Text", "Clear transcribed text and restart recognition segment");

	transcribedText = valuesCC.addStringParameter("Transcribed Text", "Recognized speech text", "");
	transcribedText->setControllableFeedbackOnly(true);
	isFinal = valuesCC.addBoolParameter("Is Final", "True when the current segment is finalized", false);
	isFinal->setControllableFeedbackOnly(true);
	wordReceived = valuesCC.addTrigger("Word Received", "Fires when new transcription text is emitted");

	defManager->add(CommandDefinition::createDef(this, "", "Start", &SpeechToTextCommand::create)->addParam("action", SpeechToTextCommand::START));
	defManager->add(CommandDefinition::createDef(this, "", "Stop", &SpeechToTextCommand::create)->addParam("action", SpeechToTextCommand::STOP));
	defManager->add(CommandDefinition::createDef(this, "", "Reset Text", &SpeechToTextCommand::create)->addParam("action", SpeechToTextCommand::RESET));
	defManager->add(CommandDefinition::createDef(this, "", "Set Locale", &SpeechToTextCommand::create)->addParam("action", SpeechToTextCommand::SET_LOCALE));

	rebuildAudioDevices(SpeechBridge::getInputDevices());

	WeakReference<SpeechToTextModule> weakThis(this);
	SpeechCallbacks callbacks;
	callbacks.onTranscription = [weakThis](const std::string& text, bool finalFlag)
	{
		String s(text);
		MessageManager::callAsync([weakThis, s, finalFlag]()
		{
			if (weakThis != nullptr)
				weakThis->handleTranscription(s, finalFlag);
		});
	};
	callbacks.onStatus = [weakThis](const std::string& st, bool running)
	{
		String s(st);
		MessageManager::callAsync([weakThis, s, running]()
		{
			if (weakThis != nullptr)
				weakThis->handleStatus(s, running);
		});
	};
	callbacks.onDevicesChanged = [weakThis](const std::vector<SpeechAudioDevice>& devs)
	{
		MessageManager::callAsync([weakThis, devs]()
		{
			if (weakThis != nullptr)
				weakThis->rebuildAudioDevices(devs);
		});
	};

	engineHandle = SpeechBridge::create(callbacks);
}

SpeechToTextModule::~SpeechToTextModule()
{
	masterReference.clear();
	if (engineHandle != nullptr)
	{
		SpeechBridge::destroy(engineHandle);
		engineHandle = nullptr;
	}
}

void SpeechToTextModule::rebuildAudioDevices(const std::vector<SpeechAudioDevice>& devices)
{
	var prevData = audioDevice->getValueData();
	audioDevice->clearOptions();

	for (const auto& d : devices)
		audioDevice->addOption(String(d.name), var(String(d.uid)));

	if (!prevData.isVoid() && prevData.toString().isNotEmpty())
		audioDevice->setValueWithData(prevData);
	else
		audioDevice->setValueWithData(var("Default"));
}

void SpeechToTextModule::startRecognition()
{
	if (engineHandle == nullptr || !enabled->boolValue())
		return;

	String loc = locale->getValueData().toString();
	if (loc.isEmpty()) loc = "en-US";

	String devUid = audioDevice->getValueData().toString();
	if (devUid.isEmpty()) devUid = "Default";

	SpeechBridge::start(engineHandle, loc.toStdString(), devUid.toStdString(), (double)silenceDuration->floatValue(), continuousCapture->boolValue());
}

void SpeechToTextModule::stopRecognition()
{
	if (engineHandle == nullptr)
		return;

	SpeechBridge::stop(engineHandle);
}

void SpeechToTextModule::resetRecognition()
{
	if (engineHandle != nullptr)
		SpeechBridge::reset(engineHandle);
	transcribedText->setValue("");
	lastEmittedText.clear();
}

void SpeechToTextModule::setLocaleByCode(const String& localeCode)
{
	if (localeCode.isEmpty()) return;
	locale->setValueWithData(var(localeCode));
	if (engineHandle != nullptr && active->boolValue() && enabled->boolValue())
		SpeechBridge::setLocale(engineHandle, localeCode.toStdString());
}

void SpeechToTextModule::handleTranscription(const String& text, bool finalSegment)
{
	if (!enabled->boolValue())
		return;

	isFinal->setValue(finalSegment);

	if (finalSegment)
		lastEmittedText.clear();

	OutputMode mode = outputMode->getValueDataAsEnum<OutputMode>();
	if (mode == NEW_WORDS_ONLY)
	{
		StringArray prevWords = StringArray::fromTokens(lastEmittedText.trim(), false);
		StringArray currWords = StringArray::fromTokens(text.trim(), false);
		prevWords.removeEmptyStrings();
		currWords.removeEmptyStrings();

		bool isNewSentence = prevWords.isEmpty()
			|| currWords.isEmpty()
			|| !currWords[0].equalsIgnoreCase(prevWords[0]);

		if (isNewSentence)
		{
			if (currWords.size() <= 5)
			{
				String delta = currWords.joinIntoString(" ");
				if (delta.isNotEmpty())
				{
					transcribedText->setValue(delta);
					wordReceived->trigger();
					inActivityTrigger->trigger();
				}
			}
		}
		else if (currWords.size() > prevWords.size())
		{
			StringArray deltaWords;
			for (int i = prevWords.size(); i < currWords.size(); ++i)
				deltaWords.add(currWords[i]);

			String delta = deltaWords.joinIntoString(" ");
			if (delta.isNotEmpty())
			{
				transcribedText->setValue(delta);
				wordReceived->trigger();
				inActivityTrigger->trigger();
			}
		}

		lastEmittedText = text;
	}
	else if (mode == CHUNK)
	{
		if (finalSegment)
		{
			transcribedText->setValue(text);
			wordReceived->trigger();
			inActivityTrigger->trigger();
		}
	}
	else
	{
		transcribedText->setValue(text);
		wordReceived->trigger();
		inActivityTrigger->trigger();
	}
}

void SpeechToTextModule::handleStatus(const String& newStatus, bool running)
{
	status->setValue(newStatus);
	isRunning->setValue(running);
	if (!running && active->boolValue() && (newStatus == "Stopped" || newStatus.containsIgnoreCase("Denied") || newStatus.containsIgnoreCase("Failed") || newStatus.containsIgnoreCase("Missing") || newStatus.containsIgnoreCase("Not Available")))
		active->setValue(false);
}

void SpeechToTextModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	Module::onControllableFeedbackUpdateInternal(cc, c);

	if (c == enabled)
	{
		if (!enabled->boolValue())
			stopRecognition();
		else if (active->boolValue())
			startRecognition();
		return;
	}

	if (c == active)
	{
		if (active->boolValue() && enabled->boolValue())
			startRecognition();
		else
			stopRecognition();
		return;
	}

	if (c == resetText)
	{
		resetRecognition();
		return;
	}

	if (engineHandle == nullptr || !active->boolValue() || !enabled->boolValue())
		return;

	if (c == locale)
	{
		String loc = locale->getValueData().toString();
		if (loc.isNotEmpty())
			SpeechBridge::setLocale(engineHandle, loc.toStdString());
	}
	else if (c == audioDevice)
	{
		String devUid = audioDevice->getValueData().toString();
		if (devUid.isNotEmpty())
			SpeechBridge::setAudioDevice(engineHandle, devUid.toStdString());
	}
	else if (c == silenceDuration)
	{
		SpeechBridge::setSilenceDuration(engineHandle, (double)silenceDuration->floatValue());
	}
	else if (c == continuousCapture)
	{
		SpeechBridge::setContinuous(engineHandle, continuousCapture->boolValue());
	}
}

void SpeechToTextModule::afterLoadJSONDataInternal()
{
	Module::afterLoadJSONDataInternal();
	if (active->boolValue() && enabled->boolValue())
		startRecognition();
}
