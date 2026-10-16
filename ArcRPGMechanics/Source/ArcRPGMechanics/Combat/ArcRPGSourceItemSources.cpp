// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcRPGSourceItemSources.h"

#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/Handlers/ArcRPGCoreDerivation.h"
#include "Combat/ArcRPGAttackPath.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcRPGItemFragment_SkillScaling.h"
#include "Fragments/ArcRPGScalableFloatItemFragment_WeaponDamage.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Mass/EntityManager.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Sources/ArcSourceItemResolver.h"

namespace ArcRPGSourceItemSourcesPrivate
{
	bool HasWeaponDamage(const FArcItemData& Item, const FArcEvaluateParameters& EvalParams)
	{
		return ArcRPGAttackPath::Resolve(EvalParams.SourceTags) == EArcRPGAttackPath::Weapon
			&& ArcItemsHelper::FindScalableItemFragment<FArcRPGScalableFloatItemFragment_WeaponDamage>(&Item);
	}

	bool ReadSourceAttribute(const FArcEffectContext& Context, FMassEntityManager& EntityManager, const FArcAttributeRef& Attribute,
		const FArcEvaluateParameters& EvalParams, float& OutValue)
	{
		const FArcAttribute* AttributePtr = ArcAttributes::ResolveAttribute(EntityManager, Context.SourceEntity, Attribute);
		if (!AttributePtr)
		{
			return false;
		}
		OutValue = ArcAttributes::EvaluateAttributeWithTags(EntityManager, Context.SourceEntity, Attribute, AttributePtr->BaseValue,
			EvalParams.SourceTags, EvalParams.TargetTags);
		return true;
	}

	FArcExtraCaptureDef MakeSnapshot(FInstancedStruct&& Source)
	{
		FArcExtraCaptureDef Def;
		Def.Source = MoveTemp(Source);
		Def.bSnapshot = true;
		return Def;
	}
}

bool FArcRPGExtraCaptureSource_SourceItem::ResolveValue(const FArcEffectContext& Context, FMassEntityManager& EntityManager,
	const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	const FArcItemData* Item = ArcAbilityKit::FindSourceItem(Context);
	return Item && ReadItem(*Item, Context, EntityManager, EvalParams, OutValue);
}

bool FArcRPGExtraCaptureSource_SourceItem::ReadItem(const FArcItemData& Item, const FArcEffectContext& Context, FMassEntityManager& EntityManager,
	const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	return false;
}

bool FArcRPGExtraCaptureSource_SourceItemWeaponDamage::ReadItem(const FArcItemData& Item, const FArcEffectContext& Context,
	FMassEntityManager& EntityManager, const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	if (!ArcRPGSourceItemSourcesPrivate::HasWeaponDamage(Item, EvalParams))
	{
		return false;
	}
	OutValue = FArcRPGScalableFloatItemFragment_WeaponDamage::GetDamageValue(&Item);
	return true;
}

bool FArcRPGExtraCaptureSource_SourceItemWeaponScale::ReadItem(const FArcItemData& Item, const FArcEffectContext& Context,
	FMassEntityManager& EntityManager, const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	if (!ArcRPGSourceItemSourcesPrivate::HasWeaponDamage(Item, EvalParams))
	{
		return false;
	}
	OutValue = FArcRPGScalableFloatItemFragment_WeaponDamage::GetScaleValue(&Item);
	return true;
}

bool FArcRPGExtraCaptureSource_SourceItemSkillPower::ReadItem(const FArcItemData& Item, const FArcEffectContext& Context,
	FMassEntityManager& EntityManager, const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	const FArcRPGItemFragment_SkillScaling* Fragment = ArcItemsHelper::GetFragment<FArcRPGItemFragment_SkillScaling>(&Item);
	const FArcValueScaling* Scaling = Fragment ? Fragment->Scaling.GetPtr<FArcValueScaling>() : nullptr;
	float Skill = 0.f;
	if (!Scaling || !ArcRPGSourceItemSourcesPrivate::ReadSourceAttribute(Context, EntityManager, Fragment->Skill, EvalParams, Skill))
	{
		return false;
	}
	OutValue = Scaling->Power(Skill, ArcRPGSkillScaling::SkillMax);
	return true;
}

bool FArcRPGExtraCaptureSource_SourceItemAttributeFactor::ReadItem(const FArcItemData& Item, const FArcEffectContext& Context,
	FMassEntityManager& EntityManager, const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	const FArcRPGItemFragment_SkillScaling* Fragment = ArcItemsHelper::GetFragment<FArcRPGItemFragment_SkillScaling>(&Item);
	float Attribute = 0.f;
	if (!Fragment || !ArcRPGSourceItemSourcesPrivate::ReadSourceAttribute(Context, EntityManager, Fragment->Attribute, EvalParams, Attribute))
	{
		return false;
	}
	OutValue = FMath::Max(0.f, 1.f + Fragment->K * ArcRPGCoreDerivation::CalculateModifier(Attribute));
	return true;
}

namespace ArcRPGSourceItemSources
{
	const FArcExtraCaptureDef& GetWeaponDamageDef()
	{
		static const FArcExtraCaptureDef Def = ArcRPGSourceItemSourcesPrivate::MakeSnapshot(
			FInstancedStruct::Make(FArcRPGExtraCaptureSource_SourceItemWeaponDamage()));
		return Def;
	}

	const FArcExtraCaptureDef& GetWeaponScaleDef()
	{
		static const FArcExtraCaptureDef Def = ArcRPGSourceItemSourcesPrivate::MakeSnapshot(
			FInstancedStruct::Make(FArcRPGExtraCaptureSource_SourceItemWeaponScale()));
		return Def;
	}

	const FArcExtraCaptureDef& GetSkillPowerDef()
	{
		static const FArcExtraCaptureDef Def = ArcRPGSourceItemSourcesPrivate::MakeSnapshot(
			FInstancedStruct::Make(FArcRPGExtraCaptureSource_SourceItemSkillPower()));
		return Def;
	}

	const FArcExtraCaptureDef& GetAttributeFactorDef()
	{
		static const FArcExtraCaptureDef Def = ArcRPGSourceItemSourcesPrivate::MakeSnapshot(
			FInstancedStruct::Make(FArcRPGExtraCaptureSource_SourceItemAttributeFactor()));
		return Def;
	}
}
