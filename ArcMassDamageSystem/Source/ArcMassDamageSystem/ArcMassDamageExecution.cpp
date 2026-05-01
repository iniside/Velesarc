// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassDamageExecution.h"

#include "ArcMassDamageTypes.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

float FArcMassTypedDamageExecution::GetAttributeValue(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcAttributeRef& AttrRef)
{
	if (!AttrRef.IsValid())
	{
		return 0.f;
	}

	FMassEntityView EntityView(EntityManager, Entity);
	FStructView FragmentView = EntityView.GetFragmentDataStruct(AttrRef.FragmentType);
	uint8* FragmentMemory = FragmentView.GetMemory();
	if (!FragmentMemory)
	{
		return 0.f;
	}

	FProperty* Prop = AttrRef.GetCachedProperty();
	if (!Prop)
	{
		return 0.f;
	}

	FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
	return Attr ? Attr->GetCurrentValue() : 0.f;
}

void FArcMassTypedDamageExecution::Execute(
	const FArcEffectExecutionContext& Context,
	TArray<FArcEffectModifierOutput>& OutModifiers)
{
	FMassEntityManager* EntityManager = Context.EntityManager;
	if (!EntityManager)
	{
		return;
	}

	const FMassEntityHandle TargetEntity = Context.TargetEntity;
	const FMassEntityHandle SourceEntity = Context.GetSourceEntity();

	if (!TargetEntity.IsValid() || !SourceEntity.IsValid())
	{
		return;
	}

	// Base damage
	float Damage = BaseDamage;

	// Step 1: Source outgoing modifiers
	float OutBonus = GetAttributeValue(*EntityManager, SourceEntity, FArcMassDamageModifiersFragment::GetOutgoingDamageBonusAttribute());
	Damage += OutBonus;

	float OutBonusPct = GetAttributeValue(*EntityManager, SourceEntity, FArcMassDamageModifiersFragment::GetOutgoingDamageBonusPercentAttribute());
	Damage *= (1.f + OutBonusPct);

	float OutReduction = GetAttributeValue(*EntityManager, SourceEntity, FArcMassDamageModifiersFragment::GetOutgoingDamageReductionAttribute());
	Damage -= OutReduction;

	float OutReductionPct = GetAttributeValue(*EntityManager, SourceEntity, FArcMassDamageModifiersFragment::GetOutgoingDamageReductionPercentAttribute());
	Damage *= (1.f - FMath::Clamp(OutReductionPct, 0.f, 1.f));

	// Step 2: Target incoming modifiers
	float InBonus = GetAttributeValue(*EntityManager, TargetEntity, FArcMassDamageModifiersFragment::GetIncomingDamageBonusAttribute());
	Damage += InBonus;

	float InBonusPct = GetAttributeValue(*EntityManager, TargetEntity, FArcMassDamageModifiersFragment::GetIncomingDamageBonusPercentAttribute());
	Damage *= (1.f + InBonusPct);

	// Step 3: Resistance
	float Resistance = GetAttributeValue(*EntityManager, TargetEntity, FArcMassDamageModifiersFragment::GetDamageResistanceAttribute());
	Damage *= (1.f - FMath::Clamp(Resistance, 0.f, 1.f));

	const float FinalDamage = FMath::Max(0.f, Damage);
	if (FinalDamage > 0.f && OutputAttribute.IsValid())
	{
		FArcEffectModifierOutput& Output = OutModifiers.AddDefaulted_GetRef();
		Output.Attribute = OutputAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = FinalDamage;
	}
}
