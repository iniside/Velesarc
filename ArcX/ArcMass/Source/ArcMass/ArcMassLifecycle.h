// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassEntityConcepts.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "MassSignalProcessorBase.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"

#include "ArcMassLifecycle.generated.h"

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	const FName LifecyclePhaseChanged = FName(TEXT("LifecyclePhaseChanged"));
	const FName LifecycleDead = FName(TEXT("LifecycleDead"));
	const FName LifecycleForceTransition = FName(TEXT("LifecycleForceTransition"));
}

// ---------------------------------------------------------------------------
// Lifecycle Phase Enum
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcLifecyclePhase : uint8
{
	/** Initial phase after entity spawn. */
	Start   = 0,
	/** Entity is actively growing / developing. */
	Growing = 1,
	/** Entity has reached full maturity. */
	Grown   = 2,
	/** Entity is deteriorating / dying. */
	Dying   = 3,
	/** Entity is dead — terminal state. */
	Dead    = 4,

	MAX     UMETA(Hidden)
};

// ---------------------------------------------------------------------------
// Per-Entity Fragment
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcLifecycleFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current lifecycle phase. */
	UPROPERTY(EditAnywhere)
	EArcLifecyclePhase CurrentPhase = EArcLifecyclePhase::Start;

	/** Previous phase — set on transition for downstream consumers. */
	UPROPERTY()
	EArcLifecyclePhase PreviousPhase = EArcLifecyclePhase::Start;

	/** Time elapsed in the current phase (seconds). Server-authoritative. */
	UPROPERTY()
	float PhaseTimeElapsed = 0.f;
};

// ---------------------------------------------------------------------------
// Config Fragment (Const Shared)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcLifecycleConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Duration (seconds) for the Start phase. 0 or negative = stay forever until forced. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float StartDuration = 0.f;

	/** Duration (seconds) for the Growing phase. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float GrowingDuration = 30.f;

	/** Duration (seconds) for the Grown phase. 0 = stay forever (typical for mature entities). */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float GrownDuration = 0.f;

	/** Duration (seconds) for the Dying phase. */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float DyingDuration = 10.f;

	/** Duration (seconds) for the Dead phase before respawning. 0 = stay dead forever (no respawn). */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	float DeadDuration = 0.f;

	/** If true, entities reaching Dead are signaled via LifecycleDead for destruction.
	 *  If false, they remain in Dead indefinitely (useful for corpses/stumps). */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	bool bSignalDeadForDestruction = true;

	/** Initial phase when entity is spawned. Override to start mid-lifecycle (e.g. already Grown). */
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	EArcLifecyclePhase InitialPhase = EArcLifecyclePhase::Start;

	/** Get duration for a given phase. */
	float GetPhaseDuration(EArcLifecyclePhase Phase) const
	{
		switch (Phase)
		{
		case EArcLifecyclePhase::Start:   return StartDuration;
		case EArcLifecyclePhase::Growing: return GrowingDuration;
		case EArcLifecyclePhase::Grown:   return GrownDuration;
		case EArcLifecyclePhase::Dying:   return DyingDuration;
		case EArcLifecyclePhase::Dead:    return DeadDuration;
		default:                          return 0.f;
		}
	}

	/** Get the next phase in the linear progression, or MAX if at Dead. */
	static EArcLifecyclePhase GetNextPhase(EArcLifecyclePhase Current)
	{
		const uint8 Next = static_cast<uint8>(Current) + 1;
		return (Next < static_cast<uint8>(EArcLifecyclePhase::MAX))
			? static_cast<EArcLifecyclePhase>(Next)
			: EArcLifecyclePhase::MAX;
	}
};

template<>
struct TMassFragmentTraits<FArcLifecycleConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Tag — observer trigger for initialization
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcLifecycleTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Actor Observer Interface — notifies hydrated actors of phase changes
// ---------------------------------------------------------------------------

UINTERFACE(BlueprintType, MinimalAPI)
class UArcLifecycleObserverInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCMASS_API IArcLifecycleObserverInterface
{
	GENERATED_BODY()

public:
	/** Called on hydrated actors when their entity's lifecycle phase changes. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ArcMass|Lifecycle")
	void OnLifecyclePhaseChanged(AActor* Actor, EArcLifecyclePhase NewPhase, EArcLifecyclePhase OldPhase);
};

// ---------------------------------------------------------------------------
// Force Transition Request (non-USTRUCT, internal queue item)
// ---------------------------------------------------------------------------

struct FArcLifecycleForceTransitionRequest
{
	FMassEntityHandle Entity;
	EArcLifecyclePhase TargetPhase = EArcLifecyclePhase::Dead;
	/** If true, resets PhaseTimeElapsed to 0. If false, preserves it. */
	bool bResetTime = true;
};

// ---------------------------------------------------------------------------
// Subsystem — request queue + query API
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcLifecycleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -- Signal-based mutation API (enqueues request + raises signal) ---------

	/** Request a forced phase transition for an entity. Processed by UArcLifecycleForceTransitionProcessor. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle")
	void RequestForceTransition(const FMassEntityHandle& Entity, EArcLifecyclePhase TargetPhase, bool bResetTime = true);

	// -- Direct mutation API (immediate deferred command) ---------------------

	/** Directly set an entity's lifecycle phase via deferred command. Alternative to the signal path. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle")
	void SetLifecyclePhase(const FMassEntityHandle& Entity, EArcLifecyclePhase NewPhase, bool bResetTime = true);

	// -- Query API -----------------------------------------------------------

	/** Get the current lifecycle phase of an entity. Returns MAX if entity has no lifecycle fragment. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle", meta = (WorldContext = "WorldContextObject"))
	static EArcLifecyclePhase GetEntityLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	/** Get time elapsed in current phase. Returns 0 if entity has no lifecycle fragment. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle", meta = (WorldContext = "WorldContextObject"))
	static float GetEntityPhaseTimeElapsed(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	// -- Internal (used by signal processor) ---------------------------------

	TArray<FArcLifecycleForceTransitionRequest>& GetPendingRequests() { return PendingRequests; }

private:
	TArray<FArcLifecycleForceTransitionRequest> PendingRequests;
};

// ---------------------------------------------------------------------------
// Tick Processor — ticks phase timers, triggers natural transitions
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcLifecycleTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcLifecycleTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// Force Transition Signal Processor — handles external requests
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcLifecycleForceTransitionProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcLifecycleForceTransitionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Init Observer — sets initial phase from config on tag add
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcLifecycleInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcLifecycleInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// InstancedActors Bridge — writes lifecycle phase to UInstancedActorsData
// for persistence and replication.
//
// Subscribes to LifecyclePhaseChanged. For entities with both
// FArcLifecycleFragment and FInstancedActorsFragment, writes the current
// phase index to the engine's delta system via
// UInstancedActorsData::SetInstanceCurrentLifecyclePhase().
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcLifecycleInstancedActorsBridgeProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcLifecycleInstancedActorsBridgeProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Actor Notify Signal Processor — notifies hydrated actors of phase changes
// via IArcLifecycleObserverInterface.
//
// Subscribes to LifecyclePhaseChanged. For entities with a valid
// FMassActorFragment actor implementing IArcLifecycleObserverInterface,
// calls OnLifecyclePhaseChanged.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcLifecycleActorNotifyProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcLifecycleActorNotifyProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Trait — opts entities into the lifecycle system
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Lifecycle"))
class ARCMASS_API UArcLifecycleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FArcLifecycleConfigFragment LifecycleConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
