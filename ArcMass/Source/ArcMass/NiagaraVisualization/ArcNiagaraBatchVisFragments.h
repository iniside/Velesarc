// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcNiagaraBatchVisFragments.generated.h"

class UNiagaraSystem;

// ---------------------------------------------------------------------------
// Batch Niagara Visualization — Data Interface Array approach
//
// Instead of one Niagara system per entity (see FArcNiagaraVisFragment),
// this approach uses ONE Niagara system per archetype that renders ALL
// entities as a single batch. Entity positions are fed into the system
// via a Position Array Data Interface each frame.
//
// Niagara asset requirements:
//   1. Add a User Parameter of type "Position Array" (UNiagaraDataInterfaceArrayPosition)
//      with the name matching PositionArrayParamName (default: "EntityPositions").
//   2. Design emitters to read particle positions from the array:
//
//      Simple (each entity = 1 particle):
//        - Spawn Burst Instantaneous, Count = ArrayLength(EntityPositions)
//        - Emitter set to loop (re-spawns each loop iteration)
//        - Particle Update: Position = EntityPositions[ExecIndex]
//
//      Complex (each entity spawns sub-particles, e.g. fire):
//        - Emitter 1 "Anchors": Spawn Burst = ArrayLength, loop,
//          Position = EntityPositions[ExecIndex], NO renderer
//        - Emitter 2 "Particles": Spawn Per Parent Particle (from Emitter 1),
//          normal particle behavior (velocity, color over life, etc.)
//
// Trade-offs vs per-entity approach:
//   + Much better scalability (1 system + 1 scene proxy for N entities)
//   + Lower per-frame cost (1 tick instead of N)
//   - Niagara asset must be authored to read from arrays
//   - 1 frame position latency (component ticks before processor fills data)
//   - Less flexibility per-entity (all entities share the same system)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Tag — marks entities for batch Niagara visualization
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcNiagaraBatchVisTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Config — const shared fragment (one per archetype)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcNiagaraBatchVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Niagara system that renders all entities in this archetype as a single batch.
	 *  Must have a User Parameter of type "Position Array" with the name below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraBatchVis")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	/** Name of the Position Array user parameter in the Niagara system.
	 *  Must match exactly (case-sensitive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraBatchVis")
	FName PositionArrayParamName = FName("EntityPositions");
};

template<>
struct TMassFragmentTraits<FArcNiagaraBatchVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
