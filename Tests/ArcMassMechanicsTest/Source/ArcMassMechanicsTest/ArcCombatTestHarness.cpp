// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCombatTestHarness.h"

#include "ArcCombatTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_Crit.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_HitChain.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_Mitigation.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "ArcMassMechanics/Combat/ArcMechHitGate.h"
#include "ArcMassDamageSystem/ArcAttributeSet_Damage.h"
#include "ArcMassDamageSystem/ArcAttributeSet_DamageModifiers.h"
#include "ArcMassDamageSystem/ArcAttributeSet_Health.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/Sources/ArcExtraCaptureSource_TargetEntity.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventTags.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Tags/ArcGameplayTagRequirements.h"

namespace ArcCombatTestHarnessPrivate
{
	const TArray<FGameplayTag>& GetOutcomeTags()
	{
		static const TArray<FGameplayTag> Tags = {
			ArcMassMechanicsTags::HitOutcome_Hit, ArcMassMechanicsTags::HitOutcome_Miss, ArcMassMechanicsTags::HitOutcome_Dodge,
			ArcMassMechanicsTags::HitOutcome_Parry, ArcMassMechanicsTags::HitOutcome_Block
		};
		return Tags;
	}
}

UArcEffectDefinition* ArcCombatTest::MakeEffect(EArcEffectDuration Duration)
{
	UArcEffectDefinition* Definition = NewObject<UArcEffectDefinition>();
	Definition->StackingPolicy.DurationType = Duration;
	if (Duration == EArcEffectDuration::Duration)
	{
		Definition->StackingPolicy.Duration = 100.f;
	}
	return Definition;
}

void ArcCombatTest::AddGate(UArcEffectDefinition* Definition)
{
	FArcMechHitGate Gate;
	Definition->Components.Add(FInstancedStruct::Make(Gate));
	Definition->BuildCaptureLayout();
}

void ArcCombatTest::AddExecution(UArcEffectDefinition* Definition, const FInstancedStruct& Execution)
{
	FArcEffectExecutionEntry Entry;
	Entry.CustomExecution = Execution;
	Definition->Executions.Add(Entry);
	Definition->BuildCaptureLayout();
}

ArcCombatTest::FCombatHarness::FCombatHarness()
	: Base(MakeUnique<ArcStatusTest::FStatusHarness>())
{
}

ArcCombatTest::FCombatHarness::~FCombatHarness() = default;

bool ArcCombatTest::FCombatHarness::Initialize(FActorTestSpawner& Spawner)
{
	if (!Base->Initialize(Spawner))
	{
		return false;
	}

	World = Base->World;
	EntityManager = Base->EntityManager;

	HandlerConfig = ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_Combat_Test"));
	if (!HandlerConfig)
	{
		return false;
	}

	for (const FGameplayTag& OutcomeTag : ArcCombatTestHarnessPrivate::GetOutcomeTags())
	{
		Base->CaptureEvents(OutcomeTag);
	}
	Base->CaptureEvents(ArcMassMechanicsTags::DamageDealt);
	Base->CaptureEvents(ArcGameEventTags::EffectApplied);
	return true;
}

FMassEntityHandle ArcCombatTest::FCombatHarness::CreateEntity(const FCombatEntityOptions& Options)
{
	FArcAttributeSet_Health Health;
	Health.InitHealth(Options.Health);
	Health.InitMaxHealth(100000.f);

	FArcAttributeSet_Damage Damage;
	Damage.InitDamage(0.f);

	FArcAttributeSet_DamageModifiers Modifiers;
	Modifiers.InitOutgoingBonus(Options.OutgoingBonus);
	Modifiers.InitIncomingBonus(Options.IncomingBonus);
	Modifiers.InitIncomingBonusPercent(Options.IncomingBonusPercent);
	Modifiers.InitMaxIncomingBonusPercent(Options.MaxIncomingBonusPercent);

	FArcAttributeSet_HitChain HitChain;
	HitChain.InitMissChance(Options.MissChance);
	HitChain.InitDodgeChance(Options.DodgeChance);
	HitChain.InitParryChance(Options.ParryChance);
	HitChain.InitBlockChance(Options.BlockChance);

	FArcAttributeSet_Crit Crit;
	Crit.InitCritChance(Options.CritChance);
	Crit.InitCritMultiplier(Options.CritMultiplier);

	FArcAttributeSet_Mitigation Mitigation;
	Mitigation.InitArmor(Options.Armor);
	Mitigation.InitResistance(Options.Resistance);
	Mitigation.InitDamageReduction(Options.DamageReduction);

	FArcMechTest_CombatSet Combat;
	Combat.InitHealMeta(0.f);

	FTransformFragment Transform;
	Transform.SetTransform(FTransform(FRotator(0.f, Options.Yaw, 0.f), Options.Location));

	FArcAttributeHandlerSharedFragment HandlerShared;
	HandlerShared.Config = Options.HandlerConfigOverride ? Options.HandlerConfigOverride : HandlerConfig;

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcAttributeSet_Health>(Health)
		.Add<FArcAttributeSet_Damage>(Damage)
		.Add<FArcAttributeSet_DamageModifiers>(Modifiers)
		.Add<FArcAttributeSet_HitChain>(HitChain)
		.Add<FArcAttributeSet_Crit>(Crit)
		.Add<FArcAttributeSet_Mitigation>(Mitigation)
		.Add<FArcMechTest_CombatSet>(Combat)
		.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcPendingAttributeOpsFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FTransformFragment>(Transform)
		.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);

	if (!Options.ImmunityTags.IsEmpty())
	{
		FArcImmunityFragment Immunity;
		Immunity.ImmunityTags.AddTags(Options.ImmunityTags);
		Builder.Add<FArcImmunityFragment>(Immunity);
	}

	if (Options.AddFragments)
	{
		Options.AddFragments(Builder);
	}

	const FMassEntityHandle Entity = Builder.Commit();
	if (!Options.OwnedTags.IsEmpty())
	{
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, Options.OwnedTags);
	}
	return Entity;
}

FArcEffectApplyResult ArcCombatTest::FCombatHarness::Apply(UArcEffectDefinition* Effect, FMassEntityHandle Target, FMassEntityHandle Source,
	const FInstancedStruct& SourceData)
{
	const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, Source, SourceData);
	Settle(Target);
	return Result;
}

void ArcCombatTest::FCombatHarness::Settle(FMassEntityHandle Entity)
{
	Base->DrainPendingOps(Entity);
	Base->DrainEvents();
}

void ArcCombatTest::FCombatHarness::TickDuration(float DeltaSeconds, FMassEntityHandle Entity)
{
	Base->TickDuration(DeltaSeconds);
	Settle(Entity);
}

void ArcCombatTest::FCombatHarness::AddIncomingBonusMod(FMassEntityHandle Entity, float Magnitude, const FGameplayTag& RequiredSourceTag)
{
	AddAttributeMod(Entity, FArcAttributeSet_DamageModifiers::GetIncomingBonusAttribute(), Magnitude, RequiredSourceTag);
}

void ArcCombatTest::FCombatHarness::AddAttributeMod(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Magnitude,
	const FGameplayTag& RequiredSourceTag)
{
	FMassEntityView View(*EntityManager, Entity);
	FArcAggregatorFragment* Aggregators = View.GetFragmentDataPtr<FArcAggregatorFragment>();
	check(Aggregators);

	FArcGameplayTagRequirements Requirements;
	if (RequiredSourceTag.IsValid())
	{
		Requirements.RequireTags.AddTag(RequiredSourceTag);
	}
	Aggregators->FindOrAddAggregator(Attribute)
		.AddMod(EArcModifierOp::Add, Magnitude, FMassEntityHandle(), &Requirements, nullptr);
}

float ArcCombatTest::FCombatHarness::GetHealth(FMassEntityHandle Entity) const
{
	FMassEntityView View(*EntityManager, Entity);
	const FArcAttributeSet_Health* Set = View.GetSparseFragmentDataPtr<FArcAttributeSet_Health>();
	return Set ? Set->GetHealth() : -1.f;
}

float ArcCombatTest::FCombatHarness::GetDamageMeta(FMassEntityHandle Entity) const
{
	FMassEntityView View(*EntityManager, Entity);
	const FArcAttributeSet_Damage* Set = View.GetSparseFragmentDataPtr<FArcAttributeSet_Damage>();
	return Set ? Set->Damage.BaseValue : -1.f;
}

int32 ArcCombatTest::FCombatHarness::CountEvents(const FGameplayTag& Tag) const
{
	return Base->CountCaptured(Tag);
}

int32 ArcCombatTest::FCombatHarness::CountOutcomeEvents() const
{
	int32 Count = 0;
	for (const FArcGameEvent& Event : Base->CapturedEvents)
	{
		if (ArcCombatTestHarnessPrivate::GetOutcomeTags().Contains(Event.Tag))
		{
			++Count;
		}
	}
	return Count;
}

int32 ArcCombatTest::FCombatHarness::CountHits() const
{
	return Base->CountCaptured(ArcMassMechanicsTags::HitOutcome_Hit);
}

int32 ArcCombatTest::FCombatHarness::CountDamageEvents() const
{
	return Base->CountCaptured(ArcMassMechanicsTags::DamageDealt);
}

const FArcGameEvent* ArcCombatTest::FCombatHarness::FindLastEvent(const FGameplayTag& Tag) const
{
	for (int32 Index = Base->CapturedEvents.Num() - 1; Index >= 0; --Index)
	{
		if (Base->CapturedEvents[Index].Tag == Tag)
		{
			return &Base->CapturedEvents[Index];
		}
	}
	return nullptr;
}

const FArcHitResolvedPayload* ArcCombatTest::FCombatHarness::FindLastPayload(const FGameplayTag& Tag) const
{
	const FArcGameEvent* Event = FindLastEvent(Tag);
	return Event ? Event->Payload.GetPtr<FArcHitResolvedPayload>() : nullptr;
}

const FArcHitResolvedPayload* ArcCombatTest::FCombatHarness::FindLastHit() const
{
	return FindLastPayload(ArcMassMechanicsTags::HitOutcome_Hit);
}

const FArcDamageDealtPayload* ArcCombatTest::FCombatHarness::FindLastDamage() const
{
	const FArcGameEvent* Event = FindLastEvent(ArcMassMechanicsTags::DamageDealt);
	return Event ? Event->Payload.GetPtr<FArcDamageDealtPayload>() : nullptr;
}

const FArcActiveEffect* ArcCombatTest::FCombatHarness::FindActiveByDefinition(FMassEntityHandle Entity, const UArcEffectDefinition* Definition) const
{
	return Base->FindActiveByDefinition(Entity, Definition);
}

void ArcCombatTest::FCombatHarness::ClearEvents()
{
	Base->CapturedEvents.Reset();
}
