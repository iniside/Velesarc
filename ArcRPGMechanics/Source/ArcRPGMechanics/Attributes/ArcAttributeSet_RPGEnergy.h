// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcAttributeCapture.h"

#include "ArcAttributeSet_RPGEnergy.generated.h"

USTRUCT()
struct ARCRPGMECHANICS_API FArcAttributeSet_RPGEnergy : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Energy;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxEnergy;

	UPROPERTY(EditAnywhere)
	FArcAttribute AddEnergy;

	UPROPERTY(EditAnywhere)
	FArcAttribute RemoveEnergy;

	UPROPERTY(EditAnywhere)
	FArcAttribute EnergyRegenPips;

	UPROPERTY(EditAnywhere)
	FArcAttribute EnergyDegenPips;

	UPROPERTY(EditAnywhere)
	FArcAttribute EnergyPerPip;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, Energy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, MaxEnergy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, AddEnergy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, RemoveEnergy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, EnergyRegenPips)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, EnergyDegenPips)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEnergy, EnergyPerPip)
	ARC_DECLARE_CAPTURE_SOURCE_SNAPSHOT(FArcAttributeSet_RPGEnergy, RemoveEnergy)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_RPGEnergy>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
