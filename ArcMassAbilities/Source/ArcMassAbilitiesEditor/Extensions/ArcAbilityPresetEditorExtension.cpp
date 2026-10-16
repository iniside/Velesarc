// Copyright Lukasz Baran. All Rights Reserved.

#include "Extensions/ArcAbilityPresetEditorExtension.h"

#include "Presets/ArcAbilityPresetGenerator.h"
#include "Presets/ArcAbilityTreePreset.h"

#include "Editor.h"
#include "StateTree.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityPresetEditorExtension)

void UArcAbilityPresetEditorExtension::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (GEditor != nullptr && GEditor->GetTimerManager()->IsTimerActive(RebuildTimerHandle))
	{
		return;
	}

	PresetBeforeEdit = SourcePreset;
}

void UArcAbilityPresetEditorExtension::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() != GET_MEMBER_NAME_CHECKED(UArcAbilityPresetEditorExtension, SourcePreset))
	{
		return;
	}

	if (SourcePreset == nullptr)
	{
		Modify();
		HashAtGeneration = 0;
		return;
	}

	if (GEditor == nullptr)
	{
		return;
	}

	UStateTree* StateTree = GetTypedOuter<UStateTree>();
	if (StateTree == nullptr)
	{
		return;
	}

	FTimerManager& TimerManager = GEditor->GetTimerManager().Get();
	if (TimerManager.IsTimerActive(RebuildTimerHandle))
	{
		return;
	}

	// Deferred: we are inside FPropertyNode::NotifyPostChange and the build destroys the very objects this view's property nodes point at.
	RebuildTimerHandle = TimerManager.SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() -> void
	{
		UStateTree* OwnerStateTree = GetTypedOuter<UStateTree>();
		if (OwnerStateTree == nullptr || SourcePreset == nullptr)
		{
			return;
		}

		const FArcAbilityPresetResult Result = ArcAbilityPresetGenerator::ConfirmAndBuild(*OwnerStateTree, SourcePreset);
		if (Result.bCancelled || !Result.bSuccess)
		{
			SourcePreset = PresetBeforeEdit;
		}
	}));
}
