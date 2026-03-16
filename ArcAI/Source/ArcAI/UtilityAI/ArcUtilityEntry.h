// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcUtilityEntry.generated.h"

USTRUCT(BlueprintType)
struct ARCAI_API FArcUtilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Entry")
	FStateTreeStateLink LinkedState;

	UPROPERTY(EditAnywhere, Category = "Entry", meta = (BaseStruct = "/Script/ArcAI.ArcUtilityConsideration"))
	TArray<FInstancedStruct> Considerations;

	UPROPERTY(EditAnywhere, Category = "Entry", meta = (ClampMin = 0.01))
	float Weight = 1.0f;
};
