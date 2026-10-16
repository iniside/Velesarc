// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMechTestSupport.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

class UArcAttributeHandlerConfig;
class UArcPendingAttributeOpsProcessor;
class UMassSignalSubsystem;
class UStateTree;
class UWorld;
struct FActorTestSpawner;
struct FArcMechTest_CostSet;
struct FMassEntityManager;

namespace ArcMassMechanicsAbilityTest
{
	struct FCostEntityOptions
	{
		float Mana = 100.f;
		float Stamina = 100.f;
		float TierValue = 0.f;
		float TierThreshold = 0.f;
		bool bHandlerConfig = true;
	};

	UStateTree* MakeCommitTree(UObject* Outer);

	class FAbilityHarness
	{
	public:
		bool Initialize(FActorTestSpawner& Spawner);

		FMassEntityHandle CreateEntity(const FCostEntityOptions& Options = FCostEntityOptions());
		void Activate(FMassEntityHandle Entity, FGameplayTag InputTag);
		void DrainPendingOps(FMassEntityHandle Entity);
		FArcMechTest_CostSet* GetSet(FMassEntityHandle Entity) const;
		int32 CountActiveEffects(FMassEntityHandle Entity) const;

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UMassSignalSubsystem* SignalSubsystem = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;

	private:
		UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
		TOptional<ArcMechTest::FVerboseLogScope> LogScope;
	};
}
