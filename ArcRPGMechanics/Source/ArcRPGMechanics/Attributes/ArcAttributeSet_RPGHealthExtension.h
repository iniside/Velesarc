// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcAttributeCapture.h"

#include "ArcAttributeSet_RPGHealthExtension.generated.h"

/**
 * ArcRPG-only extension of the canonical FArcAttributeSet_Health (in
 * ArcMassDamageSystem). Holds the healing/degen meta-attributes that drive
 * Phase B2 resource handlers.
 *
 * Canonical Health/MaxHealth live on FArcAttributeSet_Health;
 * the damage meta-attribute lives on FArcAttributeSet_Damage::Damage.
 */
USTRUCT()
struct ARCRPGMECHANICS_API FArcAttributeSet_RPGHealthExtension : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute HealthHealing;

	UPROPERTY(EditAnywhere)
	FArcAttribute DegenDamage;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGHealthExtension, HealthHealing)
	ARC_DECLARE_CAPTURE_SOURCE_SNAPSHOT(FArcAttributeSet_RPGHealthExtension, HealthHealing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGHealthExtension, DegenDamage)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_RPGHealthExtension>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
