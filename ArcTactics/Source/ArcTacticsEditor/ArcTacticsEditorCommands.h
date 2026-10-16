// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FArcTacticsEditorCommands final : public TCommands<FArcTacticsEditorCommands>
{
public:
	FArcTacticsEditorCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> AddGlobalTask;
	TSharedPtr<FUICommandInfo> AddAction;
	TSharedPtr<FUICommandInfo> AddSelector;
	TSharedPtr<FUICommandInfo> AddStep;
	TSharedPtr<FUICommandInfo> AddActiveStep;
	TSharedPtr<FUICommandInfo> AddConsideration;
	TSharedPtr<FUICommandInfo> Rename;
	TSharedPtr<FUICommandInfo> ReplaceType;
	TSharedPtr<FUICommandInfo> Delete;
	TSharedPtr<FUICommandInfo> Duplicate;
	TSharedPtr<FUICommandInfo> Copy;
	TSharedPtr<FUICommandInfo> Paste;
	TSharedPtr<FUICommandInfo> MoveUp;
	TSharedPtr<FUICommandInfo> MoveDown;
};
