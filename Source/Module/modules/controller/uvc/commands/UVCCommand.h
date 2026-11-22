/*
  ==============================================================================

    UVCCommand.h
    Commands for the UVC Camera module

  ==============================================================================
*/

#pragma once

class UVCCommand :
	public BaseCommand
{
public:
	enum UVCAction { SET_PAN_TILT, SET_PAN, SET_TILT, SET_ZOOM, SET_CONTROL, RESET_CONTROL };

	UVCCommand(UVCModule* _module, CommandContext context, var params, Multiplex* multiplex = nullptr);
	~UVCCommand() override;

	UVCAction action;
	UVCModule* uvcModule;

	FloatParameter* panParam = nullptr;
	FloatParameter* tiltParam = nullptr;
	FloatParameter* zoomParam = nullptr;
	StringParameter* controlNameParam = nullptr;
	FloatParameter* controlValueParam = nullptr;

	void triggerInternal(int multiplexIndex) override;

	static BaseCommand* create(ControllableContainer* module, CommandContext context, var params, Multiplex* multiplex)
	{
		return new UVCCommand((UVCModule*)module, context, params, multiplex);
	}
};
