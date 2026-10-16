// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"

#include "ArcAttributeSet_Health.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcAttributeSet_Health : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Health;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxHealth;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Health, Health)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Health, MaxHealth)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_Health>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
