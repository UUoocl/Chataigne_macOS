/*
  ==============================================================================

    SentimentCommand.h
    Command for the Sentiment Analysis module

  ==============================================================================
*/

#pragma once

class SentimentCommand :
	public BaseCommand
{
public:
	SentimentCommand(SentimentModule* _module, CommandContext context, var params, Multiplex* multiplex = nullptr);
	~SentimentCommand() override;

	SentimentModule* sentimentModule;
	StringParameter* textParam = nullptr;

	void triggerInternal(int multiplexIndex) override;

	static BaseCommand* create(ControllableContainer* module, CommandContext context, var params, Multiplex* multiplex)
	{
		return new SentimentCommand((SentimentModule*)module, context, params, multiplex);
	}
};
