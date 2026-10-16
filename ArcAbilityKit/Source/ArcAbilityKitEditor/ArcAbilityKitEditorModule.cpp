// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAbilityKitEditorModule.h"

#include "Animation/ArcMontageMarkExtractor.h"
#include "Templates/UnrealTemplate.h"
#include "StateTree.h"
#include "StateTreeCompilerManager.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "Tasks/ArcAbilityTask_PlayCastMontageWithMarks.h"
#include "Tasks/ArcAbilityTask_PlayMontageStartLoopEndWithMarks.h"
#include "Tasks/ArcAbilityTask_PlayMontageWithMarks.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ObjectSaveContext.h"

namespace ArcAbilityKitEditor::MontageMarkSync
{
int32 GetTagOccurrence(
	const FArcMontageMarkEntry& SourceMark,
	const int32 SourceIndex,
	const TArray<FArcMontageMarkEntry>& SourceMarks)
{
	int32 Occurrence = 0;
	for (int32 PreviousSourceIndex = 0; PreviousSourceIndex < SourceIndex; ++PreviousSourceIndex)
	{
		if (SourceMarks[PreviousSourceIndex].Tag == SourceMark.Tag)
		{
			++Occurrence;
		}
	}

	return Occurrence;
}

int32 FindGuidMatch(
	const FArcMontageMarkEntry& SourceMark,
	const TArray<FArcMontageMarkEntry>& ExistingMarks,
	const TArray<bool>& MatchedExistingMarks)
{
	for (int32 ExistingIndex = 0; ExistingIndex < ExistingMarks.Num(); ++ExistingIndex)
	{
		if (!MatchedExistingMarks[ExistingIndex]
			&& ExistingMarks[ExistingIndex].SourceNotifyGuid == SourceMark.SourceNotifyGuid)
		{
			return ExistingIndex;
		}
	}

	return INDEX_NONE;
}

int32 FindTagOccurrenceMatch(
	const FArcMontageMarkEntry& SourceMark,
	const int32 SourceOccurrence,
	const TArray<FArcMontageMarkEntry>& ExistingMarks,
	const TArray<bool>& MatchedExistingMarks)
{
	int32 ExistingOccurrence = 0;
	for (int32 ExistingIndex = 0; ExistingIndex < ExistingMarks.Num(); ++ExistingIndex)
	{
		const FArcMontageMarkEntry& ExistingMark = ExistingMarks[ExistingIndex];
		if (ExistingMark.SourceNotifyGuid.IsValid()
			&& ExistingMark.Tag == SourceMark.Tag)
		{
			if (ExistingOccurrence == SourceOccurrence)
			{
				return MatchedExistingMarks[ExistingIndex] ? INDEX_NONE : ExistingIndex;
			}
			++ExistingOccurrence;
		}
	}

	return INDEX_NONE;
}

bool AreMarksIdentical(
	const TArray<FArcMontageMarkEntry>& ExistingMarks,
	const TArray<FArcMontageMarkEntry>& SyncedMarks)
{
	if (ExistingMarks.Num() != SyncedMarks.Num())
	{
		return false;
	}

	for (int32 MarkIndex = 0; MarkIndex < ExistingMarks.Num(); ++MarkIndex)
	{
		if (!FArcMontageMarkEntry::StaticStruct()->CompareScriptStruct(
			&ExistingMarks[MarkIndex], &SyncedMarks[MarkIndex], 0))
		{
			return false;
		}
	}

	return true;
}

bool SyncMarks(
	const bool bSyncMarksFromMontageOnSave,
	UAnimMontage* Montage,
	TArray<FArcMontageMarkEntry>& ExistingMarks)
{
	if (!bSyncMarksFromMontageOnSave || !IsValid(Montage))
	{
		return false;
	}

	TArray<FArcMontageMarkEntry> SourceMarks;
	ArcMontageMarks::ExtractMarks(Montage, SourceMarks);

	TArray<bool> MatchedExistingMarks;
	MatchedExistingMarks.Init(false, ExistingMarks.Num());

	TArray<FArcMontageMarkEntry> SyncedMarks;
	SyncedMarks.Reserve(SourceMarks.Num());
	for (int32 SourceIndex = 0; SourceIndex < SourceMarks.Num(); ++SourceIndex)
	{
		FArcMontageMarkEntry SyncedMark = SourceMarks[SourceIndex];
		const bool bSourceGuidWasInvalid = !SyncedMark.SourceNotifyGuid.IsValid();
		const int32 TagOccurrence = GetTagOccurrence(SyncedMark, SourceIndex, SourceMarks);
		if (bSourceGuidWasInvalid)
		{
			const FString SyntheticGuidSource = FString::Printf(
				TEXT("ArcAbilityKit.MontageMark.InvalidNotifyGuid.%s.%d"),
				*SyncedMark.Tag.ToString(), TagOccurrence);
			SyncedMark.SourceNotifyGuid = FGuid::NewDeterministicGuid(SyntheticGuidSource);
		}

		int32 ExistingIndex = FindGuidMatch(SyncedMark, ExistingMarks, MatchedExistingMarks);
		if (ExistingIndex == INDEX_NONE && bSourceGuidWasInvalid)
		{
			ExistingIndex = FindTagOccurrenceMatch(
				SyncedMark, TagOccurrence, ExistingMarks, MatchedExistingMarks);
		}

		if (ExistingIndex != INDEX_NONE)
		{
			const FArcMontageMarkEntry& ExistingMark = ExistingMarks[ExistingIndex];
			MatchedExistingMarks[ExistingIndex] = true;
			SyncedMark.Dispatcher = ExistingMark.Dispatcher;
			SyncedMark.Payload = ExistingMark.Payload;
			SyncedMark.DebugLabel = ExistingMark.DebugLabel;
		}
		SyncedMarks.Add(MoveTemp(SyncedMark));
	}

	if (AreMarksIdentical(ExistingMarks, SyncedMarks))
	{
		return false;
	}

	ExistingMarks = MoveTemp(SyncedMarks);
	return true;
}

bool SyncMarks(FArcAbilityTask_PlayMontageWithMarksInstanceData& InstanceData)
{
	return SyncMarks(
		InstanceData.bSyncMarksFromMontageOnSave,
		InstanceData.Montage.Get(),
		InstanceData.Marks);
}

bool SyncMarks(FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	bool bChanged = false;
	bChanged |= SyncMarks(
		InstanceData.bSyncMarksFromMontagesOnSave,
		InstanceData.StartMontage.Get(),
		InstanceData.StartMarks);
	bChanged |= SyncMarks(
		InstanceData.bSyncMarksFromMontagesOnSave,
		InstanceData.LoopMontage.Get(),
		InstanceData.LoopMarks);
	bChanged |= SyncMarks(
		InstanceData.bSyncMarksFromMontagesOnSave,
		InstanceData.EndMontage.Get(),
		InstanceData.EndMarks);
	return bChanged;
}

bool SyncMarks(FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
{
	bool bChanged = false;
	bChanged |= SyncMarks(
		InstanceData.bSyncMarksFromMontagesOnSave,
		InstanceData.StartMontage.Get(),
		InstanceData.StartMarks);
	bChanged |= SyncMarks(
		InstanceData.bSyncMarksFromMontagesOnSave,
		InstanceData.LoopMontage.Get(),
		InstanceData.LoopMarks);
	bChanged |= SyncMarks(
		InstanceData.bSyncMarksFromMontagesOnSave,
		InstanceData.EndMontage.Get(),
		InstanceData.EndMarks);
	return bChanged;
}

bool SyncStateTree(UStateTree& StateTree)
{
	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree.EditorData);
	if (EditorData == nullptr)
	{
		return false;
	}

	bool bChanged = false;
	EditorData->VisitAllNodes(
		[EditorData, &bChanged](
			const UStateTreeState*,
			const FStateTreeBindableStructDesc& Desc,
			const FStateTreeDataView Value)
		{
			FStateTreeEditorNode* EditorNode = EditorData->FindNode(Desc.ID);
			if (EditorNode == nullptr)
			{
				return EStateTreeVisitor::Continue;
			}

			if (EditorNode->GetNode().GetScriptStruct() == FArcAbilityTask_PlayMontageWithMarks::StaticStruct())
			{
				const FArcAbilityTask_PlayMontageWithMarks* Task =
					EditorNode->GetNode().GetPtr<FArcAbilityTask_PlayMontageWithMarks>();
				if (Task != nullptr && Task->bTaskEnabled
					&& Desc.Struct == FArcAbilityTask_PlayMontageWithMarksInstanceData::StaticStruct())
				{
					FArcAbilityTask_PlayMontageWithMarksInstanceData* InstanceData =
						Value.GetMutablePtr<FArcAbilityTask_PlayMontageWithMarksInstanceData>();
					if (InstanceData != nullptr)
					{
						bChanged |= SyncMarks(*InstanceData);
					}
				}
				return EStateTreeVisitor::Continue;
			}

			if (EditorNode->GetNode().GetScriptStruct()
				== FArcAbilityTask_PlayMontageStartLoopEndWithMarks::StaticStruct())
			{
				const FArcAbilityTask_PlayMontageStartLoopEndWithMarks* Task =
					EditorNode->GetNode().GetPtr<FArcAbilityTask_PlayMontageStartLoopEndWithMarks>();
				if (Task != nullptr && Task->bTaskEnabled
					&& Desc.Struct == FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData::StaticStruct())
				{
					FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData* InstanceData =
						Value.GetMutablePtr<FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData>();
					if (InstanceData != nullptr)
					{
						bChanged |= SyncMarks(*InstanceData);
					}
				}
				return EStateTreeVisitor::Continue;
			}

			if (EditorNode->GetNode().GetScriptStruct()
				== FArcAbilityTask_PlayCastMontageWithMarks::StaticStruct())
			{
				const FArcAbilityTask_PlayCastMontageWithMarks* Task =
					EditorNode->GetNode().GetPtr<FArcAbilityTask_PlayCastMontageWithMarks>();
				if (Task != nullptr && Task->bTaskEnabled
					&& Desc.Struct == FArcAbilityTask_PlayCastMontageWithMarksInstanceData::StaticStruct())
				{
					FArcAbilityTask_PlayCastMontageWithMarksInstanceData* InstanceData =
						Value.GetMutablePtr<FArcAbilityTask_PlayCastMontageWithMarksInstanceData>();
					if (InstanceData != nullptr)
					{
						bChanged |= SyncMarks(*InstanceData);
					}
				}
			}

			return EStateTreeVisitor::Continue;
		});

	return bChanged;
}
}

void FArcAbilityKitEditorModule::StartupModule()
{
	ObjectPreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddRaw(
		this, &FArcAbilityKitEditorModule::HandleObjectPreSave);
}

void FArcAbilityKitEditorModule::ShutdownModule()
{
	FCoreUObjectDelegates::OnObjectPreSave.Remove(ObjectPreSaveHandle);
	ObjectPreSaveHandle.Reset();
}

void FArcAbilityKitEditorModule::HandleObjectPreSave(UObject* Object, FObjectPreSaveContext Context)
{
	UStateTree* StateTree = Cast<UStateTree>(Object);
	if (StateTree == nullptr || bHandlingObjectPreSave
		|| Context.IsFromAutoSave() || Context.IsCooking() || Context.IsProceduralSave())
	{
		return;
	}

	TGuardValue<bool> ReentryGuard(bHandlingObjectPreSave, true);
	if (ArcAbilityKitEditor::MontageMarkSync::SyncStateTree(*StateTree))
	{
		StateTree->MarkAsModified(true);
		UE::StateTree::Compiler::FCompilerManager::CompileSynchronously(StateTree);
	}
}

IMPLEMENT_MODULE(FArcAbilityKitEditorModule, ArcAbilityKitEditor)
