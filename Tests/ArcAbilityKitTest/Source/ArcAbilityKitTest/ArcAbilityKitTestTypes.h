// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Fragments/ArcAbilityCustomDataBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcAbilityKitTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcAbilityKitTest_ResourceSet : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Resource;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxResource;

	UPROPERTY(EditAnywhere)
	FArcAttribute Meta;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAbilityKitTest_ResourceSet, Resource)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAbilityKitTest_ResourceSet, MaxResource)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAbilityKitTest_ResourceSet, Meta)
};

USTRUCT(meta = (Hidden))
struct FArcAbilityKitTest_OtherSource : public FArcAbilityCustomDataBase
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Marker = 0;
};

template<>
struct TMassFragmentTraits<FArcAbilityKitTest_ResourceSet> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
