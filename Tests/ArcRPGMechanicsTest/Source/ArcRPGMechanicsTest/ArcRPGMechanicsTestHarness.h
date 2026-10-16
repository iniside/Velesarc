// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRPGMechTestSupport.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

class UArcAbilityDefinition;
class UArcAttributeHandlerConfig;
struct FArcItemDefinitionData;
class UArcPendingAttributeOpsProcessor;
class UArcCooldownProcessor;
class UMassSignalSubsystem;
class UWorld;
struct FActorTestSpawner;
struct FArcAbilityCustomData;
struct FArcAttributeSet_Health;
struct FArcAttributeSet_RPGEnergy;
struct FArcAttributeSet_RPGEntropy;
struct FArcAttributeSet_RPGHealthExtension;
struct FArcAttributeSet_RPGStamina;
struct FArcItemData;
struct FMassEntityManager;

namespace ArcRPGMechanicsTestHarness
{
	struct FCasterOptions
	{
		float Energy = 100.f;
		float Health = 100.f;
		float Stamina = 100.f;
		float Entropy = 0.f;
		float ThresholdEntropy = 0.f;
		bool bHandlerConfig = true;
	};

	const FArcItemDefinitionData* LoadItem(const TCHAR* Name);
	UArcAbilityDefinition* LoadCostAbility(const TCHAR* Name);
	void MakeItem(const FArcItemDefinitionData* Definition, FArcItemData& OutItem);
	FArcAbilityCustomData MakeItemCustomData(FArcItemData& Item);
	int32 GetCascadeStacks(const FArcItemData& Item);
	void AddCascadeStacks(const FArcItemData& Item, int32 Count);

	class FHarness
	{
	public:
		bool Initialize(FActorTestSpawner& Spawner);

		FMassEntityHandle CreateCaster(const FCasterOptions& Options = FCasterOptions());
		void Activate(FMassEntityHandle Entity, FGameplayTag InputTag);
		void DrainPendingOps(FMassEntityHandle Entity);
		void TickCooldowns(float DeltaSeconds);

		FArcAttributeSet_RPGEnergy* GetEnergy(FMassEntityHandle Entity) const;
		FArcAttributeSet_RPGStamina* GetStamina(FMassEntityHandle Entity) const;
		FArcAttributeSet_RPGEntropy* GetEntropy(FMassEntityHandle Entity) const;
		FArcAttributeSet_Health* GetHealth(FMassEntityHandle Entity) const;
		FArcAttributeSet_RPGHealthExtension* GetHealthExtension(FMassEntityHandle Entity) const;
		int32 CountCooldownEntries(FMassEntityHandle Entity) const;

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UMassSignalSubsystem* SignalSubsystem = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;

	private:
		UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
		UArcCooldownProcessor* CooldownProcessor = nullptr;
		TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	};
}
