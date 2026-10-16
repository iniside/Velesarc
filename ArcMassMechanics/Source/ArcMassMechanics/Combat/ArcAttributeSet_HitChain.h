// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "ArcAttributeSet_HitChain.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Hit chain chances as 0-1 fractions. Miss is read from the attacker, dodge, parry and block from the defender."))
struct ARCMASSMECHANICS_API FArcAttributeSet_HitChain : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Attacker")
	FArcAttribute MissChance;

	UPROPERTY(EditAnywhere, Category = "Defender")
	FArcAttribute DodgeChance;

	UPROPERTY(EditAnywhere, Category = "Defender")
	FArcAttribute ParryChance;

	UPROPERTY(EditAnywhere, Category = "Defender")
	FArcAttribute BlockChance;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_HitChain, MissChance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_HitChain, DodgeChance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_HitChain, ParryChance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_HitChain, BlockChance)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_HitChain>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
