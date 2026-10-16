// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcRPGDamageExecution.h"

#include "ArcRPGMechanicsLogs.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_Crit.h"
#include "ArcMassMechanics/Combat/ArcAttributeSet_Mitigation.h"
#include "ArcMassMechanics/Combat/ArcCombatFunctions.h"
#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"
#include "ArcMassMechanics/Triggers/ArcTriggerIf.h"
#include "ArcMassDamageSystem/ArcAttributeSet_Damage.h"
#include "ArcMassDamageSystem/ArcAttributeSet_DamageModifiers.h"
#include "Combat/ArcRPGArmorConfig.h"
#include "Combat/ArcRPGAttributeSet_SpellDamage.h"
#include "Combat/ArcRPGHitZoneConfig.h"
#include "Combat/ArcRPGSourceItemSources.h"
#include "Combat/ArcRPGZoneArmorSource.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Mass/EntityManager.h"

#define LOCTEXT_NAMESPACE "ArcRPGDamageExecution"

namespace ArcRPGDamageExecutionPrivate
{
	float ResolveSnapshot(const FArcEffectExecutionContext& Context, const FArcExtraCaptureDef& Def, float Fallback)
	{
		float Value = 0.f;
		return Context.AttemptResolveExtraCapture(Def, FArcEvaluateParameters(), Value) ? Value : Fallback;
	}

	float ReadPieceStat(const FArcEffectExecutionContext& Context, const FArcRPGExtraCaptureSource_ZoneItemStat& Source, const FArcEvaluateParameters& HitParams)
	{
		float Value = 0.f;
		if (Context.EntityManager)
		{
			Source.ResolveValue(Context.GetOwningSpec().Context, *Context.EntityManager, HitParams, Value);
		}
		return Value;
	}

	bool HasCritSet(const FArcEffectExecutionContext& Context)
	{
		const FMassEntityHandle Source = Context.GetSourceEntity();
		return Context.EntityManager && Context.EntityManager->IsEntityValid(Source)
			&& Context.EntityManager->GetSparseElementDataForEntity(FArcAttributeSet_Crit::StaticStruct(), Source).IsValid();
	}

	FArcEvaluateParameters MakeHitParams(const FArcEffectSpec& Spec)
	{
		FArcEvaluateParameters HitParams;
		HitParams.SourceTags = Spec.CapturedSourceTags;
		HitParams.TargetTags = Spec.CapturedTargetTags;
		return HitParams;
	}
}

FArcRPGDamageExecution::FArcRPGDamageExecution()
{
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_SpellDamage, SpellDamagePct));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_Crit, CritChance));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_Crit, CritMultiplier));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingBonus));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingBonusPercent));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, MaxOutgoingBonusPercent));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingReduction));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingReductionPercent));
	Captures.Add(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, MaxOutgoingReductionPercent));
	Captures.Add(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, IncomingBonus));
	Captures.Add(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, IncomingBonusPercent));
	Captures.Add(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, MaxIncomingBonusPercent));
	Captures.Add(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, DamageReduction));
	Captures.Add(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, Armor));
	Captures.Add(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, Resistance));
}

void FArcRPGDamageExecution::GatherCaptures(TArray<FArcAttributeCaptureDef>& OutCaptures, TArray<FArcExtraCaptureDef>& OutExtras) const
{
	FArcEffectExecution::GatherCaptures(OutCaptures, OutExtras);

	OutExtras.AddUnique(ArcRPGSourceItemSources::GetWeaponDamageDef());
	OutExtras.AddUnique(ArcRPGSourceItemSources::GetWeaponScaleDef());
	OutExtras.AddUnique(ArcRPGSourceItemSources::GetSkillPowerDef());
	OutExtras.AddUnique(ArcRPGSourceItemSources::GetAttributeFactorDef());
}

void FArcRPGDamageExecution::GatherBase(const FArcEffectExecutionContext& Context, EArcRPGAttackPath Path, float& OutBase, float& OutScale) const
{
	OutBase = Base;
	OutScale = Scale;
	if (Path == EArcRPGAttackPath::Weapon)
	{
		const FArcEvaluateParameters NoParams;
		float WeaponDamage = 0.f;
		if (Context.AttemptResolveExtraCapture(ArcRPGSourceItemSources::GetWeaponDamageDef(), NoParams, WeaponDamage))
		{
			OutBase = WeaponDamage;
			OutScale = ArcRPGDamageExecutionPrivate::ResolveSnapshot(Context, ArcRPGSourceItemSources::GetWeaponScaleDef(), 0.f);
			UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: weapon base %.3f scale %.3f", *ArcMechCombat::DescribeHit(Context), OutBase, OutScale);
		}
	}
	OutBase *= WeaponMultiplier;
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: path %d, weapon multiplier x%.4f, base %.3f scale %.3f",
		*ArcMechCombat::DescribeHit(Context), static_cast<int32>(Path), WeaponMultiplier, OutBase, OutScale);
}

void FArcRPGDamageExecution::ReadInputs(const FArcEffectExecutionContext& Context, const FArcEvaluateParameters& HitParams, EArcRPGAttackPath Path,
	FArcRPGDamageInputs& OutInputs) const
{
	OutInputs.SpellDamagePct = Path == EArcRPGAttackPath::Spell
		? Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_SpellDamage, SpellDamagePct), HitParams)
		: 0.f;
	OutInputs.OutgoingBonusPercent = Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingBonusPercent), HitParams);
	OutInputs.MaxOutgoingBonusPercent = Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, MaxOutgoingBonusPercent), HitParams);
	OutInputs.OutgoingReductionPercent = Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingReductionPercent), HitParams);
	OutInputs.MaxOutgoingReductionPercent = Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, MaxOutgoingReductionPercent), HitParams);
	OutInputs.OutgoingBonus = Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingBonus), HitParams);
	OutInputs.OutgoingReduction = Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingReduction), HitParams);
	OutInputs.IncomingBonus = Context.CalcCapturedMagnitude(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, IncomingBonus), HitParams);
	OutInputs.IncomingBonusPercent = Context.CalcCapturedMagnitude(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, IncomingBonusPercent), HitParams);
	OutInputs.MaxIncomingBonusPercent = Context.CalcCapturedMagnitude(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, MaxIncomingBonusPercent), HitParams);
	OutInputs.DamageReduction = Context.CalcCapturedMagnitude(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, DamageReduction), HitParams);
	OutInputs.Armor = Context.CalcCapturedMagnitude(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, Armor), HitParams);
	OutInputs.Resistance = Context.CalcCapturedMagnitude(GET_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, Resistance), HitParams);
}

float FArcRPGDamageExecution::ResolveSourceValue(const FArcEffectExecutionContext& Context, const FArcRPGDamageInputs& Inputs, float InBase,
	float InScale, float CritMultiplier, float IfDoneMultiplier, float IfDoneFlat) const
{
	using namespace ArcRPGDamageExecutionPrivate;

	const float SkillPower = ResolveSnapshot(Context, ArcRPGSourceItemSources::GetSkillPowerDef(), 0.f);
	const float AttributeFactor = ResolveSnapshot(Context, ArcRPGSourceItemSources::GetAttributeFactorDef(), 1.f);
	float Value = FMath::Max(0.f, InBase * (1.f + InScale * SkillPower));
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: skill power %.4f, base %.3f scale %.3f -> %.3f",
		*ArcMechCombat::DescribeHit(Context), SkillPower, InBase, InScale, Value);
	Value *= AttributeFactor;
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: attribute factor x%.4f -> %.3f", *ArcMechCombat::DescribeHit(Context),
		AttributeFactor, Value);

	const float BonusPct = FMath::Clamp(Inputs.OutgoingBonusPercent, 0.f, Inputs.MaxOutgoingBonusPercent);
	const float ReductionPct = FMath::Clamp(Inputs.OutgoingReductionPercent, 0.f, Inputs.MaxOutgoingReductionPercent);
	const float PercentFactor = FMath::Max(0.f, 1.f + Inputs.SpellDamagePct + BonusPct - ReductionPct);
	Value *= PercentFactor;
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: percent term x%.4f (spell %.4f, bonus %.4f, reduction %.4f) -> %.3f",
		*ArcMechCombat::DescribeHit(Context), PercentFactor, Inputs.SpellDamagePct, BonusPct, ReductionPct, Value);
	Value *= IfDoneMultiplier;
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: If DamageDoneMultiplier x%.4f -> %.3f", *ArcMechCombat::DescribeHit(Context), IfDoneMultiplier, Value);
	Value *= CritMultiplier;
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: crit multiplier x%.4f -> %.3f", *ArcMechCombat::DescribeHit(Context), CritMultiplier, Value);

	ArcMechCombat::AddFlatAmount(Inputs.OutgoingBonus, 1.f, IfDoneFlat, Value);
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: outgoing flat bonus %.3f with If DamageDoneFlat %.3f -> %.3f",
		*ArcMechCombat::DescribeHit(Context), Inputs.OutgoingBonus, IfDoneFlat, Value);
	ArcMechCombat::AddFlatAmount(Inputs.OutgoingReduction, -1.f, 0.f, Value);
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: outgoing flat reduction %.3f -> %.3f", *ArcMechCombat::DescribeHit(Context),
		Inputs.OutgoingReduction, Value);
	return Value;
}

void FArcRPGDamageExecution::CaptureApplicationState(const FArcEffectExecutionContext& Context, FInstancedStruct& OutState) const
{
	if (ValueTiming != EArcRPGDamageValueTiming::AtApplication)
	{
		return;
	}

	const FArcEffectSpec& Spec = Context.GetOwningSpec();
	const FArcEvaluateParameters HitParams = ArcRPGDamageExecutionPrivate::MakeHitParams(Spec);
	ReadInputs(Context, HitParams, ArcRPGAttackPath::Resolve(Spec.CapturedSourceTags), OutState.InitializeAs<FArcRPGDamageInputs>());
}

void FArcRPGDamageExecution::Execute(const FArcEffectExecutionContext& Context, TArray<FArcEffectModifierOutput>& OutModifiers) const
{
	using namespace ArcRPGDamageExecutionPrivate;

	const FArcEffectSpec& Spec = Context.GetOwningSpec();
	const FGameplayTag DamageType = ArcMechCombat::FindEffectDamageType(Spec.Definition);
	FArcEvaluateParameters HitParams = MakeHitParams(Spec);
	const EArcRPGAttackPath Path = ArcRPGAttackPath::Resolve(Spec.CapturedSourceTags);
	const bool bApplication = !ArcMechCombat::IsPeriodicTick(Context);

	FMassEntityManager* IfManager = bApplication ? Context.EntityManager : nullptr;
	ArcTriggers::FArcIfHit IfHit;
	IfHit.Spec = &Spec;
	IfHit.Source = Spec.Context.SourceEntity;
	IfHit.Target = Context.TargetEntity;
	IfHit.DamageType = DamageType;

	// combat.yaml pipeline order; crit and zone are rolled before the value so every value read on application sees the zone.
	bool bCrit = false;
	FGameplayTag Zone;
	float CritMultiplier = 1.f;
	float IfDoneMultiplier = 1.f;
	float IfDoneFlat = 0.f;
	if (bApplication)
	{
		float CritChance = 0.f;
		if (HasCritSet(Context)
			&& Context.AttemptCalculateCapturedAttributeMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_Crit, CritChance), HitParams, CritChance))
		{
			IfHit.bCritRolled = false;
			const float IfCritChance = IfManager ? ArcTriggers::GetIfModifier(*IfManager, IfHit, EArcIfChannel::CritChance) : 0.f;
			const float CritRoll = FMath::FRand();
			const float CritRollChance = CritChance + IfCritChance;
			bCrit = CritRoll < CritRollChance;
			UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: crit roll %.4f vs chance %.4f (If CritChance %.4f), crit %d",
				*ArcMechCombat::DescribeHit(Context), CritRoll, CritRollChance, IfCritChance, bCrit ? 1 : 0);
		}

		Zone = HitZoneConfig ? HitZoneConfig->RollZone() : FGameplayTag();
		UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: zone %ls", *ArcMechCombat::DescribeHit(Context), *Zone.ToString());
		if (Zone.IsValid())
		{
			HitParams.TargetTags.AddTag(Zone);
		}
		IfHit.bCritRolled = true;
		IfHit.bCrit = bCrit;
		IfHit.Zone = Zone;

		if (bCrit)
		{
			const float IfCritDamage = IfManager ? ArcTriggers::GetIfModifier(*IfManager, IfHit, EArcIfChannel::CritDamage) : 0.f;
			CritMultiplier = FMath::Max(0.f, Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcAttributeSet_Crit, CritMultiplier), HitParams) + IfCritDamage);
		}

		IfDoneMultiplier = IfManager ? ArcTriggers::GetIfModifier(*IfManager, IfHit, EArcIfChannel::DamageDoneMultiplier) : 1.f;
		IfDoneFlat = IfManager ? ArcTriggers::GetIfModifier(*IfManager, IfHit, EArcIfChannel::DamageDoneFlat) : 0.f;
	}

	const FArcRPGDamageInputs* StoredInputs = !bApplication && ValueTiming == EArcRPGDamageValueTiming::AtApplication && Context.ExecutionState
		? Context.ExecutionState->GetPtr<FArcRPGDamageInputs>()
		: nullptr;
	FArcRPGDamageInputs Inputs;
	if (StoredInputs)
	{
		Inputs = *StoredInputs;
		UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: values stored at application", *ArcMechCombat::DescribeHit(Context));
	}
	else
	{
		ReadInputs(Context, HitParams, Path, Inputs);
	}

	float SourceBase = 0.f;
	float SourceScale = 0.f;
	GatherBase(Context, Path, SourceBase, SourceScale);
	float Value = ResolveSourceValue(Context, Inputs, SourceBase, SourceScale, CritMultiplier, IfDoneMultiplier, IfDoneFlat);

	const float IfTakenFlat = IfManager ? ArcTriggers::GetIfModifier(*IfManager, IfHit, EArcIfChannel::DamageTakenFlat) : 0.f;
	const float IfTakenMultiplier = IfManager ? ArcTriggers::GetIfModifier(*IfManager, IfHit, EArcIfChannel::DamageTakenMultiplier) : 1.f;
	ArcMechCombat::ApplyIncomingBonus(Context, Inputs.IncomingBonus, Inputs.IncomingBonusPercent, Inputs.MaxIncomingBonusPercent,
		IfTakenFlat, IfTakenMultiplier, Value);

	// D-91 5d: a periodic tick strikes no zone, so no piece gives armor or resistance; global Armor and Resistance still apply.
	const float PieceArmor = Zone.IsValid() ? ReadPieceStat(Context, FArcRPGExtraCaptureSource_ZoneArmor(), HitParams) : 0.f;
	const float PieceResistance = Zone.IsValid() ? ReadPieceStat(Context, FArcRPGExtraCaptureSource_ZoneResistance(), HitParams) : 0.f;
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: piece armor %.3f + global %.3f, piece resistance %.4f + global %.4f",
		*ArcMechCombat::DescribeHit(Context), PieceArmor, Inputs.Armor, PieceResistance, Inputs.Resistance);

	const float PreMitigation = Value;
	const float Final = ArcMechCombat::Mitigate(Context, PreMitigation, ArmorConfig ? ArmorConfig->GetArmorCurve() : nullptr,
		ArmorConfig ? ArmorConfig->ArmorK : 0.f, PieceArmor + Inputs.Armor, Inputs.DamageReduction, PieceResistance + Inputs.Resistance);
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG damage %ls: final %.3f (pre-mitigation %.3f, crit %d)",
		*ArcMechCombat::DescribeHit(Context), Final, PreMitigation, bCrit ? 1 : 0);
	if (Final > 0.f)
	{
		FArcEffectModifierOutput& Out = OutModifiers.AddDefaulted_GetRef();
		Out.Attribute = FArcAttributeSet_Damage::GetDamageAttribute();
		Out.Operation = EArcModifierOp::Add;
		Out.Magnitude = Final;
		if (bCrit)
		{
			Out.SourceTags.AddTag(ArcMassMechanicsTags::DamageCrit);
		}
	}

	if (IfManager)
	{
		ArcTriggers::CommitIf(*IfManager, IfHit);
	}

	ArcMechCombat::PublishHitEvent(Context, HitParams, bCrit, Zone, DamageType, Final);
	ArcMechCombat::PublishDamageEvent(Context, HitParams, bCrit, Zone, DamageType, PreMitigation, Final);
}

#if WITH_EDITOR
void FArcRPGDamageExecution::GatherDataWarnings(TArray<FText>& OutWarnings) const
{
	if (!HitZoneConfig)
	{
		OutWarnings.Add(LOCTEXT("NoHitZones", "RPG Damage Execution has no Hit Zone Config: no zone is rolled and no armor piece is struck."));
	}
	if (!ArmorConfig)
	{
		OutWarnings.Add(LOCTEXT("NoArmor", "RPG Damage Execution has no Armor Config: no armor applies."));
	}
}
#endif

#undef LOCTEXT_NAMESPACE
