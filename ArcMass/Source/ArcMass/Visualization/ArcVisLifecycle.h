// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcVisLifecycle.generated.h"

class UStaticMesh;
class UMaterialInterface;

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	inline const FName VisLifecyclePhaseChanged    = FName(TEXT("VisLifecyclePhaseChanged"));
	inline const FName VisLifecycleDead            = FName(TEXT("VisLifecycleDead"));
	inline const FName VisLifecycleForceTransition = FName(TEXT("VisLifecycleForceTransition"));
}

// ---------------------------------------------------------------------------
// Per-Phase Visual Descriptor
// ---------------------------------------------------------------------------

/** Describes visual overrides for a single lifecycle phase.
  * Any nullptr/empty field inherits from the base FArcVisConfigFragment. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcVisLifecyclePhaseVisuals
{
	GENERATED_BODY()

	/** Static mesh for this phase's ISM representation. nullptr = inherit from base config. */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Material overrides for this phase. Empty = inherit from base config. */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	/** Optional actor class override for actor representation.
	  * nullptr = keep current actor and notify via IArcLifecycleObserverInterface. */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TSubclassOf<AActor> ActorClassOverride;

	bool HasMeshOverride() const { return StaticMesh != nullptr; }
	bool HasActorOverride() const { return ActorClassOverride != nullptr; }
};

// ---------------------------------------------------------------------------
// Config Fragment (Const Shared)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcVisLifecycleConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	TArray<FArcVisLifecyclePhaseVisuals> PhaseVisuals;

	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	TArray<float> PhaseDurations;

	/** Initial phase when entity is spawned. Override to start mid-lifecycle. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	uint8 InitialPhase = 0;

	/** Terminal phase — reaching this phase raises VisLifecycleDead. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	uint8 TerminalPhase = 0;

	// --- Accessors ---

	uint8 GetNumPhases() const { return static_cast<uint8>(PhaseDurations.Num()); }
	bool IsValidPhase(uint8 Phase) const { return Phase < GetNumPhases(); }
	uint8 GetNextPhase(uint8 Current) const
	{
		const uint8 Num = GetNumPhases();
		return (Num > 0 && Current < Num - 1) ? Current + 1 : Current;
	}

	float GetPhaseDuration(uint8 Phase) const;
	const FArcVisLifecyclePhaseVisuals& GetPhaseVisuals(uint8 Phase) const;

	/** Resolve mesh for a phase, falling back to base mesh fragment. */
	UStaticMesh* ResolveMesh(uint8 Phase, const FArcMassStaticMeshConfigFragment& BaseMeshConfigFrag) const;

	/** Resolve materials for a phase, falling back to base config materials. */
	const TArray<TObjectPtr<UMaterialInterface>>& ResolveMaterials(uint8 Phase, const FArcVisConfigFragment& BaseConfig) const;

	/** Resolve actor class for a phase, falling back to base config actor class. */
	TSubclassOf<AActor> ResolveActorClass(uint8 Phase, const FArcVisConfigFragment& BaseConfig) const;
};

template<>
struct TMassFragmentTraits<FArcVisLifecycleConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Per-Entity Fragment
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcVisLifecycleFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current lifecycle phase. */
	UPROPERTY()
	uint8 CurrentPhase = 0;

	/** Previous phase — set on transition for downstream consumers. */
	UPROPERTY()
	uint8 PreviousPhase = 0;

	/** Time elapsed in the current phase (seconds). Server-authoritative. */
	UPROPERTY()
	float PhaseTimeElapsed = 0.f;
};

// ---------------------------------------------------------------------------
// Tag — observer trigger for initialization
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcVisLifecycleTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCMASS_API FArcVisLifecycleAutoAdvanceTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Force Transition Request (internal queue item)
// ---------------------------------------------------------------------------

struct FArcVisLifecycleForceTransitionRequest
{
	FMassEntityHandle Entity;
	uint8 TargetPhase = 0;
	bool bResetTime = true;
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisLifecycleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -- Signal-based mutation API (enqueues request + raises signal) ---------

	/** Request a forced phase transition. Processed by UArcVisLifecycleForceTransitionProcessor. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle")
	void RequestForceTransition(const FMassEntityHandle& Entity, uint8 TargetPhase, bool bResetTime = true);

	// -- Direct mutation API (immediate mutation + signal) --------------------

	/** Directly set an entity's vis lifecycle phase. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle")
	void SetLifecyclePhase(const FMassEntityHandle& Entity, uint8 NewPhase, bool bResetTime = true);

	// -- Query API -----------------------------------------------------------

	/** Get the current vis lifecycle phase. Returns 0xFF if entity has no vis lifecycle fragment. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle", meta = (WorldContext = "WorldContextObject"))
	static uint8 GetEntityVisLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	/** Get time elapsed in current phase. Returns 0 if entity has no vis lifecycle fragment. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle", meta = (WorldContext = "WorldContextObject"))
	static float GetEntityVisPhaseTimeElapsed(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	// -- Internal (used by signal processor) ---------------------------------

	TArray<FArcVisLifecycleForceTransitionRequest>& GetPendingRequests() { return PendingRequests; }

private:
	TArray<FArcVisLifecycleForceTransitionRequest> PendingRequests;
};
