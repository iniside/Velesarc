// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcIceWeatherTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcIceTest_Set : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Integrity;

	UPROPERTY(EditAnywhere)
	FArcAttribute DamageMeta;

	UPROPERTY(EditAnywhere)
	FArcAttribute HealMeta;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcIceTest_Set, Integrity)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcIceTest_Set, DamageMeta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcIceTest_Set, HealMeta)
};

template <>
struct TMassFragmentTraits<FArcIceTest_Set> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
