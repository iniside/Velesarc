// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Combat/ArcMechHitGate.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_HitChain.h"
#include "ArcMassMechanics/Combat/ArcCombatFunctions.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Modifiers/ArcModifierFunctions.h"

namespace ArcMechHitGatePrivate
{
	constexpr float FrontHalfAngleDegrees = 90.f;

	float ReadChance(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcAttributeRef& Attribute,
		const FArcEvaluateParameters& EvalParams)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return 0.f;
		}

		const FArcAttribute* Value = ArcAttributes::ResolveAttribute(EntityManager, Entity, Attribute);
		if (!Value)
		{
			return 0.f;
		}

		const FArcAggregatorFragment* Aggregators = FMassEntityView(EntityManager, Entity).GetFragmentDataPtr<FArcAggregatorFragment>();
		const float Chance = Aggregators && Aggregators->FindAggregator(Attribute)
			? ArcAttributes::EvaluateAttributeWithTagsUnchecked(EntityManager, Entity, Attribute, Value->BaseValue, EvalParams.SourceTags, EvalParams.TargetTags)
			: Value->GetCurrentValue();
		return FMath::Clamp(Chance, 0.f, 1.f);
	}

	bool IsFromBehind(FMassEntityManager& EntityManager, FMassEntityHandle Source, FMassEntityHandle Target)
	{
		if (!EntityManager.IsEntityValid(Source) || !EntityManager.IsEntityValid(Target))
		{
			return false;
		}

		const FTransformFragment* SourceTransform = FMassEntityView(EntityManager, Source).GetFragmentDataPtr<FTransformFragment>();
		const FTransformFragment* TargetTransform = FMassEntityView(EntityManager, Target).GetFragmentDataPtr<FTransformFragment>();
		if (!SourceTransform || !TargetTransform)
		{
			return false;
		}

		const FVector ToSource = (SourceTransform->GetTransform().GetLocation() - TargetTransform->GetTransform().GetLocation()).GetSafeNormal2D();
		if (ToSource.IsNearlyZero())
		{
			return false;
		}

		const FVector Forward = TargetTransform->GetTransform().GetRotation().GetForwardVector().GetSafeNormal2D();
		const float FrontCos = FMath::Cos(FMath::DegreesToRadians(FrontHalfAngleDegrees));
		return FVector::DotProduct(Forward, ToSource) < FrontCos;
	}

	bool RollDefense(FMassEntityManager& EntityManager, const FArcEffectSpec& Spec, FMassEntityHandle Entity,
		const FArcAttributeRef& Attribute, const FArcEvaluateParameters& EvalParams, const TCHAR* DefenseName)
	{
		const float Chance = ReadChance(EntityManager, Entity, Attribute, EvalParams);
		if (Chance > 0.f)
		{
			const float Roll = FMath::FRand();
			const bool bSucceeded = Roll < Chance;
			UE_LOGF(LogArcMassMechanics, Verbose, "Hit gate %ls %ls -> %ls: %ls roll %.4f vs chance %.4f, %ls",
				*GetNameSafe(Spec.Definition), *Spec.Context.SourceEntity.DebugGetDescription(), *Spec.Context.TargetEntity.DebugGetDescription(),
				DefenseName, Roll, Chance, bSucceeded ? TEXT("succeeded") : TEXT("failed"));
			return bSucceeded;
		}
		return false;
	}

	EArcHitOutcome RollDefenses(FMassEntityManager& EntityManager, const FArcEffectSpec& Spec, const FArcEvaluateParameters& EvalParams)
	{
		const FMassEntityHandle Source = Spec.Context.SourceEntity;
		const FMassEntityHandle Target = Spec.Context.TargetEntity;

		if (RollDefense(EntityManager, Spec, Source, FArcAttributeSet_HitChain::GetMissChanceAttribute(), EvalParams, TEXT("Miss")))
		{
			return EArcHitOutcome::Miss;
		}
		if (RollDefense(EntityManager, Spec, Target, FArcAttributeSet_HitChain::GetDodgeChanceAttribute(), EvalParams, TEXT("Dodge")))
		{
			return EArcHitOutcome::Dodge;
		}
		if (IsFromBehind(EntityManager, Source, Target))
		{
			return EArcHitOutcome::Hit;
		}
		if (RollDefense(EntityManager, Spec, Target, FArcAttributeSet_HitChain::GetParryChanceAttribute(), EvalParams, TEXT("Parry")))
		{
			return EArcHitOutcome::Parry;
		}
		if (RollDefense(EntityManager, Spec, Target, FArcAttributeSet_HitChain::GetBlockChanceAttribute(), EvalParams, TEXT("Block")))
		{
			return EArcHitOutcome::Block;
		}
		return EArcHitOutcome::Hit;
	}
}

void FArcMechHitGate::OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
	FArcEffectPreApplicationResult& Result) const
{
	FMassEntityManager* EntityManager = Spec.Context.EntityManager;
	if (!EntityManager)
	{
		return;
	}

	const FMassEntityHandle Source = Spec.Context.SourceEntity;
	const FMassEntityHandle Target = Spec.Context.TargetEntity;
	const FArcEvaluateParameters EvalParams = Spec.MakeEvaluateParameters();

	const EArcHitOutcome Outcome = ArcMechHitGatePrivate::RollDefenses(*EntityManager, Spec, EvalParams);
	UE_LOGF(LogArcMassMechanics, Verbose, "Hit gate %ls %ls -> %ls: outcome %ls",
		*GetNameSafe(Spec.Definition), *Source.DebugGetDescription(), *Target.DebugGetDescription(), *UEnum::GetValueAsString(Outcome));

	if (Outcome == EArcHitOutcome::Hit)
	{
		return;
	}

	FArcHitResolvedPayload Payload;
	Payload.Outcome = Outcome;
	Payload.Definition = Spec.Definition;
	ArcMechCombat::PublishHitResolved(*EntityManager, Source, Target, Spec.CapturedSourceTags, Spec.CapturedTargetTags, MoveTemp(Payload));
	Result.Veto(ArcMechCombat::GetOutcomeTag(Outcome));
}
