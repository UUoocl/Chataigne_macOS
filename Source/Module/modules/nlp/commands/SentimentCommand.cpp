/*
  ==============================================================================

    SentimentCommand.cpp
    Command for the Sentiment Analysis module

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

SentimentCommand::SentimentCommand(SentimentModule* _module, CommandContext context, var params, Multiplex* multiplex) :
	BaseCommand(_module, context, params, multiplex),
	sentimentModule(_module)
{
	textParam = addStringParameter("Text", "Text to analyze for sentiment", "");
	linkParamToMappingIndex(textParam, 0);
}

SentimentCommand::~SentimentCommand()
{
}

void SentimentCommand::triggerInternal(int multiplexIndex)
{
	if (sentimentModule == nullptr || textParam == nullptr) return;
	sentimentModule->analyzeText(getLinkedValue(textParam, multiplexIndex).toString());
}
