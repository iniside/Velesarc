// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcRPGAttributeSet_CostAffixes.generated.h"

USTRUCT(BlueprintType)
struct ARCRPGMECHANICS_API FArcRPGAttributeSet_CostAffixes : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cost", meta = (ToolTip = "Caster's energy cost affix as a fraction (-0.1 = -10%), from item-stats.yaml magic.energy_cost. The RPG Energy Cost multiplies the whole cost, cascade included, by max(0, 1 + this)."))
	FArcAttribute EnergyCostPct;

	UPROPERTY(EditAnywhere, Category = "Cost", meta = (ToolTip = "Caster's entropy generation affix as a fraction (-0.1 = -10%), from item-stats.yaml magic.entropy_generation. The RPG Entropy Cost multiplies the whole cost by max(0, 1 + this)."))
	FArcAttribute EntropyGenerationPct;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_CostAffixes, EnergyCostPct)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_CostAffixes, EnergyCostPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_CostAffixes, EntropyGenerationPct)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_CostAffixes, EntropyGenerationPct)
};

template <>
struct TMassFragmentTraits<FArcRPGAttributeSet_CostAffixes>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
