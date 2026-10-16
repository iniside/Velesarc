// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMechTestSupport.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectTypes.h"
#include "Events/ArcGameEvent.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

class UArcAbilityStateTreeSubsystem;
class UArcAttributeAggregatorProcessor;
class UArcAttributeHandlerConfig;
class UArcEffectDurationProcessor;
class UArcGameEventSubsystem;
class UArcPendingAttributeOpsProcessor;
class UMassSignalSubsystem;
class UStateTree;
class UWorld;
struct FActorTestSpawner;
struct FArcActiveEffect;
struct FArcMechTest_CostSet;
struct FMassEntityManager;

namespace ArcStatusTest
{
	struct FStatusEntityOptions
	{
		float Mana = 100.f;
		float TierValue = 10.f;
		bool bCostSet = true;
		UArcAttributeHandlerConfig* HandlerConfigOverride = nullptr;
	};

	void SuppressTestLogs();
	UStateTree* MakeRunForeverTree(UObject* Outer);

	class FStatusHarness
	{
	public:
		bool Initialize(FActorTestSpawner& Spawner);

		FMassEntityHandle CreateEntity(const FStatusEntityOptions& Options = FStatusEntityOptions());

		void CaptureEvents(FGameplayTag Tag);
		void DrainEvents();
		int32 CountCaptured(FGameplayTag Tag) const;

		void TickDuration(float DeltaSeconds);
		void DrainPendingOps(FMassEntityHandle Entity);
		void Recalculate(FMassEntityHandle Entity);
		void Activate(FMassEntityHandle Entity, FGameplayTag InputTag);
		void AdvanceWorldTime(double Seconds);

		FArcActiveEffect* FindActive(FMassEntityHandle Entity, FArcActiveEffectHandle Handle) const;
		const FArcActiveEffect* FindActiveByDefinition(FMassEntityHandle Entity, const UArcEffectDefinition* Definition) const;
		int32 NumActive(FMassEntityHandle Entity) const;
		FArcMechTest_CostSet* GetSet(FMassEntityHandle Entity) const;
		float GetTierValue(FMassEntityHandle Entity) const;
		float GetMana(FMassEntityHandle Entity) const;

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UMassSignalSubsystem* SignalSubsystem = nullptr;
		UArcGameEventSubsystem* EventBus = nullptr;
		UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;
		TArray<FArcGameEvent> CapturedEvents;

	private:
		void HandleEvent(const FArcGameEvent& Event);

		TOptional<ArcMechTest::FVerboseLogScope> LogScope;

		UArcEffectDurationProcessor* DurationProcessor = nullptr;
		UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
		UArcAttributeAggregatorProcessor* AggregatorProcessor = nullptr;
	};
}
