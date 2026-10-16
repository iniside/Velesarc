// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Effects/ArcAttributeCapture.h"

#include "ArcAttributeSet_DamageModifiers.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcAttributeSet_DamageModifiers : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingBonus;

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingBonusPercent;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxOutgoingBonusPercent = { 1.f, 1.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingReduction;

	UPROPERTY(EditAnywhere)
	FArcAttribute OutgoingReductionPercent;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxOutgoingReductionPercent = { 1.f, 1.f };

	UPROPERTY(EditAnywhere)
	FArcAttribute IncomingBonus;

	UPROPERTY(EditAnywhere)
	FArcAttribute IncomingBonusPercent;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxIncomingBonusPercent = { 1.f, 1.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, OutgoingBonus)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, OutgoingBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, MaxOutgoingBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, OutgoingReduction)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, OutgoingReductionPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, MaxOutgoingReductionPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, IncomingBonus)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, IncomingBonusPercent)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_DamageModifiers, MaxIncomingBonusPercent)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingBonus)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingBonusPercent)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, MaxOutgoingBonusPercent)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingReduction)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, OutgoingReductionPercent)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_DamageModifiers, MaxOutgoingReductionPercent)
	ARC_DECLARE_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, IncomingBonus)
	ARC_DECLARE_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, IncomingBonusPercent)
	ARC_DECLARE_CAPTURE_TARGET_LIVE(FArcAttributeSet_DamageModifiers, MaxIncomingBonusPercent)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_DamageModifiers>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
