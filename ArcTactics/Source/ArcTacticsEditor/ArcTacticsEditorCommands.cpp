// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsEditorCommands.h"

#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ArcTacticsEditorCommands"

FArcTacticsEditorCommands::FArcTacticsEditorCommands()
	: TCommands<FArcTacticsEditorCommands>(
		TEXT("ArcTacticsEditor"),
		LOCTEXT("ContextName", "Arc Tactics Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FArcTacticsEditorCommands::RegisterCommands()
{
	UI_COMMAND(AddGlobalTask, "Add Global Task", "Add a global task.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddAction, "Add Action", "Add a standalone action.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddSelector, "Add Selector", "Add a selector.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddStep, "Add Step", "Add a sequence step.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddActiveStep, "Add While Active Step", "Add a step that stays active while the action runs.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddConsideration, "Add Consideration", "Add a consideration.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Rename, "Rename", "Rename the selected item.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));
	UI_COMMAND(ReplaceType, "Replace Type", "Replace the selected node type.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Delete, "Delete", "Delete the selected item.", EUserInterfaceActionType::Button, FInputChord(EKeys::Platform_Delete));
	UI_COMMAND(Duplicate, "Duplicate", "Duplicate the selected item and its bindings.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::D));
	UI_COMMAND(Copy, "Copy", "Copy the selected item and its bindings.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(Paste, "Paste", "Paste Arc Tactics authoring data.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::V));
	UI_COMMAND(MoveUp, "Move Up", "Move the selected item before its previous sibling.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Up));
	UI_COMMAND(MoveDown, "Move Down", "Move the selected item after its next sibling.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Down));
}

#undef LOCTEXT_NAMESPACE
