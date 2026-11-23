/*
  ==============================================================================

    SpeechToTextCommand.cpp
    Commands for the Speech to Text module

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

SpeechToTextCommand::SpeechToTextCommand(SpeechToTextModule* _module, CommandContext context, var params, Multiplex* multiplex) :
	BaseCommand(_module, context, params, multiplex),
	speechModule(_module)
{
	action = (SpeechAction)(int)params.getProperty("action", START);

	if (action == SET_LOCALE)
	{
		localeParam = addStringParameter("Locale", "BCP-47 locale identifier (e.g. en-US, es-ES, fr-FR)", "en-US");
		linkParamToMappingIndex(localeParam, 0);
	}
}

SpeechToTextCommand::~SpeechToTextCommand()
{
}

void SpeechToTextCommand::triggerInternal(int multiplexIndex)
{
	if (speechModule == nullptr) return;

	switch (action)
	{
	case START:
		speechModule->active->setValue(true);
		break;

	case STOP:
		speechModule->active->setValue(false);
		break;

	case RESET:
		speechModule->resetRecognition();
		break;

	case SET_LOCALE:
		if (localeParam != nullptr)
			speechModule->setLocaleByCode(getLinkedValue(localeParam, multiplexIndex).toString());
		break;
	}
}
