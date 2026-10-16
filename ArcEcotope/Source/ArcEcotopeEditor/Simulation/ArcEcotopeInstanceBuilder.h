// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Terrain/ArcEcotopeInstanceRaster.h"
#include "UObject/StrongObjectPtr.h"
#include "WorldPartition/WorldPartitionHandle.h"

class AArcEcotopeInstancePartitionActor;
class AArcEcotopeVolume;
class AArcPlacedEntityPartitionActor;
class UArcEcotopeSpeciesModel;
class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
struct FArcForestInstanceScale;
struct FArcForestWorldSnapshot;

/** The candidate components of one instance partition actor, outered to it and not yet registered. */
struct ARCECOTOPEEDITOR_API FArcEcotopePreparedInstanceActor
{
	TObjectPtr<AArcEcotopeInstancePartitionActor> Actor;
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> StaticComponents;
	TArray<FName> StaticFinalNames;
	TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedComponents;
	TArray<FName> SkinnedFinalNames;
};

/** One batched placed-entity append, held back until the caller's transaction is open. */
struct ARCECOTOPEEDITOR_API FArcEcotopePreparedEntityWrite
{
	TObjectPtr<AArcPlacedEntityPartitionActor> Actor;
	/**
	 * Strong: the config was loaded off a soft path during Build and nothing else holds it until the commit.
	 * Erased to UObject because ArcMass is a private dependency and the concrete type is incomplete here.
	 */
	TStrongObjectPtr<UObject> Config;
	TArray<FTransform> RelativeTransforms;
};

/** What a failed bake still owns. Each entry is set only by the phase that reached its actors. */
struct ARCECOTOPEEDITOR_API FArcEcotopeFailedBakeRecords
{
	TOptional<TArray<FArcEcotopeEmittedActor>> EntityActors;
	TOptional<TArray<FArcEcotopeEmittedActor>> InstanceActors;

	bool IsEmpty() const { return !EntityActors.IsSet() && !InstanceActors.IsSet(); }
};

/** Complete, unregistered native-component candidate. It has no rendering side effects until adopted. */
struct ARCECOTOPEEDITOR_API FArcEcotopePreparedInstances
{
	/** One entry per partition actor the bake writes geometry to. */
	TArray<FArcEcotopePreparedInstanceActor> InstanceActors;
	/** Partition actors the bake emptied and no longer owns, of either kind. */
	TArray<TSoftObjectPtr<AActor>> ActorsToDestroy;
	/** Replacement for AArcEcotopeVolume::EmittedEntityActors, written when the destroy runs. */
	TArray<FArcEcotopeEmittedActor> EntityActorRecords;
	/** Replacement for AArcEcotopeVolume::InstancePartitionActors, written when the destroy runs. */
	TArray<FArcEcotopeEmittedActor> InstanceActorRecords;
	/** Placed-entity actors the volume owned before this bake; their entries are cleared at commit time. */
	TArray<TObjectPtr<AArcPlacedEntityPartitionActor>> PreviousEntityActors;
	/** Applied after the previous entries are cleared, so a re-bake never doubles a cell. */
	TArray<FArcEcotopePreparedEntityWrite> EntityWrites;
	/** Survives Discard: the caller writes it after a failed Build. */
	FArcEcotopeFailedBakeRecords FailedBake;
#if WITH_EDITOR
	/** Holds the previously owned actors loaded; releasing one unregisters the actor it names. */
	TArray<FWorldPartitionReference> PinnedActors;
#endif
	/** Volume-local discs of every placed mesh instance, in the frame the weight raster rasterizes. */
	TArray<FArcEcotopeInstanceFootprint> WeightFootprints;
	uint64 SnapshotHash = 0;
	uint64 VisualHash = 0;
};

/** Game-thread-only translation of an authoritative forest snapshot into standard engine ISM/ISKM components. */
class ARCECOTOPEEDITOR_API FArcEcotopeInstanceBuilder
{
public:
	static bool Build(
		AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FArcEcotopePreparedInstances& OutPrepared,
		FText& OutError);

	/** Writes what a failed Build still owns, in its own transaction. Does nothing when the build owned nothing. */
	static void CommitFailedBakeRecords(AArcEcotopeVolume& Volume, FArcEcotopePreparedInstances& Prepared);

	/**
	 * Writes the volume's owned-actor records, applies the prepared placed-entity writes and destroys the
	 * actors the build emptied. Call inside the caller's transaction, after Volume.Modify(): undo must
	 * restore the records, the entity entries and the actors they stopped naming together.
	 * AdoptPreparedInstances must follow, which is what releases the pins.
	 */
	static void CommitEmittedActorChanges(AArcEcotopeVolume& Volume, FArcEcotopePreparedInstances& Prepared);

	/**
	 * Drops everything a volume's bake owns: its instance partition actors, its entries on the placed-entity
	 * actors it shares, and the recorded representation. Same seam as CommitEmittedActorChanges — call it
	 * inside the caller's transaction, after Volume.Modify(). Returns false and describes every record it
	 * could not resolve or that names another owner; those records stay on the volume.
	 */
	static bool ClearEmittedActors(AArcEcotopeVolume& Volume, bool bRequiresRebuild, FText& OutError);

	/**
	 * Adopts every prepared per-actor component set, records the representation on the volume, paints the
	 * volume's terrain weight stamp from the prepared footprints, then releases the pins.
	 */
	static bool AdoptPreparedInstances(
		AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FArcEcotopePreparedInstances& Prepared,
		FText& OutError);

	/** Destroys a candidate which was not adopted, leaving the owning volume's saved set untouched. */
	static void Discard(FArcEcotopePreparedInstances& Prepared);
};

namespace ArcEcotopeInstanceBuilder
{
	/**
	 * Uniform scale of one instance. FromHeight runs Alpha across the bounds of the band SizeCm
	 * selected, so an author sizes a band against itself; a zero-width band yields MinScale.
	 * FromAge and Random ignore both bounds.
	 */
	ARCECOTOPEEDITOR_API double MakeScale(
		const UArcEcotopeSpeciesModel& Species,
		const FArcForestInstanceScale& Scale,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal,
		const float SizeCm,
		const float AgeYears,
		const float BandMinHeightCm,
		const float BandMaxHeightCm);
}
