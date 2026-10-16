// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

#include "ArcAttributeSet_RPGStamina.generated.h"

USTRUCT()
struct ARCRPGMECHANICS_API FArcAttributeSet_RPGStamina : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Stamina;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxStamina;

	UPROPERTY(EditAnywhere)
	FArcAttribute AddStamina;

	UPROPERTY(EditAnywhere)
	FArcAttribute RemoveStamina;

	UPROPERTY(EditAnywhere)
	FArcAttribute StaminaRegenPips;

	UPROPERTY(EditAnywhere)
	FArcAttribute StaminaDegenPips;

	UPROPERTY(EditAnywhere)
	FArcAttribute StaminaPerPip;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, Stamina)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, MaxStamina)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, AddStamina)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, RemoveStamina)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, StaminaRegenPips)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, StaminaDegenPips)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGStamina, StaminaPerPip)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_RPGStamina>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
