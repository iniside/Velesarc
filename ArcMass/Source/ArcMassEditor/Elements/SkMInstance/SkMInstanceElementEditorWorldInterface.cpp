// Copyright Lukasz Baran. All Rights Reserved.

#include "Elements/SkMInstance/SkMInstanceElementEditorWorldInterface.h"

#include "ArcMass/Elements/SkMInstance/ArcSkMInstanceElementUtils.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkMInstanceElementEditorWorldInterface)

bool USkMInstanceElementEditorWorldInterface::CanDeleteElement(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.CanDeleteSkMInstance();
}

bool USkMInstanceElementEditorWorldInterface::DeleteElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions)
{
	// Batch deletion by manager to avoid per-element overhead
	TMap<ISkMInstanceManager*, TArray<FSkMInstanceId>> BatchedInstancesToDelete;
	for (const FTypedElementHandle& ElementHandle : InElementHandles)
	{
		const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(ElementHandle);
		if (SkMInstance && SkMInstance.CanDeleteSkMInstance())
		{
			TArray<FSkMInstanceId>& InstanceIds = BatchedInstancesToDelete.FindOrAdd(SkMInstance.GetInstanceManager());
			InstanceIds.Add(SkMInstance.GetInstanceId());
		}
	}

	bool bDidDelete = false;
	for (TTuple<ISkMInstanceManager*, TArray<FSkMInstanceId>>& BatchPair : BatchedInstancesToDelete)
	{
		bDidDelete |= BatchPair.Key->DeleteSkMInstances(BatchPair.Value);
	}

	return bDidDelete;
}

bool USkMInstanceElementEditorWorldInterface::CanDuplicateElement(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.CanDuplicateSkMInstance();
}

void USkMInstanceElementEditorWorldInterface::DuplicateElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements)
{
	// Batch duplication by manager
	TMap<ISkMInstanceManager*, TArray<FSkMInstanceId>> BatchedInstancesToDuplicate;
	for (const FTypedElementHandle& ElementHandle : InElementHandles)
	{
		const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(ElementHandle);
		if (SkMInstance && SkMInstance.CanDuplicateSkMInstance())
		{
			TArray<FSkMInstanceId>& InstanceIds = BatchedInstancesToDuplicate.FindOrAdd(SkMInstance.GetInstanceManager());
			InstanceIds.Add(SkMInstance.GetInstanceId());
		}
	}

	const bool bOffsetIsZero = InLocationOffset.IsZero();

	for (const TTuple<ISkMInstanceManager*, TArray<FSkMInstanceId>>& BatchPair : BatchedInstancesToDuplicate)
	{
		TArray<FSkMInstanceId> NewInstanceIds;
		if (BatchPair.Key->DuplicateSkMInstances(BatchPair.Value, NewInstanceIds))
		{
			OutNewElements.Reserve(OutNewElements.Num() + NewInstanceIds.Num());
			for (const FSkMInstanceId& NewInstanceId : NewInstanceIds)
			{
				if (!bOffsetIsZero)
				{
					FTransform NewInstanceTransform = FTransform::Identity;
					BatchPair.Key->GetSkMInstanceTransform(NewInstanceId, NewInstanceTransform, /*bWorldSpace*/false);
					NewInstanceTransform.SetTranslation(NewInstanceTransform.GetTranslation() + InLocationOffset);
					BatchPair.Key->SetSkMInstanceTransform(NewInstanceId, NewInstanceTransform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);
				}

				FTypedElementHandle NewHandle = ArcSkMInstanceElementUtils::AcquireEditorSkMInstanceElementHandle(NewInstanceId);
				if (NewHandle)
				{
					OutNewElements.Add(NewHandle);
				}
			}
		}
	}
}
