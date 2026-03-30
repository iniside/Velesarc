// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcIWTypes.generated.h"

class UPCGComponent;
class APartitionActor;
class UStaticMesh;
class USkinnedAsset;
class UTransformProviderData;
class UMaterialInterface;
class UMassEntityConfigAsset;
class UBodySetup;

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

	/** Skinned mesh asset. If set, Mesh is ignored and the skinned path is used. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkinnedAsset> SkinnedAsset = nullptr;

	/** Transform provider for GPU animation (wind sway, looping idle). Only used with SkinnedAsset. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTransformProviderData> TransformProvider = nullptr;

	/** Default animation index for instances of this mesh. */
	UPROPERTY(VisibleAnywhere)
	uint32 DefaultAnimationIndex = 0;

	bool IsSkinned() const { return SkinnedAsset != nullptr; }
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

	/** Merged collision body setup from collision-enabled components. Null if actor has no collision. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBodySetup> CollisionBodySetup = nullptr;

	/** Collision profile to apply to the physics body. Copied from root primitive component. */
	UPROPERTY(VisibleAnywhere)
	FCollisionProfileName CollisionProfile;

	/** Full physics body template from root primitive. Carries damping, mass, DOF, etc. */
	UPROPERTY(VisibleAnywhere)
	FBodyInstance BodyTemplate;

	/** Time in seconds before a removed instance respawns. 0 = never respawn. */
	UPROPERTY(VisibleAnywhere)
	int32 RespawnTimeSeconds = 0;

	/** Physics body creation mode. Static = trace-only, Dynamic = can respond to forces. */
	UPROPERTY(VisibleAnywhere)
	EArcMassPhysicsBodyType PhysicsBodyType = EArcMassPhysicsBodyType::Static;

	/** True if this entry was created by a PCG component. Used to detect stale entries. */
	UPROPERTY()
	bool bFromPCG = false;

	/** PCG component that created this entry. Null for non-PCG entries (ConvertActorsToPartition).
	 *  Used to detect stale entries when the PCG volume is deleted or regenerated. */
	UPROPERTY()
	TWeakObjectPtr<UPCGComponent> SourcePCGComponent;
};

/** Tracks which instances of a single actor class have been removed at runtime.
 *  Parallel to FArcIWActorClassData — same array index means same class. */
USTRUCT()
struct ARCINSTANCEDWORLD_API FArcIWClassRemovals
{
	GENERATED_BODY()

	/** Indices into InstanceTransforms that have been removed. */
	UPROPERTY(SaveGame)
	TSet<int32> RemovedInstances;
};

/** Queued respawn entry. Sorted ascending by RespawnAtUtc (append-only — chronological removals). */
USTRUCT()
struct ARCINSTANCEDWORLD_API FArcIWPendingRespawn
{
	GENERATED_BODY()

	/** UTC Unix timestamp (seconds) at which this entity should respawn. */
	UPROPERTY(SaveGame)
	int64 RespawnAtUtc = 0;

	UPROPERTY(SaveGame)
	int32 ClassIndex = INDEX_NONE;

	UPROPERTY(SaveGame)
	int32 TransformIndex = INDEX_NONE;
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

	/** Mesh-domain grid cell. */
	FIntVector MeshGridCoords = FIntVector::ZeroValue;

	/** Physics-domain grid cell. */
	FIntVector PhysicsGridCoords = FIntVector::ZeroValue;

	/** Actor-domain grid cell. */
	FIntVector ActorGridCoords = FIntVector::ZeroValue;

	/** Flat index into the partition actor's SpawnedEntities. Used as the IActorInstanceManagerInterface instance index. */
	int32 InstanceIndex = INDEX_NONE;

	/** Base index into the partition actor's ISMHolderEntities for this entity's class.
	 *  ISMHolderEntities[MeshSlotBase + MeshIdx] gives the ISM holder entity for that mesh slot. */
	int32 MeshSlotBase = INDEX_NONE;

	/** Index into the partition actor's ActorClassEntries. */
	int32 ClassIndex = INDEX_NONE;

	/** Index into ActorClassEntries[ClassIndex].InstanceTransforms — the original transform slot.
	 *  Stable across save/load; used by persistence to identify individual instances. */
	int32 TransformIndex = INDEX_NONE;

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

/** Tag to identify ArcIW simple visualization entities (single-mesh, rendered directly). */
USTRUCT()
struct ARCINSTANCEDWORLD_API FArcIWSimpleVisEntityTag : public FMassTag
{
	GENERATED_BODY()
};

/** Tag to identify ArcIW simple visualization entities using skinned meshes. */
USTRUCT()
struct ARCINSTANCEDWORLD_API FArcIWSimpleVisSkinnedTag : public FMassTag
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
}

