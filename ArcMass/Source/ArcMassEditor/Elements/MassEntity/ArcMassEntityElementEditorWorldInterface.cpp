// Copyright Lukasz Baran. All Rights Reserved.

#include "Elements/MassEntity/ArcMassEntityElementEditorWorldInterface.h"

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"

#include "Elements/Framework/TypedElementSelectionSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityElementEditorWorldInterface)

bool UArcMassEntityElementEditorWorldInterface::CanDeleteElement(const FTypedElementHandle& InElementHandle)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.CanDeleteMassEntityInstance();
}

bool UArcMassEntityElementEditorWorldInterface::DeleteElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions)
{
	// Batch deletion by manager to avoid per-element overhead
	TMap<IArcMassEntityManager*, TArray<FArcMassEntityElementId>> BatchedInstancesToDelete;
	for (const FTypedElementHandle& ElementHandle : InElementHandles)
	{
		const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(ElementHandle);
		if (MassEntity && MassEntity.CanDeleteMassEntityInstance())
		{
			// Deselect before the element is destroyed. DeleteMassEntityInstances tears down and
			// rebuilds the render state, which calls Registry->DestroyElement on these handles —
			// resetting their internal type ID to 0. A still-selected handle then lingers in the
			// selection set as a stale (DataPtr!=null, TypeId==0) entry and trips
			// UTypedElementRegistry::GetElementImpl's "type ID not registered" ensure on the next
			// viewport interaction. Mirrors USMInstanceElementEditorWorldInterface::DeleteElements.
			if (InSelectionSet)
			{
				InSelectionSet->DeselectElement(ElementHandle, FTypedElementSelectionOptions());
			}

			TArray<FArcMassEntityElementId>& InstanceIds = BatchedInstancesToDelete.FindOrAdd(MassEntity.GetInstanceManager());
			InstanceIds.Add(MassEntity.GetInstanceId());
		}
	}

	bool bDidDelete = false;
	for (TTuple<IArcMassEntityManager*, TArray<FArcMassEntityElementId>>& BatchPair : BatchedInstancesToDelete)
	{
		bDidDelete |= BatchPair.Key->DeleteMassEntityInstances(BatchPair.Value);
	}

	return bDidDelete;
}

bool UArcMassEntityElementEditorWorldInterface::CanDuplicateElement(const FTypedElementHandle& InElementHandle)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.CanDuplicateMassEntityInstance();
}

void UArcMassEntityElementEditorWorldInterface::DuplicateElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements)
{
	// Batch duplication by manager
	TMap<IArcMassEntityManager*, TArray<FArcMassEntityElementId>> BatchedInstancesToDuplicate;
	for (const FTypedElementHandle& ElementHandle : InElementHandles)
	{
		const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(ElementHandle);
		if (MassEntity && MassEntity.CanDuplicateMassEntityInstance())
		{
			TArray<FArcMassEntityElementId>& InstanceIds = BatchedInstancesToDuplicate.FindOrAdd(MassEntity.GetInstanceManager());
			InstanceIds.Add(MassEntity.GetInstanceId());
		}
	}

	const bool bOffsetIsZero = InLocationOffset.IsZero();

	for (const TTuple<IArcMassEntityManager*, TArray<FArcMassEntityElementId>>& BatchPair : BatchedInstancesToDuplicate)
	{
		TArray<FArcMassEntityElementId> NewInstanceIds;
		if (BatchPair.Key->DuplicateMassEntityInstances(BatchPair.Value, NewInstanceIds))
		{
			OutNewElements.Reserve(OutNewElements.Num() + NewInstanceIds.Num());
			for (const FArcMassEntityElementId& NewInstanceId : NewInstanceIds)
			{
				if (!bOffsetIsZero)
				{
					FTransform NewInstanceTransform = FTransform::Identity;
					BatchPair.Key->GetMassEntityInstanceTransform(NewInstanceId, NewInstanceTransform, /*bWorldSpace*/false);
					NewInstanceTransform.SetTranslation(NewInstanceTransform.GetTranslation() + InLocationOffset);
					BatchPair.Key->SetMassEntityInstanceTransform(NewInstanceId, NewInstanceTransform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);
				}

				FTypedElementHandle NewHandle = ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(NewInstanceId);
				if (NewHandle)
				{
					OutNewElements.Add(NewHandle);
				}
			}
		}
	}
}
