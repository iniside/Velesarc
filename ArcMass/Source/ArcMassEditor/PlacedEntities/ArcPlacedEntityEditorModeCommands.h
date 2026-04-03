// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ArcPlacedEntityEditorMode"

class FArcPlacedEntityEditorModeCommands : public TCommands<FArcPlacedEntityEditorModeCommands>
{
public:
	FArcPlacedEntityEditorModeCommands()
		: TCommands<FArcPlacedEntityEditorModeCommands>(
			"ArcPlacedEntityEditorMode",
			NSLOCTEXT("Contexts", "ArcPlacedEntityEditor", "Placed Entity Editor"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo> ExitMode;

	virtual void RegisterCommands() override
	{
		UI_COMMAND(ExitMode, "Exit Mode", "Exit the placed entity editing mode", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));
	}
};

#undef LOCTEXT_NAMESPACE
