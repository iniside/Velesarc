// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcRPGAttributeSet_SpellDamage.generated.h"

USTRUCT(BlueprintType)
struct ARCRPGMECHANICS_API FArcRPGAttributeSet_SpellDamage : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ToolTip = "Caster's Spell Damage as a fraction (0.15 = +15%). The RPG Damage Execution adds it to the percent term of spell attack class damage, together with the damage affixes."))
	FArcAttribute SpellDamagePct;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_SpellDamage, SpellDamagePct)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_SpellDamage, SpellDamagePct)
};

template <>
struct TMassFragmentTraits<FArcRPGAttributeSet_SpellDamage>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
