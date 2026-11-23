/*
  ==============================================================================

    SpeechToTextCommand.h
    Commands for the Speech to Text module

  ==============================================================================
*/

#pragma once

class SpeechToTextCommand :
	public BaseCommand
{
public:
	enum SpeechAction { START, STOP, RESET, SET_LOCALE };

	SpeechToTextCommand(SpeechToTextModule* _module, CommandContext context, var params, Multiplex* multiplex = nullptr);
	~SpeechToTextCommand() override;

	SpeechAction action;
	SpeechToTextModule* speechModule;

	StringParameter* localeParam = nullptr;

	void triggerInternal(int multiplexIndex) override;

	static BaseCommand* create(ControllableContainer* module, CommandContext context, var params, Multiplex* multiplex)
	{
		return new SpeechToTextCommand((SpeechToTextModule*)module, context, params, multiplex);
	}
};
