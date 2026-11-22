/*
  ==============================================================================

    UVCCommand.cpp
    Commands for the UVC Camera module

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"

UVCCommand::UVCCommand(UVCModule* _module, CommandContext context, var params, Multiplex* multiplex) :
	BaseCommand(_module, context, params, multiplex),
	uvcModule(_module)
{
	action = (UVCAction)(int)params.getProperty("action", SET_PAN_TILT);

	switch (action)
	{
	case SET_PAN_TILT:
		panParam = addFloatParameter("Pan", "Target Pan value", 0.0f, -648000.0f, 648000.0f);
		tiltParam = addFloatParameter("Tilt", "Target Tilt value", 0.0f, -648000.0f, 648000.0f);
		linkParamToMappingIndex(panParam, 0);
		linkParamToMappingIndex(tiltParam, 1);
		break;

	case SET_PAN:
		panParam = addFloatParameter("Pan", "Target Pan value", 0.0f, -648000.0f, 648000.0f);
		linkParamToMappingIndex(panParam, 0);
		break;

	case SET_TILT:
		tiltParam = addFloatParameter("Tilt", "Target Tilt value", 0.0f, -648000.0f, 648000.0f);
		linkParamToMappingIndex(tiltParam, 0);
		break;

	case SET_ZOOM:
		zoomParam = addFloatParameter("Zoom", "Target Zoom value", 0.0f, 0.0f, 1000.0f);
		linkParamToMappingIndex(zoomParam, 0);
		break;

	case SET_CONTROL:
		controlNameParam = addStringParameter("Control", "UVC control name (e.g. focus-abs, brightness, exposure-time-abs)", "focus-abs");
		controlValueParam = addFloatParameter("Value", "Target control value", 0.0f, -1000000.0f, 1000000.0f);
		linkParamToMappingIndex(controlValueParam, 0);
		break;

	case RESET_CONTROL:
		controlNameParam = addStringParameter("Control", "UVC control name to reset, or 'all'", "all");
		break;
	}
}

UVCCommand::~UVCCommand()
{
}

void UVCCommand::triggerInternal(int multiplexIndex)
{
	if (uvcModule == nullptr) return;

	switch (action)
	{
	case SET_PAN_TILT:
		uvcModule->setPanTilt((double)getLinkedValue(panParam, multiplexIndex), (double)getLinkedValue(tiltParam, multiplexIndex));
		break;

	case SET_PAN:
		uvcModule->setPan((double)getLinkedValue(panParam, multiplexIndex));
		break;

	case SET_TILT:
		uvcModule->setTilt((double)getLinkedValue(tiltParam, multiplexIndex));
		break;

	case SET_ZOOM:
		uvcModule->setZoom((double)getLinkedValue(zoomParam, multiplexIndex));
		break;

	case SET_CONTROL:
		uvcModule->setControlValue(getLinkedValue(controlNameParam, multiplexIndex).toString(), (double)getLinkedValue(controlValueParam, multiplexIndex));
		break;

	case RESET_CONTROL:
		uvcModule->resetControl(getLinkedValue(controlNameParam, multiplexIndex).toString());
		break;
	}
}
