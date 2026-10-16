// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/Handlers/ArcRPGDerivedValueHandlers.h"

#include "ArcMassDamageSystem/ArcAttributeSet_Health.h"
#include "Attributes/ArcAttributeNotification.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGEnergy.h"
#include "Attributes/ArcAttributeSet_RPGEntropy.h"
#include "Attributes/ArcAttributeSet_RPGStamina.h"
#include "Attributes/Handlers/ArcRPGCoreDerivation.h"
#include "Mass/EntityManager.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Survival/ArcSurvivalAttributeFragment.h"

namespace ArcRPGDerivedValueHandlersInternal
{
	static bool ReadAttribute(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcAttributeRef& Ref, float& OutValue)
	{
		const FArcAttribute* Attribute = Ref.IsValid() ? ArcAttributes::ResolveAttribute(EntityManager, Entity, Ref) : nullptr;
		if (!Attribute)
		{
			return false;
		}
		OutValue = Attribute->GetCurrentValue();
		return true;
	}
}

void FArcRPGAttributeHandler_Linear::Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const
{
	Attribute.InitValue(ComputeValue(Context.EntityManager, Context.Entity));
}

void FArcRPGAttributeHandler_Linear::OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute,
	const FArcAttributeRef& ChangedRef, float OldValue, float NewValue) const
{
	const float Value = ComputeValue(Context.EntityManager, Context.Entity);
	if (Value != Attribute.GetBaseValue() && Context.Attribute.IsValid())
	{
		ArcAttributes::QueueSetValue(Context.EntityManager, Context.Entity, Context.Attribute, Value);
	}
}

void FArcRPGAttributeHandler_Linear::GatherInputs(TArray<FArcAttributeRef>& OutInputs) const
{
	for (const FArcRPGChanceTerm& Term : Terms)
	{
		if (Term.Input.IsValid())
		{
			OutInputs.AddUnique(Term.Input);
		}
	}
}

float FArcRPGAttributeHandler_Linear::ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const
{
	float Value = Base;
	for (const FArcRPGChanceTerm& Term : Terms)
	{
		float InputValue = 0.f;
		if (!ArcRPGDerivedValueHandlersInternal::ReadAttribute(EntityManager, Entity, Term.Input, InputValue))
		{
			continue;
		}

		const float Steps = Term.Shape == EArcRPGChanceTermShape::PerModifier ? ArcRPGCoreDerivation::CalculateModifier(InputValue) : InputValue;
		Value += Term.Coefficient * Steps;
	}
	return Value;
}

void FArcRPGAttributeHandler_DerivedValue::Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const
{
	float Value = 0.f;
	if (!ComputeValue(Context.EntityManager, Context.Entity, Value))
	{
		return;
	}

	Attribute.InitValue(Value);
	OnInitialized(Context, Value);
}

void FArcRPGAttributeHandler_DerivedValue::OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute,
	const FArcAttributeRef& ChangedRef, float OldValue, float NewValue) const
{
	float Value = 0.f;
	if (!ComputeValue(Context.EntityManager, Context.Entity, Value) || Value == Attribute.GetBaseValue())
	{
		return;
	}

	if (Context.Attribute.IsValid())
	{
		ArcAttributes::QueueSetValue(Context.EntityManager, Context.Entity, Context.Attribute, Value);
	}
}

bool FArcRPGAttributeHandler_DerivedValue::ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const
{
	return false;
}

void FArcRPGAttributeHandler_DerivedValue::OnInitialized(FArcAttributeHandlerContext& Context, float Value) const
{
}

void FArcRPGAttributeHandler_ResourceMax::PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const
{
	const FArcAttributeRef Resource = GetResourceAttribute();
	FArcAttribute* ResourceAttribute = Resource.IsValid() ? ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Resource) : nullptr;
	if (ResourceAttribute && ResourceAttribute->GetCurrentValue() > NewValue)
	{
		ArcAttributes::SetAttributeDirectAndNotify(Context, Resource, *ResourceAttribute, NewValue);
	}
}

void FArcRPGAttributeHandler_ResourceMax::OnInitialized(FArcAttributeHandlerContext& Context, float Value) const
{
	const FArcAttributeRef Resource = GetResourceAttribute();
	FArcAttribute* ResourceAttribute = Resource.IsValid() ? ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Resource) : nullptr;
	if (!ResourceAttribute)
	{
		return;
	}

	if (bFillResourceOnSpawn || ResourceAttribute->GetBaseValue() > Value)
	{
		ResourceAttribute->InitValue(Value);
	}
}

FArcAttributeRef FArcRPGAttributeHandler_ResourceMax::GetResourceAttribute() const
{
	return FArcAttributeRef();
}

bool FArcRPGAttributeHandler_MaxHealth::ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const
{
	float Constitution = 0.f;
	if (!ArcRPGDerivedValueHandlersInternal::ReadAttribute(EntityManager, Entity, FArcAttributeSet_RPGCoreStats::GetConstitutionAttribute(), Constitution))
	{
		return false;
	}

	OutValue = Base + PerModifier * ArcRPGCoreDerivation::CalculateModifier(Constitution);
	return true;
}

void FArcRPGAttributeHandler_MaxHealth::GatherInputs(TArray<FArcAttributeRef>& OutInputs) const
{
	OutInputs.AddUnique(FArcAttributeSet_RPGCoreStats::GetConstitutionAttribute());
}

FArcAttributeRef FArcRPGAttributeHandler_MaxHealth::GetResourceAttribute() const
{
	return FArcAttributeSet_Health::GetHealthAttribute();
}

bool FArcRPGAttributeHandler_FatiguePooledMax::ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const
{
	float InputValue = 0.f;
	if (!ArcRPGDerivedValueHandlersInternal::ReadAttribute(EntityManager, Entity, GetInputAttribute(), InputValue))
	{
		return false;
	}

	float Fatigue = 0.f;
	ArcRPGDerivedValueHandlersInternal::ReadAttribute(EntityManager, Entity, FArcSurvivalAttributeFragment::GetFatigueAttribute(), Fatigue);

	const float Pool = FMath::Max(PoolFloor, 1.f - Fatigue * FatiguePenaltyPerPoint);
	OutValue = (Base + PerPoint * InputValue) * Pool;
	return true;
}

void FArcRPGAttributeHandler_FatiguePooledMax::GatherInputs(TArray<FArcAttributeRef>& OutInputs) const
{
	const FArcAttributeRef Input = GetInputAttribute();
	if (Input.IsValid())
	{
		OutInputs.AddUnique(Input);
	}
	OutInputs.AddUnique(FArcSurvivalAttributeFragment::GetFatigueAttribute());
}

FArcAttributeRef FArcRPGAttributeHandler_FatiguePooledMax::GetInputAttribute() const
{
	return FArcAttributeRef();
}

FArcRPGAttributeHandler_MaxStamina::FArcRPGAttributeHandler_MaxStamina()
{
	Base = 100.f;
	PerPoint = 5.f;
}

FArcAttributeRef FArcRPGAttributeHandler_MaxStamina::GetResourceAttribute() const
{
	return FArcAttributeSet_RPGStamina::GetStaminaAttribute();
}

FArcAttributeRef FArcRPGAttributeHandler_MaxStamina::GetInputAttribute() const
{
	return FArcAttributeSet_RPGCoreStats::GetConstitutionAttribute();
}

FArcRPGAttributeHandler_MaxEnergy::FArcRPGAttributeHandler_MaxEnergy()
{
	Base = 50.f;
	PerPoint = 10.f;
}

FArcAttributeRef FArcRPGAttributeHandler_MaxEnergy::GetResourceAttribute() const
{
	return FArcAttributeSet_RPGEnergy::GetEnergyAttribute();
}

FArcAttributeRef FArcRPGAttributeHandler_MaxEnergy::GetInputAttribute() const
{
	return FArcAttributeSet_RPGCoreStats::GetMagicAttribute();
}

FArcRPGAttributeHandler_MaxEntropy::FArcRPGAttributeHandler_MaxEntropy()
{
	bFillResourceOnSpawn = false;
}

bool FArcRPGAttributeHandler_MaxEntropy::ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const
{
	float Willpower = 0.f;
	if (!ArcRPGDerivedValueHandlersInternal::ReadAttribute(EntityManager, Entity, FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute(), Willpower))
	{
		return false;
	}

	OutValue = ThresholdBase + ThresholdPerPoint * Willpower + Headroom;
	return true;
}

void FArcRPGAttributeHandler_MaxEntropy::GatherInputs(TArray<FArcAttributeRef>& OutInputs) const
{
	OutInputs.AddUnique(FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute());
}

FArcAttributeRef FArcRPGAttributeHandler_MaxEntropy::GetResourceAttribute() const
{
	return FArcAttributeSet_RPGEntropy::GetEntropyAttribute();
}
