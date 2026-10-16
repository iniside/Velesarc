// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Combat/ArcCombatFunctions.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Mass/EntityManager.h"
#include "GameplayTagsManager.h"

namespace ArcCombatFunctionsPrivate
{
	UArcGameEventSubsystem* GetEventBus(FMassEntityManager& EntityManager)
	{
		UWorld* World = EntityManager.GetWorld();
		return World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
	}

	float ApplyReduction(float Value, float Reduction)
	{
		return Value * (1.f - FMath::Min(Reduction, 1.f));
	}
}

namespace ArcMechCombat
{
	bool IsLeafDamageType(const FGameplayTag& Tag)
	{
		const FGameplayTag Root = ArcMassMechanicsTags::DamageType;
		return Tag.IsValid() && Tag != Root && Tag.MatchesTag(Root)
			&& UGameplayTagsManager::Get().RequestGameplayTagChildren(Tag).IsEmpty();
	}

	FGameplayTag FindEffectDamageType(const UArcEffectDefinition* Definition)
	{
		const FGameplayTag Root = ArcMassMechanicsTags::DamageType;
		for (const FGameplayTag& Tag : ArcEffectHelpers::GetEffectTags(Definition))
		{
			if (Tag != Root && Tag.MatchesTag(Root))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	FGameplayTag GetOutcomeTag(EArcHitOutcome Outcome)
	{
		switch (Outcome)
		{
		case EArcHitOutcome::Hit:
			return ArcMassMechanicsTags::HitOutcome_Hit;
		case EArcHitOutcome::Miss:
			return ArcMassMechanicsTags::HitOutcome_Miss;
		case EArcHitOutcome::Dodge:
			return ArcMassMechanicsTags::HitOutcome_Dodge;
		case EArcHitOutcome::Parry:
			return ArcMassMechanicsTags::HitOutcome_Parry;
		case EArcHitOutcome::Block:
			return ArcMassMechanicsTags::HitOutcome_Block;
		}
		return ArcMassMechanicsTags::HitOutcome;
	}

	void PublishHitResolved(FMassEntityManager& EntityManager, FMassEntityHandle Source, FMassEntityHandle Target,
		const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, FArcHitResolvedPayload&& Payload)
	{
		UArcGameEventSubsystem* EventBus = ArcCombatFunctionsPrivate::GetEventBus(EntityManager);
		if (!EventBus)
		{
			return;
		}

		FArcGameEvent Event;
		Event.Tag = GetOutcomeTag(Payload.Outcome);
		Event.Instigator = Source;
		Event.Target = Target;
		Event.Magnitude = Payload.Damage;
		Event.SourceTags = SourceTags;
		Event.TargetTags = TargetTags;
		Event.Payload = FInstancedStruct::Make(MoveTemp(Payload));
		EventBus->Queue(MoveTemp(Event));
	}

	FArcEffectApplyResult ApplyEffectWithSetByCaller(FMassEntityManager& EntityManager, FMassEntityHandle Target,
		FMassEntityHandle Source, UArcEffectDefinition* Effect, FGameplayTag SetByCallerTag, float Magnitude)
	{
		if (!Effect)
		{
			return FArcEffectApplyResult{};
		}

		FArcEffectApplyRequest Request;
		Request.Definition = Effect;
		Request.Context.SourceEntity = Source;
		Request.SetByCallerMagnitudes.Add(SetByCallerTag, Magnitude);
		return ArcEffects::TryApplyEffect(EntityManager, Target, Request);
	}

	bool IsPeriodicTick(const FArcEffectExecutionContext& Context)
	{
		return Context.Trigger == EArcEffectExecutionTrigger::PeriodicTick;
	}

	FString DescribeHit(const FArcEffectExecutionContext& Context)
	{
		const FArcEffectSpec& Spec = Context.GetOwningSpec();
		return FString::Printf(TEXT("%s %s -> %s%s"), *GetNameSafe(Spec.Definition), *Spec.Context.SourceEntity.DebugGetDescription(),
			*Context.TargetEntity.DebugGetDescription(), IsPeriodicTick(Context) ? TEXT(" (periodic tick)") : TEXT(""));
	}

	void AddFlatAmount(float Flat, float Sign, float IfAmount, float& InOutValue)
	{
		InOutValue = FMath::Max(0.f, InOutValue + Sign * (Flat + IfAmount));
	}

	void ApplyIncomingBonus(const FArcEffectExecutionContext& Context, float Flat, float Percent, float MaxPercent,
		float IfTakenFlat, float IfTakenMultiplier, float& InOutValue)
	{
		AddFlatAmount(Flat, 1.f, IfTakenFlat, InOutValue);
		UE_LOGF(LogArcMassMechanics, Verbose, "Damage %ls: incoming flat bonus %.3f with If DamageTakenFlat %.3f -> %.3f",
			*DescribeHit(Context), Flat, IfTakenFlat, InOutValue);

		const float PercentFactor = 1.f + FMath::Clamp(Percent, 0.f, MaxPercent);
		InOutValue = FMath::Max(0.f, InOutValue * PercentFactor * IfTakenMultiplier);
		UE_LOGF(LogArcMassMechanics, Verbose, "Damage %ls: incoming bonus x%.4f, If DamageTakenMultiplier x%.4f -> %.3f",
			*DescribeHit(Context), PercentFactor, IfTakenMultiplier, InOutValue);
	}

	float Mitigate(const FArcEffectExecutionContext& Context, float Value, const FArcArmorCurve* ArmorCurve, float ArmorK,
		float Armor, float DamageReduction, float Resistance)
	{
		using namespace ArcCombatFunctionsPrivate;

		// Order of combat.yaml mitigation.layers: armor, damage_reduction_pct, resistance.
		if (ArmorCurve)
		{
			Value = ArmorCurve->Mitigate(Value, FMath::Max(0.f, Armor), ArmorK);
			UE_LOGF(LogArcMassMechanics, Verbose, "Damage %ls: armor %.3f (K %.3f) -> %.3f", *DescribeHit(Context), Armor, ArmorK, Value);
		}
		Value = ApplyReduction(Value, DamageReduction);
		UE_LOGF(LogArcMassMechanics, Verbose, "Damage %ls: damage reduction %.4f -> %.3f", *DescribeHit(Context), DamageReduction, Value);
		Value = ApplyReduction(Value, Resistance);
		UE_LOGF(LogArcMassMechanics, Verbose, "Damage %ls: resistance %.4f -> %.3f", *DescribeHit(Context), Resistance, Value);
		return FMath::Max(0.f, Value);
	}

	void PublishHitEvent(const FArcEffectExecutionContext& Context, const FArcEvaluateParameters& HitParams, bool bCrit,
		const FGameplayTag& Zone, const FGameplayTag& DamageType, float Final)
	{
		if (!Context.EntityManager)
		{
			return;
		}

		const FArcEffectSpec& Spec = Context.GetOwningSpec();
		FArcHitResolvedPayload Payload;
		Payload.Outcome = EArcHitOutcome::Hit;
		Payload.Definition = Spec.Definition;
		Payload.bCrit = bCrit;
		Payload.Zone = Zone;
		Payload.DamageType = DamageType;
		Payload.Damage = Final;
		PublishHitResolved(*Context.EntityManager, Spec.Context.SourceEntity, Context.TargetEntity, HitParams.SourceTags, HitParams.TargetTags,
			MoveTemp(Payload));
	}

	void PublishDamageEvent(const FArcEffectExecutionContext& Context, const FArcEvaluateParameters& HitParams, bool bCrit,
		const FGameplayTag& Zone, const FGameplayTag& DamageType, float PreMitigation, float Final)
	{
		if (Final <= 0.f || !Context.EntityManager)
		{
			return;
		}

		UArcGameEventSubsystem* EventBus = ArcCombatFunctionsPrivate::GetEventBus(*Context.EntityManager);
		if (!EventBus)
		{
			return;
		}

		const FArcEffectSpec& Spec = Context.GetOwningSpec();
		FArcDamageDealtPayload Payload;
		Payload.Definition = Spec.Definition;
		Payload.Zone = Zone;
		Payload.bCrit = bCrit;
		Payload.bPeriodicTick = IsPeriodicTick(Context);
		Payload.DamageType = DamageType;
		Payload.PreMitigation = PreMitigation;
		Payload.Final = Final;

		FArcGameEvent Event;
		Event.Tag = ArcMassMechanicsTags::DamageDealt;
		Event.Instigator = Spec.Context.SourceEntity;
		Event.Target = Context.TargetEntity;
		Event.Magnitude = Final;
		Event.SourceTags = HitParams.SourceTags;
		Event.TargetTags = HitParams.TargetTags;
		Event.Payload = FInstancedStruct::Make(MoveTemp(Payload));
		EventBus->Queue(MoveTemp(Event));
	}
}
