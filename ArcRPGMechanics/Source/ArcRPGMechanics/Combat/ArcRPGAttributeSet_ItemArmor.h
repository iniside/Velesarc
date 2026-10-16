// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "ArcRPGAttributeSet_ItemArmor.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Item-side defense stat keys: an armor piece carries its armor and intrinsic resistance as Item Stats entries for Armor and Resistance, tagged per damage type. Read by the zone armor and zone resistance capture sources from the defender equipment."))
struct ARCRPGMECHANICS_API FArcRPGAttributeSet_ItemArmor : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Armor")
	FArcAttribute Armor;

	UPROPERTY(EditAnywhere, Category = "Armor")
	FArcAttribute Resistance;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_ItemArmor, Armor)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_ItemArmor, Resistance)
};

template <>
struct TMassFragmentTraits<FArcRPGAttributeSet_ItemArmor>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
