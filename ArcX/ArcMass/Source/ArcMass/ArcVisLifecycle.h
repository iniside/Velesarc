// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassLifecycle.h"
#include "ArcMassEntityVisualization.h"
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
	const FName VisLifecyclePhaseChanged    = FName(TEXT("VisLifecyclePhaseChanged"));
	const FName VisLifecycleDead            = FName(TEXT("VisLifecycleDead"));
	const FName VisLifecycleForceTransition = FName(TEXT("VisLifecycleForceTransition"));
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

	// --- Per-Phase Visuals ---

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	FArcVisLifecyclePhaseVisuals StartVisuals;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	FArcVisLifecyclePhaseVisuals GrowingVisuals;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	FArcVisLifecyclePhaseVisuals GrownVisuals;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	FArcVisLifecyclePhaseVisuals DyingVisuals;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	FArcVisLifecyclePhaseVisuals DeadVisuals;

	// --- Phase Durations ---

	/** Duration (seconds) for the Start phase. 0 or negative = stay until forced. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float StartDuration = 0.f;

	/** Duration (seconds) for the Growing phase. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float GrowingDuration = 30.f;

	/** Duration (seconds) for the Grown phase. 0 = stay forever. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float GrownDuration = 0.f;

	/** Duration (seconds) for the Dying phase. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float DyingDuration = 10.f;

	/** Duration (seconds) for the Dead phase before respawning. 0 = stay dead forever. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float DeadDuration = 0.f;

	// --- Flags ---

	/** If true, auto-tick is disabled. Phase changes happen only via forced transitions. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	bool bDisableAutoTick = false;

	/** If true, entities reaching Dead are signaled via VisLifecycleDead for destruction. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	bool bSignalDeadForDestruction = true;

	/** Initial phase when entity is spawned. Override to start mid-lifecycle. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	EArcLifecyclePhase InitialPhase = EArcLifecyclePhase::Start;

	// --- Accessors ---

	float GetPhaseDuration(EArcLifecyclePhase Phase) const;
	const FArcVisLifecyclePhaseVisuals& GetPhaseVisuals(EArcLifecyclePhase Phase) const;

	/** Resolve ISM mesh for a phase, falling back to base config mesh. */
	UStaticMesh* ResolveISMMesh(EArcLifecyclePhase Phase, const FArcVisConfigFragment& BaseConfig) const;

	/** Resolve ISM materials for a phase, falling back to base config materials. */
	const TArray<TObjectPtr<UMaterialInterface>>& ResolveISMMaterials(EArcLifecyclePhase Phase, const FArcVisConfigFragment& BaseConfig) const;

	/** Resolve actor class for a phase, falling back to base config actor class. */
	TSubclassOf<AActor> ResolveActorClass(EArcLifecyclePhase Phase, const FArcVisConfigFragment& BaseConfig) const;
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
	EArcLifecyclePhase CurrentPhase = EArcLifecyclePhase::Start;

	/** Previous phase — set on transition for downstream consumers. */
	UPROPERTY()
	EArcLifecyclePhase PreviousPhase = EArcLifecyclePhase::Start;

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

// ---------------------------------------------------------------------------
// Transient Mesh Switch Fragment — consumed by ISM mesh switch processor
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcVisLifecycleMeshSwitchFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The phase to switch visuals to. Processor resolves mesh from config. */
	EArcLifecyclePhase TargetPhase = EArcLifecyclePhase::Start;
};

// ---------------------------------------------------------------------------
// Force Transition Request (internal queue item)
// ---------------------------------------------------------------------------

struct FArcVisLifecycleForceTransitionRequest
{
	FMassEntityHandle Entity;
	EArcLifecyclePhase TargetPhase = EArcLifecyclePhase::Dead;
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
	void RequestForceTransition(const FMassEntityHandle& Entity, EArcLifecyclePhase TargetPhase, bool bResetTime = true);

	// -- Direct mutation API (immediate mutation + signal) --------------------

	/** Directly set an entity's vis lifecycle phase. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle")
	void SetLifecyclePhase(const FMassEntityHandle& Entity, EArcLifecyclePhase NewPhase, bool bResetTime = true);

	// -- Query API -----------------------------------------------------------

	/** Get the current vis lifecycle phase. Returns MAX if entity has no vis lifecycle fragment. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle", meta = (WorldContext = "WorldContextObject"))
	static EArcLifecyclePhase GetEntityVisLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	/** Get time elapsed in current phase. Returns 0 if entity has no vis lifecycle fragment. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|VisLifecycle", meta = (WorldContext = "WorldContextObject"))
	static float GetEntityVisPhaseTimeElapsed(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	// -- Internal (used by signal processor) ---------------------------------

	TArray<FArcVisLifecycleForceTransitionRequest>& GetPendingRequests() { return PendingRequests; }

private:
	TArray<FArcVisLifecycleForceTransitionRequest> PendingRequests;
};
