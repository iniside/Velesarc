// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcInitialAttributesPreset.generated.h"

UCLASS(BlueprintType)
class ARCMASSABILITIES_API UArcInitialAttributesPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Authored attribute fragment defaults. Each FInstancedStruct is a fragment
	 * deriving from FArcMassAttributesBase; edit BaseValue on each FArcAttribute
	 * field directly. The trait composes these onto the entity at spawn time;
	 * the initial-attributes observer fires PostChange for each property so
	 * derived stats cascade.
	 */
	UPROPERTY(EditAnywhere, Category = "Attributes",
		meta=(BaseStruct="/Script/ArcMassAbilities.ArcMassAttributesBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> AttributeFragments;
};
