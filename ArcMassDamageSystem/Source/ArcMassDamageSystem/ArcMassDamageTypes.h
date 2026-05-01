// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "MassEntityTypes.h"

#include "ArcMassDamageTypes.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassDamageModifiersFragment : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingDamageBonus;

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingDamageBonusPercent;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxOutgoingDamageBonusPercent = { 1.f, 1.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingDamageReduction;

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingDamageReductionPercent;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxOutgoingDamageReductionPercent = { 1.f, 1.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute IncomingDamageBonus;

	UPROPERTY(EditAnywhere)
	FArcAttribute IncomingDamageBonusPercent;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxIncomingDamageBonusPercent = { 1.f, 1.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute DamageResistance;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxDamageResistance = { 1.f, 1.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, OutgoingDamageBonus)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, OutgoingDamageBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, MaxOutgoingDamageBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, OutgoingDamageReduction)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, OutgoingDamageReductionPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, MaxOutgoingDamageReductionPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, IncomingDamageBonus)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, IncomingDamageBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, MaxIncomingDamageBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, DamageResistance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageModifiersFragment, MaxDamageResistance)
};

template <>
struct TMassFragmentTraits<FArcMassDamageModifiersFragment>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassDamageStatsFragment : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute HealthDamage;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassDamageStatsFragment, HealthDamage)
};

template <>
struct TMassFragmentTraits<FArcMassDamageStatsFragment>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
