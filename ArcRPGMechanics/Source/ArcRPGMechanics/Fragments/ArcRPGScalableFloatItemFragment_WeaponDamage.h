// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcScalableFloat.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcRPGScalableFloatItemFragment_WeaponDamage.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Weapon Damage", Category = "Combat", ToolTip = "Weapon Damage and Scale of a weapon (D-74, D-75), read from the source item (the item that granted the ability) by the RPG Damage Execution as the base of a weapon attack. The attack defines the damage type; the skill comes from the Skill Scaling fragment on the same item."))
struct ARCRPGMECHANICS_API FArcRPGScalableFloatItemFragment_WeaponDamage : public FArcScalableFloatItemFragment
{
	GENERATED_BODY()

public:
	virtual ~FArcRPGScalableFloatItemFragment_WeaponDamage() override = default;

protected:
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (EnableCategories, ToolTip = "Weapon Damage: the base of a weapon attack."))
	FArcScalableFloat Damage;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGScalableFloatItemFragment_WeaponDamage, Damage);

	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (EnableCategories, ToolTip = "D-75 Scale: a weapon attack reaches Damage * (1 + Scale) at max skill. 0 is fixed damage."))
	FArcScalableFloat Scale;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGScalableFloatItemFragment_WeaponDamage, Scale);
};
