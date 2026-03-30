// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Components/ArcIWISMComponent.h"
#include "ArcInstancedWorld/ArcIWTypes.h"

int32 UArcIWISMComponent::AddTrackedInstance(const FTransform& WorldTransform, int32 MeshEntryIndex, int32 InInstanceIndex)
{
	const int32 NewIndex = AddInstance(WorldTransform, /*bWorldSpace=*/true);

	if (Owners.Num() <= NewIndex)
	{
		Owners.SetNum(NewIndex + 1);
	}
	Owners[NewIndex] = FInstanceOwnerEntry{MeshEntryIndex, InInstanceIndex};

	return NewIndex;
}

void UArcIWISMComponent::RemoveTrackedInstance(int32 InstanceId, FMassEntityManager& EntityManager)
{
	const int32 InstanceCount = GetInstanceCount();
	if (InstanceId >= InstanceCount)
	{
		return;
	}

	const int32 LastIndex = InstanceCount - 1;
	if (InstanceId != LastIndex && LastIndex >= 0 && SpawnedEntities)
	{
		const FInstanceOwnerEntry SwappedOwner = Owners[LastIndex];
		const FMassEntityHandle SwappedEntity = (*SpawnedEntities)[SwappedOwner.InstanceIndex];
		const int32 SwappedMeshEntryIndex = SwappedOwner.MeshEntryIndex;

		if (EntityManager.IsEntityValid(SwappedEntity))
		{
			FArcIWInstanceFragment* SwappedFragment =
				EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(SwappedEntity);
			if (SwappedFragment
				&& !SwappedFragment->bIsActorRepresentation
				&& SwappedMeshEntryIndex >= 0
				&& SwappedMeshEntryIndex < SwappedFragment->ISMInstanceIds.Num())
			{
				SwappedFragment->ISMInstanceIds[SwappedMeshEntryIndex] = InstanceId;
			}
		}

		Owners[InstanceId] = SwappedOwner;
	}

	if (Owners.IsValidIndex(LastIndex))
	{
		Owners.RemoveAt(LastIndex);
	}

	RemoveInstance(InstanceId);
}

const UArcIWISMComponent::FInstanceOwnerEntry* UArcIWISMComponent::ResolveOwner(int32 InstanceIndex) const
{
	return Owners.IsValidIndex(InstanceIndex) ? &Owners[InstanceIndex] : nullptr;
}
