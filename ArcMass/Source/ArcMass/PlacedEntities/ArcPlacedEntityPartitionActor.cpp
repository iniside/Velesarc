// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/EntityFragments.h"
#include "Components/InstancedStaticMeshComponent.h"

#include "ArcMass/PlacedEntities/ArcLinkableGuid.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"

#if WITH_EDITOR
#include "ArcMass/Visualization/ArcMeshEntityVisualizationTrait.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationTrait.h"
#include "ArcMass/NiagaraVisualization/ArcNiagaraVisTrait.h"
#include "ArcMass/Visualization/ArcSkinnedMeshEntityVisualizationTrait.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "ArcMass/Elements/SkMInstance/ArcEditorInstancedSkinnedMeshComponent.h"
#include "NiagaraComponent.h"
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityCollisionDrawComponent.h"
#include "ArcMass/Elements/SkMInstance/ArcSkMInstanceElementUtils.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityPartitionActor)

AArcPlacedEntityPartitionActor::AArcPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// -----------------------------------------------------------------------------
// ISMInstanceManager
// -----------------------------------------------------------------------------

FText AArcPlacedEntityPartitionActor::GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return FText();
	}

	const UMassEntityConfigAsset* ConfigAsset = Entries[EntryIndex].EntityConfig;
	return FText::FromString(FString::Printf(TEXT("%s [%d]"), *GetNameSafe(ConfigAsset), TransformIndex));
#else
	return FText();
#endif
}

FText AArcPlacedEntityPartitionActor::GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return FText();
	}

	const UMassEntityConfigAsset* ConfigAsset = Entries[EntryIndex].EntityConfig;
	return FText::FromString(FString::Printf(TEXT("%s (Instance %d of %d)"),
		*GetPathNameSafe(ConfigAsset), TransformIndex, Entries[EntryIndex].InstanceTransforms.Num()));
#else
	return FText();
#endif
}

void AArcPlacedEntityPartitionActor::ForEachSMInstanceInSelectionGroup(const FSMInstanceId& InstanceId, TFunctionRef<bool(FSMInstanceId)> Callback)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return;
	}

	if (!EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		return;
	}

	const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];
	for (UInstancedStaticMeshComponent* ISMComp : Group.ISMComponents)
	{
		if (ISMComp == nullptr)
		{
			continue;
		}

		FSMInstanceId GroupMemberId;
		GroupMemberId.ISMComponent = ISMComp;
		GroupMemberId.InstanceIndex = TransformIndex;
		if (!Callback(GroupMemberId))
		{
			return;
		}
	}
#endif
}

bool AArcPlacedEntityPartitionActor::CanEditSMInstance(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return false;
	}

	OutInstanceTransform = Entries[EntryIndex].InstanceTransforms[TransformIndex];
	if (bWorldSpace)
	{
		OutInstanceTransform = OutInstanceTransform * GetActorTransform();
	}
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return false;
	}

	FTransform LocalTransform = InstanceTransform;
	if (bWorldSpace)
	{
		LocalTransform = InstanceTransform.GetRelativeTransform(GetActorTransform());
	}

	Entries[EntryIndex].InstanceTransforms[TransformIndex] = LocalTransform;

	if (EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];
		for (int32 CompIndex = 0; CompIndex < Group.ISMComponents.Num(); ++CompIndex)
		{
			UInstancedStaticMeshComponent* ISMComp = Group.ISMComponents[CompIndex];
			if (ISMComp == nullptr)
			{
				continue;
			}
			FTransform FinalTransform = Group.ComponentRelativeTransforms[CompIndex] * LocalTransform;
			ISMComp->UpdateInstanceTransform(TransformIndex, FinalTransform, /*bWorldSpace=*/false, bMarkRenderStateDirty, bTeleport);
		}

		if (Group.NiagaraComponents.IsValidIndex(TransformIndex))
		{
			UNiagaraComponent* NiagaraComp = Group.NiagaraComponents[TransformIndex];
			if (NiagaraComp != nullptr)
			{
				FTransform NiagaraFinalTransform = Group.NiagaraComponentTransform * LocalTransform;
				NiagaraComp->SetRelativeTransform(NiagaraFinalTransform);
			}
		}

		// Update skinned mesh components
		for (int32 CompIndex = 0; CompIndex < Group.SkinnedMeshComponents.Num(); ++CompIndex)
		{
			UInstancedSkinnedMeshComponent* SkMComp = Group.SkinnedMeshComponents[CompIndex];
			if (SkMComp == nullptr)
			{
				continue;
			}
			FTransform SkMFinalTransform = Group.SkinnedMeshRelativeTransforms[CompIndex] * LocalTransform;
			FPrimitiveInstanceId PrimId = SkMComp->GetInstanceId(TransformIndex);
			SkMComp->SetInstanceTransform(PrimId, SkMFinalTransform, /*bWorldSpace=*/false);
		}
	}

	return true;
#else
	return false;
#endif
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId)
{
#if WITH_EDITOR
	Modify();
#endif
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return;
	}

	if (EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];
		for (UInstancedStaticMeshComponent* ISMComp : Group.ISMComponents)
		{
			if (ISMComp != nullptr)
			{
				ISMComp->SelectInstance(bIsSelected, TransformIndex);
				ISMComp->MarkRenderStateDirty();
			}
		}
	}
#endif
}

bool AArcPlacedEntityPartitionActor::DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds)
{
#if WITH_EDITOR
	if (InstanceIds.Num() == 0)
	{
		return false;
	}

	// Pass 1: Resolve all instance IDs and group by entry index
	TMap<int32, TArray<int32>> TransformIndicesToRemoveByEntry;
	for (const FSMInstanceId& InstanceId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
		{
			TransformIndicesToRemoveByEntry.FindOrAdd(EntryIndex).AddUnique(TransformIndex);
		}
	}

	if (TransformIndicesToRemoveByEntry.IsEmpty())
	{
		return false;
	}

	Modify();

	// Pass 2: Remove transforms from back to front per entry
	for (TPair<int32, TArray<int32>>& Pair : TransformIndicesToRemoveByEntry)
	{
		TArray<int32>& IndicesToRemove = Pair.Value;
		IndicesToRemove.Sort(TGreater<int32>());

		FArcPlacedEntityEntry& Entry = Entries[Pair.Key];
		TArray<FTransform>& Transforms = Entry.InstanceTransforms;

		// Remove overrides and transforms (iterate back to front so indices stay valid)
		for (int32 TransformIndex : IndicesToRemove)
		{
			Entry.PerInstanceOverrides.Remove(TransformIndex);
			Transforms.RemoveAt(TransformIndex);
		}

		// Re-index remaining override keys: any key > a removed index shifts down by the
		// number of removed indices that were smaller than it.
		// Collect (newKey, value) pairs first to avoid mutating the map while reading it.
		TArray<TTuple<int32, FArcPlacedEntityFragmentOverrides>> ReindexedPairs;
		ReindexedPairs.Reserve(Entry.PerInstanceOverrides.Num());
		for (TPair<int32, FArcPlacedEntityFragmentOverrides>& OverridePair : Entry.PerInstanceOverrides)
		{
			int32 OriginalKey = OverridePair.Key;
			int32 Shift = 0;
			for (int32 RemovedIndex : IndicesToRemove)
			{
				if (RemovedIndex < OriginalKey)
				{
					++Shift;
				}
			}
			ReindexedPairs.Emplace(OriginalKey - Shift, MoveTemp(OverridePair.Value));
		}
		Entry.PerInstanceOverrides.Reset();
		for (TTuple<int32, FArcPlacedEntityFragmentOverrides>& ReindexedPair : ReindexedPairs)
		{
			Entry.PerInstanceOverrides.Emplace(ReindexedPair.Get<0>(), MoveTemp(ReindexedPair.Get<1>()));
		}
	}

	// Prune empty entries (iterate back to front)
	for (int32 EntryIndex = Entries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (Entries[EntryIndex].InstanceTransforms.Num() == 0)
		{
			Entries.RemoveAt(EntryIndex);
		}
	}

	// Self-destruct if no entries remain
	if (Entries.Num() == 0)
	{
		UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
		if (EditorActorSubsystem != nullptr)
		{
			EditorActorSubsystem->DestroyActor(this);
		}
		return true;
	}

	// Rebuild ISMCs to reflect remaining instances
	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds)
{
#if WITH_EDITOR
	if (InstanceIds.Num() == 0)
	{
		return false;
	}

	// Pass 1: Resolve all instance IDs, deduplicate composite entries that share the same logical instance
	struct FSourceInstance
	{
		int32 EntryIndex;
		int32 TransformIndex;
	};
	TArray<FSourceInstance> UniqueSources;

	for (const FSMInstanceId& InstanceId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
		{
			continue;
		}

		bool bAlreadyPresent = false;
		for (const FSourceInstance& Existing : UniqueSources)
		{
			if (Existing.EntryIndex == EntryIndex && Existing.TransformIndex == TransformIndex)
			{
				bAlreadyPresent = true;
				break;
			}
		}

		if (!bAlreadyPresent)
		{
			UniqueSources.Add({ EntryIndex, TransformIndex });
		}
	}

	if (UniqueSources.IsEmpty())
	{
		return false;
	}

	Modify();

	// Pass 2: Copy transforms, track new indices per entry
	struct FNewInstance
	{
		int32 EntryIndex;
		int32 NewTransformIndex;
	};
	TArray<FNewInstance> NewInstances;
	NewInstances.Reserve(UniqueSources.Num());

	for (const FSourceInstance& Source : UniqueSources)
	{
		FArcPlacedEntityEntry& Entry = Entries[Source.EntryIndex];
		FTransform SourceTransform = Entry.InstanceTransforms[Source.TransformIndex];
		int32 NewIndex = Entry.InstanceTransforms.Add(SourceTransform);

		// Copy overrides if source instance has any
		const FArcPlacedEntityFragmentOverrides* SourceOverrides = Entry.PerInstanceOverrides.Find(Source.TransformIndex);
		if (SourceOverrides != nullptr)
		{
			Entry.PerInstanceOverrides.Add(NewIndex, *SourceOverrides);

			// Regenerate LinkGuid for the duplicate so it gets its own identity
			FArcPlacedEntityFragmentOverrides* NewOverrides = Entry.PerInstanceOverrides.Find(NewIndex);
			if (NewOverrides != nullptr)
			{
				for (FInstancedStruct& FragmentValue : NewOverrides->FragmentValues)
				{
					if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
					{
						FArcLinkableGuidFragment& GuidFrag = FragmentValue.GetMutable<FArcLinkableGuidFragment>();
						GuidFrag.LinkGuid = FGuid::NewGuid();
						break;
					}
				}
			}
		}

		// Ensure duplicate gets a LinkGuid even if source had no overrides
		AutoAssignLinkGuidIfNeeded(Entry, NewIndex);

		NewInstances.Add({ Source.EntryIndex, NewIndex });
	}

	// Rebuild ISMCs to include the new instances
	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();

	// Pass 3: Build output IDs using the primary (first) ISMC of each entry group
	OutNewInstanceIds.Reserve(NewInstances.Num());
	for (const FNewInstance& NewInst : NewInstances)
	{
		if (EditorPreviewGroups.IsValidIndex(NewInst.EntryIndex)
			&& EditorPreviewGroups[NewInst.EntryIndex].ISMComponents.Num() > 0)
		{
			FSMInstanceId NewId;
			NewId.ISMComponent = EditorPreviewGroups[NewInst.EntryIndex].ISMComponents[0];
			NewId.InstanceIndex = NewInst.NewTransformIndex;
			OutNewInstanceIds.Add(NewId);
		}
	}

	return OutNewInstanceIds.Num() > 0;
#else
	return false;
#endif
}

TSubclassOf<USMInstanceProxyEditingObject> AArcPlacedEntityPartitionActor::GetSMInstanceEditingProxyClass() const
{
#if WITH_EDITOR
	static UClass* ProxyClass = FindObject<UClass>(nullptr, TEXT("/Script/ArcMassEditor.ArcPlacedEntityProxyEditingObject"));
	return ProxyClass;
#else
	return nullptr;
#endif
}

// -----------------------------------------------------------------------------
// ISkMInstanceManager
// -----------------------------------------------------------------------------

FText AArcPlacedEntityPartitionActor::GetSkMInstanceDisplayName(const FSkMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return FText();
	}

	const UMassEntityConfigAsset* ConfigAsset = Entries[EntryIndex].EntityConfig;
	return FText::FromString(FString::Printf(TEXT("%s [%d]"), *GetNameSafe(ConfigAsset), TransformIndex));
#else
	return FText();
#endif
}

FText AArcPlacedEntityPartitionActor::GetSkMInstanceTooltip(const FSkMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return FText();
	}

	const UMassEntityConfigAsset* ConfigAsset = Entries[EntryIndex].EntityConfig;
	return FText::FromString(FString::Printf(TEXT("%s (Instance %d of %d)"),
		*GetPathNameSafe(ConfigAsset), TransformIndex, Entries[EntryIndex].InstanceTransforms.Num()));
#else
	return FText();
#endif
}

void AArcPlacedEntityPartitionActor::ForEachSkMInstanceInSelectionGroup(const FSkMInstanceId& InstanceId, TFunctionRef<bool(FSkMInstanceId)> Callback)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return;
	}

	if (!EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		return;
	}

	const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];
	for (UInstancedSkinnedMeshComponent* SkMComp : Group.SkinnedMeshComponents)
	{
		if (SkMComp == nullptr)
		{
			continue;
		}

		FPrimitiveInstanceId PrimId = SkMComp->GetInstanceId(TransformIndex);
		FSkMInstanceId GroupMemberId;
		GroupMemberId.SkMComponent = SkMComp;
		GroupMemberId.InstanceId = PrimId.GetAsIndex();
		if (!Callback(GroupMemberId))
		{
			return;
		}
	}
#endif
}

bool AArcPlacedEntityPartitionActor::CanEditSkMInstance(const FSkMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::CanMoveSkMInstance(const FSkMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::GetSkMInstanceTransform(const FSkMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return false;
	}

	OutInstanceTransform = Entries[EntryIndex].InstanceTransforms[TransformIndex];
	if (bWorldSpace)
	{
		OutInstanceTransform = OutInstanceTransform * GetActorTransform();
	}
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::SetSkMInstanceTransform(const FSkMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveSkMInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return false;
	}

	FTransform LocalTransform = InstanceTransform;
	if (bWorldSpace)
	{
		LocalTransform = InstanceTransform.GetRelativeTransform(GetActorTransform());
	}

	Entries[EntryIndex].InstanceTransforms[TransformIndex] = LocalTransform;

	if (EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];

		// Update ISM components
		for (int32 CompIndex = 0; CompIndex < Group.ISMComponents.Num(); ++CompIndex)
		{
			UInstancedStaticMeshComponent* ISMComp = Group.ISMComponents[CompIndex];
			if (ISMComp == nullptr)
			{
				continue;
			}
			FTransform FinalTransform = Group.ComponentRelativeTransforms[CompIndex] * LocalTransform;
			ISMComp->UpdateInstanceTransform(TransformIndex, FinalTransform, /*bWorldSpace=*/false, bMarkRenderStateDirty, bTeleport);
		}

		// Update skinned mesh components
		for (int32 CompIndex = 0; CompIndex < Group.SkinnedMeshComponents.Num(); ++CompIndex)
		{
			UInstancedSkinnedMeshComponent* SkMComp = Group.SkinnedMeshComponents[CompIndex];
			if (SkMComp == nullptr)
			{
				continue;
			}
			FTransform SkMFinalTransform = Group.SkinnedMeshRelativeTransforms[CompIndex] * LocalTransform;
			FPrimitiveInstanceId PrimId = SkMComp->GetInstanceId(TransformIndex);
			SkMComp->SetInstanceTransform(PrimId, SkMFinalTransform, /*bWorldSpace=*/false);
		}

		// Update Niagara components
		if (Group.NiagaraComponents.IsValidIndex(TransformIndex))
		{
			UNiagaraComponent* NiagaraComp = Group.NiagaraComponents[TransformIndex];
			if (NiagaraComp != nullptr)
			{
				FTransform NiagaraFinalTransform = Group.NiagaraComponentTransform * LocalTransform;
				NiagaraComp->SetRelativeTransform(NiagaraFinalTransform);
			}
		}
	}

	return true;
#else
	return false;
#endif
}

void AArcPlacedEntityPartitionActor::NotifySkMInstanceMovementStarted(const FSkMInstanceId& InstanceId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifySkMInstanceMovementOngoing(const FSkMInstanceId& InstanceId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifySkMInstanceMovementEnded(const FSkMInstanceId& InstanceId)
{
#if WITH_EDITOR
	Modify();
#endif
}

void AArcPlacedEntityPartitionActor::NotifySkMInstanceSelectionChanged(const FSkMInstanceId& InstanceId, const bool bIsSelected)
{
	// No-op — skinned mesh instances do not support per-instance selection highlighting yet
}

bool AArcPlacedEntityPartitionActor::DeleteSkMInstances(TArrayView<const FSkMInstanceId> InstanceIds)
{
#if WITH_EDITOR
	if (InstanceIds.Num() == 0)
	{
		return false;
	}

	// Convert SkM instance IDs to SM instance IDs and delegate to the existing DeleteSMInstances.
	// Both ISM and SkM share the same Entries[EntryIndex].InstanceTransforms[TransformIndex],
	// so we resolve the SkM ID to entry+transform, then build an FSMInstanceId from the ISM component in that group.
	TArray<FSMInstanceId> SMInstanceIds;
	SMInstanceIds.Reserve(InstanceIds.Num());

	for (const FSkMInstanceId& SkMId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (!ResolveSkMInstanceId(SkMId, EntryIndex, TransformIndex))
		{
			continue;
		}

		// If the entry also has ISM components, delegate via FSMInstanceId
		if (EditorPreviewGroups.IsValidIndex(EntryIndex)
			&& EditorPreviewGroups[EntryIndex].ISMComponents.Num() > 0)
		{
			FSMInstanceId SMId;
			SMId.ISMComponent = EditorPreviewGroups[EntryIndex].ISMComponents[0];
			SMId.InstanceIndex = TransformIndex;
			SMInstanceIds.Add(SMId);
		}
		else
		{
			// Skinned-mesh-only entry — handle directly (same logic as DeleteSMInstances)
			// For now this path is unlikely since most configs have both ISM and SkM,
			// but we handle it for completeness by collecting and removing below.
		}
	}

	if (SMInstanceIds.Num() > 0)
	{
		return DeleteSMInstances(SMInstanceIds);
	}

	// Direct deletion path for skinned-mesh-only entries
	TMap<int32, TArray<int32>> TransformIndicesToRemoveByEntry;
	for (const FSkMInstanceId& SkMId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (ResolveSkMInstanceId(SkMId, EntryIndex, TransformIndex))
		{
			TransformIndicesToRemoveByEntry.FindOrAdd(EntryIndex).AddUnique(TransformIndex);
		}
	}

	if (TransformIndicesToRemoveByEntry.IsEmpty())
	{
		return false;
	}

	Modify();

	for (TPair<int32, TArray<int32>>& Pair : TransformIndicesToRemoveByEntry)
	{
		TArray<int32>& IndicesToRemove = Pair.Value;
		IndicesToRemove.Sort(TGreater<int32>());

		FArcPlacedEntityEntry& Entry = Entries[Pair.Key];
		TArray<FTransform>& Transforms = Entry.InstanceTransforms;

		for (int32 TransformIndex : IndicesToRemove)
		{
			Entry.PerInstanceOverrides.Remove(TransformIndex);
			Transforms.RemoveAt(TransformIndex);
		}

		// Re-index remaining override keys
		TArray<TTuple<int32, FArcPlacedEntityFragmentOverrides>> ReindexedPairs;
		ReindexedPairs.Reserve(Entry.PerInstanceOverrides.Num());
		for (TPair<int32, FArcPlacedEntityFragmentOverrides>& OverridePair : Entry.PerInstanceOverrides)
		{
			int32 OriginalKey = OverridePair.Key;
			int32 Shift = 0;
			for (int32 RemovedIndex : IndicesToRemove)
			{
				if (RemovedIndex < OriginalKey)
				{
					++Shift;
				}
			}
			ReindexedPairs.Emplace(OriginalKey - Shift, MoveTemp(OverridePair.Value));
		}
		Entry.PerInstanceOverrides.Reset();
		for (TTuple<int32, FArcPlacedEntityFragmentOverrides>& ReindexedPair : ReindexedPairs)
		{
			Entry.PerInstanceOverrides.Emplace(ReindexedPair.Get<0>(), MoveTemp(ReindexedPair.Get<1>()));
		}
	}

	// Prune empty entries (iterate back to front)
	for (int32 EntryIndex = Entries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (Entries[EntryIndex].InstanceTransforms.Num() == 0)
		{
			Entries.RemoveAt(EntryIndex);
		}
	}

	if (Entries.Num() == 0)
	{
		UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
		if (EditorActorSubsystem != nullptr)
		{
			EditorActorSubsystem->DestroyActor(this);
		}
		return true;
	}

	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::DuplicateSkMInstances(TArrayView<const FSkMInstanceId> InstanceIds, TArray<FSkMInstanceId>& OutNewInstanceIds)
{
#if WITH_EDITOR
	if (InstanceIds.Num() == 0)
	{
		return false;
	}

	// Deduplicate: resolve all SkM IDs to entry+transform pairs
	struct FSourceInstance
	{
		int32 EntryIndex;
		int32 TransformIndex;
	};
	TArray<FSourceInstance> UniqueSources;

	for (const FSkMInstanceId& SkMId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (!ResolveSkMInstanceId(SkMId, EntryIndex, TransformIndex))
		{
			continue;
		}

		bool bAlreadyPresent = false;
		for (const FSourceInstance& Existing : UniqueSources)
		{
			if (Existing.EntryIndex == EntryIndex && Existing.TransformIndex == TransformIndex)
			{
				bAlreadyPresent = true;
				break;
			}
		}

		if (!bAlreadyPresent)
		{
			UniqueSources.Add({ EntryIndex, TransformIndex });
		}
	}

	if (UniqueSources.IsEmpty())
	{
		return false;
	}

	Modify();

	// Copy transforms and overrides
	struct FNewInstance
	{
		int32 EntryIndex;
		int32 NewTransformIndex;
	};
	TArray<FNewInstance> NewInstances;
	NewInstances.Reserve(UniqueSources.Num());

	for (const FSourceInstance& Source : UniqueSources)
	{
		FArcPlacedEntityEntry& Entry = Entries[Source.EntryIndex];
		FTransform SourceTransform = Entry.InstanceTransforms[Source.TransformIndex];
		int32 NewIndex = Entry.InstanceTransforms.Add(SourceTransform);

		const FArcPlacedEntityFragmentOverrides* SourceOverrides = Entry.PerInstanceOverrides.Find(Source.TransformIndex);
		if (SourceOverrides != nullptr)
		{
			Entry.PerInstanceOverrides.Add(NewIndex, *SourceOverrides);

			FArcPlacedEntityFragmentOverrides* NewOverrides = Entry.PerInstanceOverrides.Find(NewIndex);
			if (NewOverrides != nullptr)
			{
				for (FInstancedStruct& FragmentValue : NewOverrides->FragmentValues)
				{
					if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
					{
						FArcLinkableGuidFragment& GuidFrag = FragmentValue.GetMutable<FArcLinkableGuidFragment>();
						GuidFrag.LinkGuid = FGuid::NewGuid();
						break;
					}
				}
			}
		}

		AutoAssignLinkGuidIfNeeded(Entry, NewIndex);
		NewInstances.Add({ Source.EntryIndex, NewIndex });
	}

	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();

	// Build output FSkMInstanceIds from rebuilt skinned mesh components
	OutNewInstanceIds.Reserve(NewInstances.Num());
	for (const FNewInstance& NewInst : NewInstances)
	{
		if (EditorPreviewGroups.IsValidIndex(NewInst.EntryIndex)
			&& EditorPreviewGroups[NewInst.EntryIndex].SkinnedMeshComponents.Num() > 0)
		{
			UInstancedSkinnedMeshComponent* SkMComp = EditorPreviewGroups[NewInst.EntryIndex].SkinnedMeshComponents[0];
			if (SkMComp != nullptr)
			{
				FPrimitiveInstanceId PrimId = SkMComp->GetInstanceId(NewInst.NewTransformIndex);
				FSkMInstanceId NewId;
				NewId.SkMComponent = SkMComp;
				NewId.InstanceId = PrimId.GetAsIndex();
				OutNewInstanceIds.Add(NewId);
			}
		}
	}

	return OutNewInstanceIds.Num() > 0;
#else
	return false;
#endif
}

// -----------------------------------------------------------------------------
// ISkMInstanceManagerProvider
// -----------------------------------------------------------------------------

ISkMInstanceManager* AArcPlacedEntityPartitionActor::GetSkMInstanceManager(const FSkMInstanceId& InstanceId)
{
#if WITH_EDITORONLY_DATA
	UInstancedSkinnedMeshComponent* Component = InstanceId.SkMComponent;
	if (Component != nullptr)
	{
		for (const FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
		{
			if (Group.SkinnedMeshComponents.Contains(Component))
			{
				return this;
			}
		}
	}
#endif
	return nullptr;
}

// -----------------------------------------------------------------------------
// ISMInstanceManagerProvider
// -----------------------------------------------------------------------------

ISMInstanceManager* AArcPlacedEntityPartitionActor::GetSMInstanceManager(const FSMInstanceId& InstanceId)
{
#if WITH_EDITORONLY_DATA
	UInstancedStaticMeshComponent* Component = InstanceId.ISMComponent;
	if (Component != nullptr)
	{
		for (const FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
		{
			if (Group.ISMComponents.Contains(Component))
			{
				return this;
			}
		}
	}
#endif
	return nullptr;
}

// -----------------------------------------------------------------------------
// Runtime Entity Spawning
// -----------------------------------------------------------------------------

void AArcPlacedEntityPartitionActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnEntities();
}

void AArcPlacedEntityPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnEntities();
	Super::EndPlay(EndPlayReason);
}

void AArcPlacedEntityPartitionActor::SpawnEntities()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Count total entities to reserve
	int32 TotalCount = 0;
	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig != nullptr)
		{
			TotalCount += Entry.InstanceTransforms.Num();
		}
	}

	if (TotalCount == 0)
	{
		return;
	}

	SpawnedEntities.Reserve(TotalCount);

	// Single creation context across all entries — observers fire once when it drops
	TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager.GetOrMakeCreationContext();

	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig == nullptr || Entry.InstanceTransforms.Num() == 0)
		{
			continue;
		}

		const FMassEntityTemplate& Template = Entry.EntityConfig->GetOrCreateEntityTemplate(*World);
		const FMassArchetypeHandle& Archetype = Template.GetArchetype();
		if (!Archetype.IsValid())
		{
			UE_LOG(LogMass, Warning, TEXT("AArcPlacedEntityPartitionActor::SpawnEntities: Invalid archetype for config '%s'"),
				*GetNameSafe(Entry.EntityConfig));
			continue;
		}

		const FMassArchetypeSharedFragmentValues& SharedValues = Template.GetSharedFragmentValues();
		const int32 Count = Entry.InstanceTransforms.Num();

		TArray<FMassEntityHandle> EntryEntities;
		EntityManager.BatchCreateEntities(Archetype, SharedValues, Count, EntryEntities);

		TConstArrayView<FInstancedStruct> InitialFragmentValues = Template.GetInitialFragmentValues();

		for (int32 i = 0; i < Count; ++i)
		{
			FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(EntryEntities[i]);
			TransformFragment.SetTransform(Entry.InstanceTransforms[i] * GetActorTransform());

			for (const FInstancedStruct& FragmentValue : InitialFragmentValues)
			{
				const UScriptStruct* FragmentType = FragmentValue.GetScriptStruct();
				if (FragmentType != nullptr && FragmentType != FTransformFragment::StaticStruct())
				{
					FStructView TargetFragment = EntityManager.GetFragmentDataStruct(EntryEntities[i], FragmentType);
					if (TargetFragment.IsValid())
					{
						FragmentType->CopyScriptStruct(TargetFragment.GetMemory(), FragmentValue.GetMemory());
					}
				}
			}

			// Apply per-instance fragment overrides
			const FArcPlacedEntityFragmentOverrides* Overrides = Entry.PerInstanceOverrides.Find(i);
			if (Overrides != nullptr)
			{
				for (const FInstancedStruct& Override : Overrides->FragmentValues)
				{
					const UScriptStruct* FragmentType = Override.GetScriptStruct();
					if (FragmentType == nullptr)
					{
						continue;
					}
					FStructView TargetFragment = EntityManager.GetFragmentDataStruct(EntryEntities[i], FragmentType);
					if (TargetFragment.IsValid())
					{
						FragmentType->CopyScriptStruct(TargetFragment.GetMemory(), Override.GetMemory());
					}
				}
			}

			// If entity has both LinkGuid and PersistenceGuid, unify them
			FStructView LinkGuidView = EntityManager.GetFragmentDataStruct(EntryEntities[i], FArcLinkableGuidFragment::StaticStruct());
			if (LinkGuidView.IsValid())
			{
				const FArcLinkableGuidFragment& LinkGuidFrag = LinkGuidView.Get<const FArcLinkableGuidFragment>();
				if (LinkGuidFrag.LinkGuid.IsValid())
				{
					FStructView PersistView = EntityManager.GetFragmentDataStruct(EntryEntities[i], FArcMassPersistenceFragment::StaticStruct());
					if (PersistView.IsValid())
					{
						FArcMassPersistenceFragment& PersistFrag = PersistView.Get<FArcMassPersistenceFragment>();
						PersistFrag.PersistenceGuid = LinkGuidFrag.LinkGuid;
					}
				}
			}
		}

		SpawnedEntities.Append(EntryEntities);
	}

	// CreationContext drops here — all observers fire once
}

void AArcPlacedEntityPartitionActor::DespawnEntities()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	TArray<FMassEntityHandle> ValidEntities;
	ValidEntities.Reserve(SpawnedEntities.Num());
	for (const FMassEntityHandle& EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			ValidEntities.Add(EntityHandle);
		}
	}

	if (ValidEntities.Num() > 0)
	{
		EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	}

	SpawnedEntities.Reset();
}

void AArcPlacedEntityPartitionActor::BeginDestroy()
{
#if WITH_EDITOR
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
#endif
	Super::BeginDestroy();
}

// -----------------------------------------------------------------------------
// Editor
// -----------------------------------------------------------------------------

#if WITH_EDITOR

uint32 AArcPlacedEntityPartitionActor::GetDefaultGridSize(UWorld* InWorld) const
{
	return 25600;
}

void AArcPlacedEntityPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		RebuildEditorPreviewISMCs();

		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		FEditorDelegates::EndPIE.Remove(EndPIEHandle);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &AArcPlacedEntityPartitionActor::OnPreBeginPIE);
		EndPIEHandle = FEditorDelegates::EndPIE.AddUObject(this, &AArcPlacedEntityPartitionActor::OnEndPIE);
	}
}

void AArcPlacedEntityPartitionActor::OnPreBeginPIE(const bool bIsSimulating)
{
	DestroyEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::OnEndPIE(const bool bIsSimulating)
{
	RebuildEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::PostLoad()
{
	Super::PostLoad();

	for (FArcPlacedEntityEntry& Entry : Entries)
	{
		for (int32 i = 0; i < Entry.InstanceTransforms.Num(); ++i)
		{
			AutoAssignLinkGuidIfNeeded(Entry, i);
		}
	}
	RebuildGuidIndex();
}

void AArcPlacedEntityPartitionActor::AutoAssignLinkGuidIfNeeded(FArcPlacedEntityEntry& Entry, int32 InstanceIndex)
{
	if (Entry.EntityConfig == nullptr)
	{
		return;
	}

	const UMassEntityTraitBase* LinkableTrait = Entry.EntityConfig->GetConfig().FindTrait(UArcLinkableGuidTrait::StaticClass());
	if (LinkableTrait == nullptr)
	{
		return;
	}

	FArcPlacedEntityFragmentOverrides& Overrides = Entry.PerInstanceOverrides.FindOrAdd(InstanceIndex);

	// Check if a LinkGuid override already exists
	for (FInstancedStruct& FragmentValue : Overrides.FragmentValues)
	{
		if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
		{
			return;
		}
	}

	FArcLinkableGuidFragment NewGuidFrag;
	NewGuidFrag.LinkGuid = FGuid::NewGuid();
	Overrides.FragmentValues.Add(FInstancedStruct::Make(NewGuidFrag));
}

void AArcPlacedEntityPartitionActor::AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex)
{
	OutEntryIndex = INDEX_NONE;
	OutInstanceIndex = INDEX_NONE;

	if (ConfigAsset == nullptr)
	{
		return;
	}

	Modify();

	// Find existing entry for this config
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		if (Entry.EntityConfig == ConfigAsset)
		{
			OutInstanceIndex = Entry.InstanceTransforms.Add(Transform);
			AutoAssignLinkGuidIfNeeded(Entry, OutInstanceIndex);
			OutEntryIndex = EntryIndex;
			RebuildGuidIndex();
			RebuildEditorPreviewISMCs();
			return;
		}
	}

	// Create new entry
	OutEntryIndex = Entries.Num();
	FArcPlacedEntityEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EntityConfig = ConfigAsset;
	OutInstanceIndex = NewEntry.InstanceTransforms.Add(Transform);
	AutoAssignLinkGuidIfNeeded(NewEntry, OutInstanceIndex);
	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();
}

UInstancedStaticMeshComponent* AArcPlacedEntityPartitionActor::GetEditorPreviewISMC(int32 EntryIndex) const
{
	if (!EditorPreviewGroups.IsValidIndex(EntryIndex) || EditorPreviewGroups[EntryIndex].ISMComponents.Num() == 0)
	{
		return nullptr;
	}
	return EditorPreviewGroups[EntryIndex].ISMComponents[0];
}

void AArcPlacedEntityPartitionActor::RebuildEditorPreviewISMCs()
{
	if (bIsRebuildingPreviewISMCs)
	{
		return;
	}
	TGuardValue<bool> RebuildGuard(bIsRebuildingPreviewISMCs, true);

	DestroyEditorPreviewISMCs();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups.AddDefaulted_GetRef();

		if (Entry.EntityConfig == nullptr)
		{
			continue;
		}

		// Try single-mesh trait
		const UArcMeshEntityVisualizationTrait* SingleMeshTrait = Cast<UArcMeshEntityVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcMeshEntityVisualizationTrait::StaticClass()));
		if (SingleMeshTrait != nullptr && SingleMeshTrait->IsValid())
		{
			UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
			ISMComponent->SetStaticMesh(SingleMeshTrait->GetMesh());
			ISMComponent->bIsEditorOnly = true;
			ISMComponent->bHasPerInstanceHitProxies = true;
			ISMComponent->SetCanEverAffectNavigation(false);

			const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = SingleMeshTrait->GetMaterialOverrides();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialOverrides.Num(); ++MaterialIndex)
			{
				if (MaterialOverrides[MaterialIndex] != nullptr)
				{
					ISMComponent->SetMaterial(MaterialIndex, MaterialOverrides[MaterialIndex]);
				}
			}

			ISMComponent->SetupAttachment(GetRootComponent());
			ISMComponent->RegisterComponent();
			AddInstanceComponent(ISMComponent);

			const FTransform& CompTransform = SingleMeshTrait->GetComponentTransform();
			for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
			{
				FTransform FinalTransform = CompTransform * InstanceTransform;
				ISMComponent->AddInstance(FinalTransform, /*bWorldSpace=*/false);
			}

			Group.ISMComponents.Add(ISMComponent);
			Group.ComponentRelativeTransforms.Add(CompTransform);
		}

		// Try composite mesh trait
		const UArcCompositeMeshVisualizationTrait* CompositeTrait = Cast<UArcCompositeMeshVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcCompositeMeshVisualizationTrait::StaticClass()));
		if (CompositeTrait != nullptr && CompositeTrait->IsValid())
		{
			const TArray<FArcCompositeMeshEntry>& MeshEntries = CompositeTrait->GetMeshEntries();
			for (int32 MeshIndex = 0; MeshIndex < MeshEntries.Num(); ++MeshIndex)
			{
				const FArcCompositeMeshEntry& MeshEntry = MeshEntries[MeshIndex];
				if (MeshEntry.Mesh == nullptr)
				{
					continue;
				}

				UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
				ISMComponent->SetStaticMesh(MeshEntry.Mesh);
				ISMComponent->bIsEditorOnly = true;
				ISMComponent->bHasPerInstanceHitProxies = true;
				ISMComponent->SetCanEverAffectNavigation(false);

				for (int32 MaterialIndex = 0; MaterialIndex < MeshEntry.MaterialOverrides.Num(); ++MaterialIndex)
				{
					if (MeshEntry.MaterialOverrides[MaterialIndex] != nullptr)
					{
						ISMComponent->SetMaterial(MaterialIndex, MeshEntry.MaterialOverrides[MaterialIndex]);
					}
				}

				ISMComponent->SetupAttachment(GetRootComponent());
				ISMComponent->RegisterComponent();
				AddInstanceComponent(ISMComponent);

				const FTransform& MeshRelativeTransform = MeshEntry.RelativeTransform;
				for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
				{
					FTransform FinalTransform = MeshRelativeTransform * InstanceTransform;
					ISMComponent->AddInstance(FinalTransform, /*bWorldSpace=*/false);
				}

				Group.ISMComponents.Add(ISMComponent);
				Group.ComponentRelativeTransforms.Add(MeshRelativeTransform);
			}
		}

		// Try Niagara visualization trait
		const UArcNiagaraVisTrait* NiagaraTrait = Cast<UArcNiagaraVisTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcNiagaraVisTrait::StaticClass()));
		if (NiagaraTrait != nullptr && NiagaraTrait->IsValid())
		{
			const FArcNiagaraVisConfigFragment& NiagaraConfig = NiagaraTrait->GetNiagaraConfig();
			UNiagaraSystem* NiagaraSystem = NiagaraConfig.NiagaraSystem.LoadSynchronous();
			if (NiagaraSystem != nullptr)
			{
				const FTransform& ComponentTransform = NiagaraTrait->GetComponentTransform();
				Group.NiagaraComponentTransform = ComponentTransform;

				for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
				{
					UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(this);
					NiagaraComp->bIsEditorOnly = true;
					NiagaraComp->SetAsset(NiagaraSystem);
					NiagaraComp->SetupAttachment(GetRootComponent());
					FTransform FinalTransform = ComponentTransform * InstanceTransform;
					NiagaraComp->SetRelativeTransform(FinalTransform);
					NiagaraComp->RegisterComponent();
					NiagaraComp->Activate(true);

					Group.NiagaraComponents.Add(NiagaraComp);
				}
			}
		}

		// Try skinned mesh trait
		const UArcSkinnedMeshEntityVisualizationTrait* SkinnedMeshTrait = Cast<UArcSkinnedMeshEntityVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcSkinnedMeshEntityVisualizationTrait::StaticClass()));
		if (SkinnedMeshTrait != nullptr && SkinnedMeshTrait->IsValid())
		{
			UInstancedSkinnedMeshComponent* SkinnedISMComp = NewObject<UArcEditorInstancedSkinnedMeshComponent>(this);
			SkinnedISMComp->SetSkinnedAssetAndUpdate(SkinnedMeshTrait->GetSkinnedAsset());
			if (SkinnedMeshTrait->GetTransformProvider() != nullptr)
			{
				SkinnedISMComp->SetTransformProvider(SkinnedMeshTrait->GetTransformProvider());
			}
			SkinnedISMComp->bIsEditorOnly = true;
			SkinnedISMComp->SetCanEverAffectNavigation(false);
			SkinnedISMComp->bHasPerInstanceHitProxies = true;

			const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = SkinnedMeshTrait->GetMaterialOverrides();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialOverrides.Num(); ++MaterialIndex)
			{
				if (MaterialOverrides[MaterialIndex] != nullptr)
				{
					SkinnedISMComp->SetMaterial(MaterialIndex, MaterialOverrides[MaterialIndex]);
				}
			}

			SkinnedISMComp->SetupAttachment(GetRootComponent());
			SkinnedISMComp->RegisterComponent();
			AddInstanceComponent(SkinnedISMComp);

			const FTransform& CompTransform = SkinnedMeshTrait->GetComponentTransform();
			for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
			{
				FTransform FinalTransform = CompTransform * InstanceTransform;
				SkinnedISMComp->AddInstance(FinalTransform, /*AnimationIndex=*/0, /*bWorldSpace=*/false);
			}

			Group.SkinnedMeshComponents.Add(SkinnedISMComp);
			Group.SkinnedMeshRelativeTransforms.Add(CompTransform);
		}
	}

	// Collision draw component
	if (CollisionDrawComponent == nullptr)
	{
		CollisionDrawComponent = NewObject<UArcPlacedEntityCollisionDrawComponent>(this);
		CollisionDrawComponent->bIsEditorOnly = true;
		CollisionDrawComponent->SetupAttachment(GetRootComponent());
		CollisionDrawComponent->RegisterComponent();
		AddInstanceComponent(CollisionDrawComponent);
	}
	CollisionDrawComponent->Populate(Entries);
}

void AArcPlacedEntityPartitionActor::DestroyEditorPreviewISMCs()
{
	if (CollisionDrawComponent != nullptr)
	{
		CollisionDrawComponent->DestroyComponent();
		CollisionDrawComponent = nullptr;
	}

	for (FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
	{
		for (UNiagaraComponent* NiagaraComp : Group.NiagaraComponents)
		{
			if (NiagaraComp != nullptr)
			{
				NiagaraComp->Deactivate();
				NiagaraComp->DestroyComponent();
			}
		}
		Group.NiagaraComponents.Empty();
	}

	for (FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
	{
		for (UInstancedSkinnedMeshComponent* SkinnedComp : Group.SkinnedMeshComponents)
		{
			if (SkinnedComp != nullptr)
			{
				const int32 InstanceCount = SkinnedComp->GetInstanceCount();
				for (int32 Idx = 0; Idx < InstanceCount; ++Idx)
				{
					FPrimitiveInstanceId PrimId = SkinnedComp->GetInstanceId(Idx);
					if (PrimId.IsValid())
					{
						FSkMInstanceElementId ElementId;
						ElementId.SkMComponent = SkinnedComp;
						ElementId.InstanceId = PrimId.GetAsIndex();
						ArcSkMInstanceElementUtils::DestroyEditorSkMInstanceElement(ElementId);
					}
				}
				SkinnedComp->DestroyComponent();
			}
		}
		Group.SkinnedMeshComponents.Empty();
		Group.SkinnedMeshRelativeTransforms.Empty();
	}

	TArray<UInstancedSkinnedMeshComponent*> AllSkinnedComponents;
	GetComponents(AllSkinnedComponents);
	for (UInstancedSkinnedMeshComponent* SkinnedComp : AllSkinnedComponents)
	{
		if (SkinnedComp->bIsEditorOnly)
		{
			SkinnedComp->DestroyComponent();
		}
	}

	TArray<UInstancedStaticMeshComponent*> ISMComponents;
	GetComponents(ISMComponents);
	for (UInstancedStaticMeshComponent* ISMComponent : ISMComponents)
	{
		ISMComponent->DestroyComponent();
	}

	EditorPreviewGroups.Empty();
}

void AArcPlacedEntityPartitionActor::RebuildGuidIndex()
{
	GuidToInstanceMap.Reset();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		for (const TPair<int32, FArcPlacedEntityFragmentOverrides>& OverridePair : Entry.PerInstanceOverrides)
		{
			for (const FInstancedStruct& FragmentValue : OverridePair.Value.FragmentValues)
			{
				if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
				{
					const FArcLinkableGuidFragment& GuidFrag = FragmentValue.Get<const FArcLinkableGuidFragment>();
					if (GuidFrag.LinkGuid.IsValid())
					{
						GuidToInstanceMap.Add(GuidFrag.LinkGuid, TPair<int32, int32>(EntryIndex, OverridePair.Key));
					}
					break;
				}
			}
		}
	}
}

bool AArcPlacedEntityPartitionActor::FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	const TPair<int32, int32>* Found = GuidToInstanceMap.Find(Guid);
	if (Found == nullptr)
	{
		return false;
	}
	OutEntryIndex = Found->Key;
	OutTransformIndex = Found->Value;
	return true;
}

bool AArcPlacedEntityPartitionActor::ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	UInstancedStaticMeshComponent* Component = InstanceId.ISMComponent;
	if (Component == nullptr)
	{
		return false;
	}

	for (int32 GroupIndex = 0; GroupIndex < EditorPreviewGroups.Num(); ++GroupIndex)
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[GroupIndex];
		for (const TObjectPtr<UInstancedStaticMeshComponent>& ISMComp : Group.ISMComponents)
		{
			if (ISMComp == Component)
			{
				if (!Entries.IsValidIndex(GroupIndex))
				{
					return false;
				}

				int32 InstanceIndex = InstanceId.InstanceIndex;
				if (!Entries[GroupIndex].InstanceTransforms.IsValidIndex(InstanceIndex))
				{
					return false;
				}

				OutEntryIndex = GroupIndex;
				OutTransformIndex = InstanceIndex;
				return true;
			}
		}
	}

	return false;
}

bool AArcPlacedEntityPartitionActor::ResolveSkMInstanceId(const FSkMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	UInstancedSkinnedMeshComponent* Component = InstanceId.SkMComponent;
	if (Component == nullptr)
	{
		return false;
	}

	for (int32 GroupIndex = 0; GroupIndex < EditorPreviewGroups.Num(); ++GroupIndex)
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[GroupIndex];
		for (const TObjectPtr<UInstancedSkinnedMeshComponent>& SkMComp : Group.SkinnedMeshComponents)
		{
			if (SkMComp == Component)
			{
				if (!Entries.IsValidIndex(GroupIndex))
				{
					return false;
				}

				// InstanceId.InstanceId is the FPrimitiveInstanceId value — use it directly as transform index
				int32 TransformIndex = InstanceId.InstanceId;
				if (!Entries[GroupIndex].InstanceTransforms.IsValidIndex(TransformIndex))
				{
					return false;
				}

				OutEntryIndex = GroupIndex;
				OutTransformIndex = TransformIndex;
				return true;
			}
		}
	}

	return false;
}

UInstancedSkinnedMeshComponent* AArcPlacedEntityPartitionActor::GetEditorPreviewSkMC(int32 EntryIndex) const
{
	if (!EditorPreviewGroups.IsValidIndex(EntryIndex) || EditorPreviewGroups[EntryIndex].SkinnedMeshComponents.Num() == 0)
	{
		return nullptr;
	}
	return EditorPreviewGroups[EntryIndex].SkinnedMeshComponents[0];
}

#endif // WITH_EDITOR
