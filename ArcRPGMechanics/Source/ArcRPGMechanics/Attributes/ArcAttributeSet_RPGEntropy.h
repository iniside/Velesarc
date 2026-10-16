// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

#include "ArcAttributeSet_RPGEntropy.generated.h"

USTRUCT()
struct ARCRPGMECHANICS_API FArcAttributeSet_RPGEntropy : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Entropy;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxEntropy;

	UPROPERTY(EditAnywhere)
	FArcAttribute ThresholdEntropy;

	UPROPERTY(EditAnywhere)
	FArcAttribute AddEntropy;

	UPROPERTY(EditAnywhere)
	FArcAttribute RemoveEntropy;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEntropy, Entropy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEntropy, MaxEntropy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEntropy, ThresholdEntropy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEntropy, AddEntropy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGEntropy, RemoveEntropy)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_RPGEntropy>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
