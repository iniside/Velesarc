// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityEditorMode.h"

#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LevelEditor.h"
#include "PlacedEntities/ArcPlacedEntityEditorModeCommands.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityEditorMode)

const FEditorModeID UArcPlacedEntityEditorMode::ModeId = FEditorModeID("EM_ArcPlacedEntity");

UArcPlacedEntityEditorMode::UArcPlacedEntityEditorMode()
{
	Info = FEditorModeInfo(
		ModeId,
		NSLOCTEXT("ArcPlacedEntityEditorMode", "ModeName", "Placed Entity Editor"),
		FSlateIcon(),
		false
	);
}

void UArcPlacedEntityEditorMode::Enter()
{
	Super::Enter();
}

void UArcPlacedEntityEditorMode::Exit()
{
	EditTarget.Reset();
	Super::Exit();
}

bool UArcPlacedEntityEditorMode::IsSelectionDisallowed(AActor* InActor, bool bInSelection) const
{
	if (!EditTarget.IsValid())
	{
		return false;
	}

	return InActor != EditTarget.Get();
}

bool UArcPlacedEntityEditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return OtherModeID == FBuiltinEditorModes::EM_Default;
}

void UArcPlacedEntityEditorMode::BindCommands()
{
	UEdMode::BindCommands();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
	const FArcPlacedEntityEditorModeCommands& Commands = FArcPlacedEntityEditorModeCommands::Get();

	CommandList->MapAction(
		Commands.ExitMode,
		FExecuteAction::CreateUObject(this, &UArcPlacedEntityEditorMode::ExitModeCommand));
}

void UArcPlacedEntityEditorMode::ExitModeCommand()
{
	ExitEditMode();
}

void UArcPlacedEntityEditorMode::SetEditTarget(AActor* InTarget)
{
	EditTarget = InTarget;
}

AActor* UArcPlacedEntityEditorMode::GetEditTarget() const
{
	return EditTarget.Get();
}

void UArcPlacedEntityEditorMode::EnterEditMode(AActor* InTarget)
{
	if (!InTarget)
	{
		return;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (!LevelEditor.IsValid())
	{
		return;
	}

	FEditorModeTools& ModeManager = LevelEditor->GetEditorModeManager();
	ModeManager.ActivateMode(ModeId);

	UArcPlacedEntityEditorMode* Mode = Cast<UArcPlacedEntityEditorMode>(ModeManager.GetActiveScriptableMode(ModeId));
	if (Mode)
	{
		Mode->SetEditTarget(InTarget);
	}
}

void UArcPlacedEntityEditorMode::ExitEditMode()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (!LevelEditor.IsValid())
	{
		return;
	}

	FEditorModeTools& ModeManager = LevelEditor->GetEditorModeManager();
	if (ModeManager.IsModeActive(ModeId))
	{
		ModeManager.DeactivateMode(ModeId);
	}
}
