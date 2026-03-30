// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorldEditorModule.h"

#include "ArcIWEditorCommands.h"
#include "ArcInstancedWorld/Components/ArcIWMassConfigComponent.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ArcInstancedWorldEditor"

static void ConvertAllConfigActorsToPartition()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<AActor*> ActorsToConvert;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && !Actor->IsA<AArcIWMassISMPartitionActor>() && Actor->FindComponentByClass<UArcIWMassConfigComponent>())
		{
			ActorsToConvert.Add(Actor);
		}
	}

	if (ActorsToConvert.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIW: No actors with UArcIWMassConfigComponent found in the level."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ArcIW: Converting %d actors to partition."), ActorsToConvert.Num());
	UE::ArcIW::Editor::ConvertActorsToPartition(World, ActorsToConvert);
}

static void ConvertAllPartitionsToActors()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<AArcIWMassISMPartitionActor*> PartitionActors;
	for (TActorIterator<AArcIWMassISMPartitionActor> It(World); It; ++It)
	{
		PartitionActors.Add(*It);
	}

	if (PartitionActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIW: No ArcIW partition actors found in the level."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ArcIW: Converting %d partition actors back to actors."), PartitionActors.Num());

	for (AArcIWMassISMPartitionActor* PartitionActor : PartitionActors)
	{
		UE::ArcIW::Editor::ConvertPartitionToActors(World, PartitionActor);
	}
}

static void RegisterToolbarMenu()
{
	UToolMenu* UserToolBar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& Section = UserToolBar->FindOrAddSection("ArcIW");

	FToolMenuEntry ComboEntry = FToolMenuEntry::InitComboButton(
		"ArcIWCombo",
		FUIAction(),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			FToolMenuSection& MenuSection = InMenu->AddSection("ArcIWConvert", LOCTEXT("ArcIWConvertSection", "ArcInstancedWorld"));

			MenuSection.AddMenuEntry(
				"ConvertToArcIW",
				LOCTEXT("ConvertToPartition", "Convert to ArcIW"),
				LOCTEXT("ConvertToPartitionTooltip", "Finds all actors with ArcIWMassConfigComponent in the level and converts them to partition representation."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&ConvertAllConfigActorsToPartition))
			);

			MenuSection.AddMenuEntry(
				"RestoreFromArcIW",
				LOCTEXT("ConvertFromPartition", "Restore from ArcIW"),
				LOCTEXT("ConvertFromPartitionTooltip", "Finds all ArcIW partition actors in the level and converts them back to regular actors."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&ConvertAllPartitionsToActors))
			);
		}),
		LOCTEXT("ArcIWLabel", "ArcIW"),
		LOCTEXT("ArcIWTooltip", "ArcInstancedWorld conversion tools")
	);
	ComboEntry.StyleNameOverride = "CalloutToolbar";

	Section.AddEntry(ComboEntry);
}

void FArcInstancedWorldEditorModule::StartupModule()
{
	if (!IsRunningCommandlet())
	{
		AddLevelViewportMenuExtender();
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterToolbarMenu));
	}
}

void FArcInstancedWorldEditorModule::ShutdownModule()
{
	RemoveLevelViewportMenuExtender();
	UToolMenus::UnRegisterStartupCallback(this);
}

void FArcInstancedWorldEditorModule::AddLevelViewportMenuExtender()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TArray<FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors>& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(
		this, &FArcInstancedWorldEditorModule::CreateLevelViewportContextMenuExtender));
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void FArcInstancedWorldEditorModule::RemoveLevelViewportMenuExtender()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll(
				[this](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& In)
				{
					return In.GetHandle() == LevelViewportExtenderHandle;
				});
		}
		LevelViewportExtenderHandle.Reset();
	}
}

TSharedRef<FExtender> FArcInstancedWorldEditorModule::CreateLevelViewportContextMenuExtender(
	const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	if (InActors.Num() == 0)
	{
		return Extender;
	}

	bool bHasRegularActors = false;
	bool bHasPartitionActors = false;

	for (AActor* Actor : InActors)
	{
		if (Actor->IsA<AArcIWMassISMPartitionActor>())
		{
			bHasPartitionActors = true;
		}
		else
		{
			bHasRegularActors = true;
		}
		if (bHasRegularActors && bHasPartitionActors)
		{
			break;
		}
	}

	Extender->AddMenuExtension(
		"ActorConvert",
		EExtensionHook::After,
		CommandList,
		FMenuExtensionDelegate::CreateLambda([bHasRegularActors, bHasPartitionActors, InActors](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("CtxConvertToPartition", "Convert to ArcIW Partition"),
				LOCTEXT("CtxConvertToPartitionTooltip", "Converts selected actors into ArcIW partition representation."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([InActors]()
					{
						UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
						if (!World)
						{
							return;
						}
						TArray<AActor*> RegularActors;
						for (AActor* Actor : InActors)
						{
							if (IsValid(Actor) && !Actor->IsA<AArcIWMassISMPartitionActor>())
							{
								RegularActors.Add(Actor);
							}
						}
						if (RegularActors.Num() > 0)
						{
							UE::ArcIW::Editor::ConvertActorsToPartition(World, RegularActors);
						}
					}),
					FCanExecuteAction::CreateLambda([bHasRegularActors]() { return bHasRegularActors; }))
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CtxConvertFromPartition", "Restore Actors from ArcIW Partition"),
				LOCTEXT("CtxConvertFromPartitionTooltip", "Restores actors from selected ArcIW partition actors."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([InActors]()
					{
						UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
						if (!World)
						{
							return;
						}
						for (AActor* Actor : InActors)
						{
							AArcIWMassISMPartitionActor* MassISMActor = Cast<AArcIWMassISMPartitionActor>(Actor);
							if (IsValid(MassISMActor))
							{
								UE::ArcIW::Editor::ConvertPartitionToActors(World, MassISMActor);
							}
						}
					}),
					FCanExecuteAction::CreateLambda([bHasPartitionActors]() { return bHasPartitionActors; }))
			);
		}));

	return Extender;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcInstancedWorldEditorModule, ArcInstancedWorldEditor)
