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
	inline const FName LifecyclePhaseChanged = FName(TEXT("LifecyclePhaseChanged"));
	inline const FName LifecycleDead = FName(TEXT("LifecycleDead"));
	inline const FName LifecycleForceTransition = FName(TEXT("LifecycleForceTransition"));
}

// ---------------------------------------------------------------------------
// Per-Entity Fragment
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcLifecycleFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	uint8 CurrentPhase = 0;
	UPROPERTY(SaveGame)
	uint8 PreviousPhase = 0;
	UPROPERTY(SaveGame)
	float PhaseTimeElapsed = 0.f;
};

// ---------------------------------------------------------------------------
// Config Fragment (Const Shared)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcLifecycleConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	TArray<float> PhaseDurations;
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	uint8 InitialPhase = 0;
	UPROPERTY(EditAnywhere, Category = "Lifecycle")
	uint8 TerminalPhase = 0;

	uint8 GetNumPhases() const { return static_cast<uint8>(PhaseDurations.Num()); }
	float GetPhaseDuration(uint8 Phase) const
	{
		return PhaseDurations.IsValidIndex(Phase) ? PhaseDurations[Phase] : 0.f;
	}
	uint8 GetNextPhase(uint8 Current) const
	{
		const uint8 Num = GetNumPhases();
		return (Num > 0 && Current < Num - 1) ? Current + 1 : Current;
	}
	bool IsValidPhase(uint8 Phase) const { return Phase < GetNumPhases(); }
};

template<>
struct TMassFragmentTraits<FArcLifecycleConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// ---------------------------------------------------------------------------
// Tag — observer trigger for initialization
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcLifecycleTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCMASS_API FArcLifecycleAutoAdvanceTag : public FMassTag
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
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ArcMass|Lifecycle")
	void OnLifecyclePhaseChanged(AActor* Actor, uint8 NewPhase, uint8 OldPhase);
};

// ---------------------------------------------------------------------------
// Force Transition Request (non-USTRUCT, internal queue item)
// ---------------------------------------------------------------------------

struct FArcLifecycleForceTransitionRequest
{
	FMassEntityHandle Entity;
	uint8 TargetPhase = 0;
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
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle")
	void RequestForceTransition(const FMassEntityHandle& Entity, uint8 TargetPhase, bool bResetTime = true);
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle")
	void SetLifecyclePhase(const FMassEntityHandle& Entity, uint8 NewPhase, bool bResetTime = true);
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle", meta = (WorldContext = "WorldContextObject"))
	static uint8 GetEntityLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity);
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle", meta = (WorldContext = "WorldContextObject"))
	static bool HasLifecycle(const UObject* WorldContextObject, const FMassEntityHandle& Entity);
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Lifecycle", meta = (WorldContext = "WorldContextObject"))
	static float GetEntityPhaseTimeElapsed(const UObject* WorldContextObject, const FMassEntityHandle& Entity);
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

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Lifecycle", Category = "Lifecycle"))
class ARCMASS_API UArcLifecycleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FArcLifecycleConfigFragment LifecycleConfig;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	bool bAutoAdvance = true;
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
