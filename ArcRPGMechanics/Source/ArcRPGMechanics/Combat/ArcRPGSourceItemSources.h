// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcRPGSourceItemSources.generated.h"

struct FArcItemData;

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Value read from the source item (the item that granted the ability). Fails without a source item."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_SourceItem : public FArcExtraCaptureSource
{
	GENERATED_BODY()

	virtual bool ResolveValue(const FArcEffectContext& Context, FMassEntityManager& EntityManager, const FArcEvaluateParameters& EvalParams,
		float& OutValue) const override;

protected:
	virtual bool ReadItem(const FArcItemData& Item, const FArcEffectContext& Context, FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams, float& OutValue) const;
};

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Weapon Damage of the source item, for weapon attacks (ArcRPG.AttackClass.Weapon without ArcRPG.AttackClass.Spell in the source tags). Fails for other attacks and without Weapon Damage on the item."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_SourceItemWeaponDamage : public FArcRPGExtraCaptureSource_SourceItem
{
	GENERATED_BODY()

protected:
	virtual bool ReadItem(const FArcItemData& Item, const FArcEffectContext& Context, FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams, float& OutValue) const override;
};

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Weapon Scale of the source item, for weapon attacks. Fails for other attacks and without Weapon Damage on the item."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_SourceItemWeaponScale : public FArcRPGExtraCaptureSource_SourceItem
{
	GENERATED_BODY()

protected:
	virtual bool ReadItem(const FArcItemData& Item, const FArcEffectContext& Context, FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams, float& OutValue) const override;
};

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Skill power of the source item's Skill Scaling: its curve of the source entity's Skill against the skill max. Fails without the fragment, its Skill or Scaling, or the Skill on the source entity."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_SourceItemSkillPower : public FArcRPGExtraCaptureSource_SourceItem
{
	GENERATED_BODY()

protected:
	virtual bool ReadItem(const FArcItemData& Item, const FArcEffectContext& Context, FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams, float& OutValue) const override;
};

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Attribute factor of the source item's Skill Scaling: max(0, 1 + K * modifier of the source entity's Attribute). Fails without the fragment, its Attribute, or the Attribute on the source entity."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_SourceItemAttributeFactor : public FArcRPGExtraCaptureSource_SourceItem
{
	GENERATED_BODY()

protected:
	virtual bool ReadItem(const FArcItemData& Item, const FArcEffectContext& Context, FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams, float& OutValue) const override;
};

namespace ArcRPGSourceItemSources
{
	ARCRPGMECHANICS_API const FArcExtraCaptureDef& GetWeaponDamageDef();

	ARCRPGMECHANICS_API const FArcExtraCaptureDef& GetWeaponScaleDef();

	ARCRPGMECHANICS_API const FArcExtraCaptureDef& GetSkillPowerDef();

	ARCRPGMECHANICS_API const FArcExtraCaptureDef& GetAttributeFactorDef();
}
