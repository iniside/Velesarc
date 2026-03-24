// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcIWTypes.generated.h"

class APartitionActor;
struct FArcMassPhysicsLinkFragment;
class UStaticMesh;
class UMaterialInterface;
class UMassEntityConfigAsset;

// ---------------------------------------------------------------------------
// Serialized Types (stored in partition actor)
// ---------------------------------------------------------------------------

/** One mesh component captured from a source actor. */
USTRUCT(BlueprintType)
struct ARCINSTANCEDWORLD_API FArcIWMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	/** Transform of this mesh relative to the actor root. */
	UPROPERTY(VisibleAnywhere)
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere)
	bool bCastShadows = false;
};

/** Per-actor-class data stored in the partition actor. */
USTRUCT(BlueprintType)
struct ARCINSTANCEDWORLD_API FArcIWActorClassData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TSubclassOf<AActor> ActorClass;

	/** Composite mesh snapshot — shared by all instances of this class. */
	UPROPERTY(VisibleAnywhere)
	TArray<FArcIWMeshEntry> MeshDescriptors;

	/** World transforms of all instances. */
	UPROPERTY(VisibleAnywhere)
	TArray<FTransform> InstanceTransforms;

	/** Optional Mass entity config from the actor's component. Merged into the entity template. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMassEntityConfigAsset> AdditionalEntityConfig = nullptr;
};

// ---------------------------------------------------------------------------
// Mass Fragments (runtime)
// ---------------------------------------------------------------------------

/** Const shared fragment — one per actor class archetype. */
USTRUCT(BlueprintType)
struct ARCINSTANCEDWORLD_API FArcIWVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "ArcIW")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "ArcIW")
	TArray<FArcIWMeshEntry> MeshDescriptors;
};

template<>
struct TMassFragmentTraits<FArcIWVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Per-entity mutable visualization state. */
USTRUCT()
struct ARCINSTANCEDWORLD_API FArcIWInstanceFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Visualization grid cell. */
	FIntVector GridCoords = FIntVector::ZeroValue;

	/** Flat index into the partition actor's SpawnedEntities. Used as the IActorInstanceManagerInterface instance index. */
	int32 InstanceIndex = INDEX_NONE;

	/** Base index into the partition actor's ISMHolderEntities for this entity's class.
	 *  ISMHolderEntities[MeshSlotBase + MeshIdx] gives the ISM holder entity for that mesh slot. */
	int32 MeshSlotBase = INDEX_NONE;

	/** Owning partition actor. Set during entity spawn, used by processors to perform partition operations directly. */
	TWeakObjectPtr<APartitionActor> PartitionActor;

	/** One ISM instance ID per mesh entry. INDEX_NONE when not instanced. */
	TArray<int32> ISMInstanceIds;

	/** True when represented by a spawned actor instead of ISM. */
	bool bIsActorRepresentation = false;

	/** Weak reference to the hydrated actor (valid only when bIsActorRepresentation is true). */
	TWeakObjectPtr<AActor> HydratedActor;

	/** When true, the dehydration processor will not return this entity to ISM.
	 *  Set by gameplay code (via UArcIWEntityComponent) during active interactions.
	 *  Cleared when the interaction ends. */
	bool bKeepHydrated = false;
};

template<>
struct TMassFragmentTraits<FArcIWInstanceFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Tag to identify ArcIW entities. */
USTRUCT()
struct ARCINSTANCEDWORLD_API FArcIWEntityTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcIW::Signals
{
	inline const FName ActorCellActivated = FName(TEXT("ArcIWActorCellActivated"));
	inline const FName ActorCellDeactivated = FName(TEXT("ArcIWActorCellDeactivated"));
	inline const FName MeshCellActivated = FName(TEXT("ArcIWMeshCellActivated"));
	inline const FName MeshCellDeactivated = FName(TEXT("ArcIWMeshCellDeactivated"));
	inline const FName PhysicsCellActivated = FName(TEXT("ArcIWPhysicsCellActivated"));
	inline const FName PhysicsCellDeactivated = FName(TEXT("ArcIWPhysicsCellDeactivated"));
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

namespace UE::ArcIW
{
	/** Detach all physics link entries (clear Chaos user data binding). */
	void DetachPhysicsLinkEntries(FArcMassPhysicsLinkFragment& LinkFragment);
}
