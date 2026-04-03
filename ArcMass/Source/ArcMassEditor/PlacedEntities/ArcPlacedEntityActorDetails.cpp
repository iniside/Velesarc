// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityActorDetails.h"

#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorModeManager.h"
#include "LevelEditor.h"
#include "PlacedEntities/ArcPlacedEntityEditorMode.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcPlacedEntityActorDetails"

namespace ArcPlacedEntityActorDetailsCallbacks
{
	static bool IsEditButtonEnabled(TWeakObjectPtr<AArcPlacedEntityPartitionActor> ActorPtr)
	{
		if (!ActorPtr.IsValid())
		{
			return false;
		}

		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (!LevelEditorModule)
		{
			return false;
		}

		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetFirstLevelEditor();
		if (!LevelEditor.IsValid())
		{
			return false;
		}

		FEditorModeTools& ModeManager = LevelEditor->GetEditorModeManager();
		if (!ModeManager.IsModeActive(UArcPlacedEntityEditorMode::ModeId))
		{
			return true;
		}

		UArcPlacedEntityEditorMode* Mode = Cast<UArcPlacedEntityEditorMode>(
			ModeManager.GetActiveScriptableMode(UArcPlacedEntityEditorMode::ModeId));
		return !Mode || Mode->GetEditTarget() != ActorPtr.Get();
	}

	static FReply OnEditButtonClicked(TWeakObjectPtr<AArcPlacedEntityPartitionActor> ActorPtr)
	{
		if (ActorPtr.IsValid())
		{
			UArcPlacedEntityEditorMode::EnterEditMode(ActorPtr.Get());
		}
		return FReply::Handled();
	}
}

TSharedRef<IDetailCustomization> FArcPlacedEntityActorDetails::MakeInstance()
{
	return MakeShareable(new FArcPlacedEntityActorDetails);
}

void FArcPlacedEntityActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);

	if (EditingObjects.Num() != 1)
	{
		return;
	}

	TWeakObjectPtr<AArcPlacedEntityPartitionActor> Actor = Cast<AArcPlacedEntityPartitionActor>(EditingObjects[0].Get());
	if (!Actor.IsValid())
	{
		return;
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		"PlacedEntities",
		LOCTEXT("PlacedEntitiesCategory", "Placed Entities"),
		ECategoryPriority::Important);

	Category.AddCustomRow(LOCTEXT("EditInstancesRow", "Edit Instances"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("EditInstancesLabel", "Edit Instances"))
		]
		.ValueContent()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.IsEnabled_Static(&ArcPlacedEntityActorDetailsCallbacks::IsEditButtonEnabled, Actor)
			.OnClicked_Static(&ArcPlacedEntityActorDetailsCallbacks::OnEditButtonClicked, Actor)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("EditInstancesButton", "Edit Instances"))
			]
		];
}

#undef LOCTEXT_NAMESPACE
