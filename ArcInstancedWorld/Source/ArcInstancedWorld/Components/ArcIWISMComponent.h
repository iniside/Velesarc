// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/EntityHandle.h"
#include "MassEntityManager.h"
#include "ArcIWISMComponent.generated.h"

struct FArcIWInstanceFragment;

/**
 * ISM component subclass that stores per-slot owner tracking for ArcIW.
 * Encapsulates add/remove with swap-fixup and trace resolution.
 */
UCLASS()
class ARCINSTANCEDWORLD_API UArcIWISMComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	/** Tracks which entity owns a particular ISM instance slot. */
	struct FInstanceOwnerEntry
	{
		int32 MeshEntryIndex = INDEX_NONE;
		int32 InstanceIndex = INDEX_NONE;  // flat index into SpawnedEntities
	};

	/** Index into the partition actor's mesh slot array. */
	int32 MeshSlotIndex = INDEX_NONE;

	/** Back-pointer to partition actor's SpawnedEntities for swap-fixup entity derivation. */
	const TArray<FMassEntityHandle>* SpawnedEntities = nullptr;

	/** Add an ISM instance and track ownership. Returns ISM instance ID. */
	int32 AddTrackedInstance(const FTransform& WorldTransform, int32 MeshEntryIndex, int32 InInstanceIndex);

	/** Remove an ISM instance with swap-fixup. Updates swapped entity's ISMInstanceIds. */
	void RemoveTrackedInstance(int32 InstanceId, FMassEntityManager& EntityManager);

	/** Bounds-checked access to owner entry. Returns nullptr if out of range. */
	const FInstanceOwnerEntry* ResolveOwner(int32 InstanceIndex) const;

	/** Mirrors the ISM instances array — one owner entry per ISM instance. */
	TArray<FInstanceOwnerEntry> Owners;
};
