// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/Handlers/ArcRPGNeedRateHandler.h"

#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/Handlers/ArcRPGCoreDerivation.h"
#include "Fragments/ArcNeedFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Modifiers/ArcModifierFunctions.h"

namespace ArcRPGNeedRateHandlerInternal
{
	static float ReadModifier(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcAttributeRef& Ref)
	{
		const FArcAttribute* Attribute = ArcAttributes::ResolveAttribute(EntityManager, Entity, Ref);
		return Attribute ? ArcRPGCoreDerivation::CalculateModifier(Attribute->GetCurrentValue()) : 0.f;
	}
}

void FArcRPGAttributeHandler_NeedRates::Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const
{
	WriteNeedRates(Context, Attribute.GetCurrentValue());
}

void FArcRPGAttributeHandler_NeedRates::PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const
{
	WriteNeedRates(Context, NewValue);
}

void FArcRPGAttributeHandler_NeedRates::OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute,
	const FArcAttributeRef& ChangedRef, float OldValue, float NewValue) const
{
	WriteNeedRates(Context, Attribute.GetCurrentValue());
}

void FArcRPGAttributeHandler_NeedRates::GatherInputs(TArray<FArcAttributeRef>& OutInputs) const
{
	OutInputs.AddUnique(FArcAttributeSet_RPGCoreStats::GetStrengthAttribute());
	OutInputs.AddUnique(FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute());
}

void FArcRPGAttributeHandler_NeedRates::WriteNeedRates(const FArcAttributeHandlerContext& Context, float Constitution) const
{
	const float ConstitutionModifier = ArcRPGCoreDerivation::CalculateModifier(Constitution);
	const float StrengthModifier = ArcRPGNeedRateHandlerInternal::ReadModifier(Context.EntityManager, Context.Entity, FArcAttributeSet_RPGCoreStats::GetStrengthAttribute());
	const float WillpowerModifier = ArcRPGNeedRateHandlerInternal::ReadModifier(Context.EntityManager, Context.Entity, FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute());
	const float PerGameHourToPerSecond = 1.f / FMath::Max(ArcRPGCoreDerivation::ResolveRealSecondsPerGameHour(Context.World), SMALL_NUMBER);
	const float ThresholdShift = ThresholdShiftPerModifier * WillpowerModifier;

	FMassEntityView View(Context.EntityManager, Context.Entity);

	if (FArcHungerNeedFragment* Hunger = View.GetFragmentDataPtr<FArcHungerNeedFragment>())
	{
		Hunger->ChangeRate = HungerPerGameHour * PerGameHourToPerSecond
			* (1.f + HungerConstitutionPerModifier * ConstitutionModifier + HungerStrengthPerModifier * StrengthModifier);
		Hunger->Resistance = ThresholdShift;
	}

	if (FArcThirstNeedFragment* Thirst = View.GetFragmentDataPtr<FArcThirstNeedFragment>())
	{
		Thirst->ChangeRate = ThirstPerGameHour * PerGameHourToPerSecond * (1.f + ThirstConstitutionPerModifier * ConstitutionModifier);
		Thirst->Resistance = ThresholdShift;
	}

	if (FArcFatigueNeedFragment* Fatigue = View.GetFragmentDataPtr<FArcFatigueNeedFragment>())
	{
		Fatigue->ChangeRate = FatiguePerGameHour * PerGameHourToPerSecond * (1.f + FatigueConstitutionPerModifier * ConstitutionModifier);
	}
}
