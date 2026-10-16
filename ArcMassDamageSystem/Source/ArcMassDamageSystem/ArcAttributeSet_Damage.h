// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Effects/ArcAttributeCapture.h"

#include "ArcAttributeSet_Damage.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcAttributeSet_Damage : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Damage;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Damage, Damage)
	ARC_DECLARE_CAPTURE_SOURCE_SNAPSHOT(FArcAttributeSet_Damage, Damage)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_Damage>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
