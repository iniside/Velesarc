// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTypes.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

class UArcAttributeHandlerConfig;
class UArcEffectDefinition;
class UWorld;
struct FActorTestSpawner;
struct FArcActiveEffect;
struct FArcAttributeRef;
struct FArcDamageDealtPayload;
struct FArcGameEvent;
struct FArcHitResolvedPayload;
struct FMassEntityManager;

namespace ArcStatusTest
{
	class FStatusHarness;
}

namespace ArcCombatTest
{
	struct FCombatEntityOptions
	{
		float Health = 1000.f;

		float MissChance = 0.f;
		float CritChance = 0.f;
		float CritMultiplier = 1.5f;
		float DodgeChance = 0.f;
		float ParryChance = 0.f;
		float BlockChance = 0.f;

		float Resistance = 0.f;
		float DamageReduction = 0.f;

		float OutgoingBonus = 0.f;
		float IncomingBonus = 0.f;
		float IncomingBonusPercent = 0.f;
		float MaxIncomingBonusPercent = 0.f;

		float Armor = 0.f;

		FVector Location = FVector::ZeroVector;
		float Yaw = 0.f;

		FGameplayTagContainer OwnedTags;
		FGameplayTagContainer ImmunityTags;

		UArcAttributeHandlerConfig* HandlerConfigOverride = nullptr;

		TFunction<void(UE::Mass::FEntityBuilder&)> AddFragments;
	};

	ARCMASSMECHANICSTEST_API UArcEffectDefinition* MakeEffect(EArcEffectDuration Duration = EArcEffectDuration::Instant);
	ARCMASSMECHANICSTEST_API void AddGate(UArcEffectDefinition* Definition);
	ARCMASSMECHANICSTEST_API void AddExecution(UArcEffectDefinition* Definition, const FInstancedStruct& Execution);

	class ARCMASSMECHANICSTEST_API FCombatHarness
	{
	public:
		FCombatHarness();
		~FCombatHarness();
		FCombatHarness(const FCombatHarness&) = delete;
		FCombatHarness& operator=(const FCombatHarness&) = delete;

		bool Initialize(FActorTestSpawner& Spawner);

		FMassEntityHandle CreateEntity(const FCombatEntityOptions& Options = FCombatEntityOptions());

		FArcEffectApplyResult Apply(UArcEffectDefinition* Effect, FMassEntityHandle Target, FMassEntityHandle Source,
			const FInstancedStruct& SourceData = FInstancedStruct());
		void Settle(FMassEntityHandle Entity);
		void TickDuration(float DeltaSeconds, FMassEntityHandle Entity);

		void AddIncomingBonusMod(FMassEntityHandle Entity, float Magnitude, const FGameplayTag& RequiredSourceTag);
		void AddAttributeMod(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Magnitude, const FGameplayTag& RequiredSourceTag);

		float GetHealth(FMassEntityHandle Entity) const;
		float GetDamageMeta(FMassEntityHandle Entity) const;
		int32 CountEvents(const FGameplayTag& Tag) const;
		int32 CountOutcomeEvents() const;
		int32 CountHits() const;
		int32 CountDamageEvents() const;
		const FArcGameEvent* FindLastEvent(const FGameplayTag& Tag) const;
		const FArcHitResolvedPayload* FindLastPayload(const FGameplayTag& Tag) const;
		const FArcHitResolvedPayload* FindLastHit() const;
		const FArcDamageDealtPayload* FindLastDamage() const;
		const FArcActiveEffect* FindActiveByDefinition(FMassEntityHandle Entity, const UArcEffectDefinition* Definition) const;
		void ClearEvents();

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;

	private:
		TUniquePtr<ArcStatusTest::FStatusHarness> Base;
	};
}
