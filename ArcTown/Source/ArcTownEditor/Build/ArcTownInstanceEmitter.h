// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/ArcTownLayout.h"
#include "Town/ArcTownVolume.h"
#include "WorldPartition/WorldPartitionHandle.h"

class AArcPlacedEntityPartitionActor;
class AArcTerrain;
class AArcTownPartitionActor;
class UArcMassEntityConfigAsset;
class UArcTownPathComponent;
class UInstancedStaticMeshComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class UWorldPartition;

/** One flattened pad a rebuild will lay under a building, in world space. */
struct ARCTOWNEDITOR_API FArcTownPadSpec
{
	/** Centre of the building's floor plane, the height the pad conforms the terrain to. */
	FVector WorldLocation = FVector::ZeroVector;
	/** The building's own yaw: the pad rectangle is its footprint, not an axis-aligned box around it. */
	FQuat WorldRotation = FQuat::Identity;
	FVector2D CoverageXY = FVector2D::ZeroVector;
	FGuid ContentId;
};

struct ARCTOWNEDITOR_API FArcTownRoadModifierSpec
{
	TArray<TObjectPtr<UArcTownPathComponent>> Paths;
	FVector WorldLocation = FVector::ZeroVector;
	FQuat WorldRotation = FQuat::Identity;
	FVector2D CoverageXY = FVector2D::ZeroVector;
};

/** What to spawn and where, not a spawn: a spawned actor is registered and visible, so the spawn belongs to adoption. */
struct ARCTOWNEDITOR_API FArcTownPreparedBuildingActor
{
	TObjectPtr<UClass> ActorClass;
	FTransform WorldXform;
};

/** The candidate components of one partition actor, outered to it and not yet registered. */
struct ARCTOWNEDITOR_API FArcTownPreparedActor
{
	TObjectPtr<AArcTownPartitionActor> Actor;
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;
	TArray<FName> FinalNames;
	TArray<TObjectPtr<USplineMeshComponent>> RoadComponents;
	TArray<FName> RoadFinalNames;
	TArray<TObjectPtr<UStaticMeshComponent>> RoadNodeComponents;
	TArray<FName> RoadNodeFinalNames;
	TArray<FArcTownPreparedBuildingActor> BuildingActors;
};

struct ARCTOWNEDITOR_API FArcTownPreparedEntityWrite
{
	TObjectPtr<AArcPlacedEntityPartitionActor> Actor;
	TObjectPtr<UArcMassEntityConfigAsset> Config;
	TArray<FTransform> RelativeTransforms;
};

struct ARCTOWNEDITOR_API FArcTownFailedBuildRecords
{
	TOptional<TArray<FArcTownEmittedActor>> EntityHosts;

	bool IsEmpty() const { return !EntityHosts.IsSet(); }
};

/** Complete, unregistered candidate set of one rebuild. Nothing in it is visible until it is adopted. */
struct ARCTOWNEDITOR_API FArcTownPreparedInstances
{
	TArray<FArcTownPreparedActor> Actors;
	/** Partition actors this town owned and this rebuild emptied. */
	TArray<TSoftObjectPtr<AActor>> ActorsToDestroy;
	/** Partition actors this rebuild spawned; a discard destroys them, so no empty actor outlives a failure. */
	TArray<TObjectPtr<AActor>> CreatedActors;
	/** Building actors the adoption spawned and no partition actor owns yet; a discard destroys them. */
	TArray<TObjectPtr<AActor>> SpawnedBuildingActors;
	/** Replacement for AArcTownVolume::EmittedActors, written when the destroys run. */
	TArray<FArcTownEmittedActor> ActorRecords;
	TArray<FArcTownEmittedActor> EntityHostRecords;
	TArray<TObjectPtr<AArcPlacedEntityPartitionActor>> PreviousEntityHosts;
	TArray<FArcTownPreparedEntityWrite> EntityWrites;
	FArcTownFailedBuildRecords FailedBuild;
	/** Holds the previously owned actors loaded; releasing one unregisters the actor it names. */
	TArray<FWorldPartitionReference> PinnedActors;
	/** One per placed building, laid on the volume when the instances commit. */
	TArray<FArcTownPadSpec> PadSpecs;
	FArcTownRoadModifierSpec RoadSpec;
	TArray<FArcTownGeneratedJunction> Junctions;
	/** Terrain the pads composite into, resolved from the site snapshot's terrain guid. */
	TSoftObjectPtr<AArcTerrain> PadTerrain;
	int32 BuildingCount = 0;
	int32 CellCount = 0;
	int32 RoadSplineCount = 0;
};

/** Game-thread-only translation of a town's simulated settlement into standard engine ISM components. */
class ARCTOWNEDITOR_API FArcTownInstanceEmitter
{
public:
	static FGuid MakeEntityHostGridGuid(const FGuid& TownId);

	static void CollectEntityHostDescGuids(
		UWorldPartition* WorldPartition,
		const FGuid& EntityHostGridGuid,
		TArray<FGuid>& OutGuids);

	static bool ResolveGeneratedPaths(
		const AArcTownVolume& Town,
		TArray<FArcTownGeneratedPath>& OutPaths,
		TArray<FArcTownGeneratedJunction>& OutJunctions,
		FText& OutError);

	/**
	 * Builds the town's candidate components without touching the scene. A failure discards its own
	 * partial state, including any partition actor it had to spawn.
	 */
	static bool Prepare(AArcTownVolume& Town, FArcTownPreparedInstances& OutPrepared, FText& OutError);

	/**
	 * Writes the town's owned-actor records, replaces its pad stamps, destroys the actors this rebuild
	 * emptied and releases the pins. Call inside the caller's transaction and only after a successful
	 * Adopt: undo has to restore the records and those actors together, both writes need the pinned
	 * actors still loaded, and a failed adoption must leave the previous pads standing.
	 */
	static void CommitEmittedActorChanges(AArcTownVolume& Town, FArcTownPreparedInstances& Prepared);

	/** Adopts every prepared per-actor component set. Discards on failure; the pins stay held for the commit. */
	static bool Adopt(AArcTownVolume& Town, FArcTownPreparedInstances& Prepared, FText& OutError);

	/**
	 * Destroys the candidates which were not adopted and the partition actors this rebuild spawned,
	 * leaving every pre-existing actor's saved set untouched.
	 */
	static void Discard(FArcTownPreparedInstances& Prepared);
};
