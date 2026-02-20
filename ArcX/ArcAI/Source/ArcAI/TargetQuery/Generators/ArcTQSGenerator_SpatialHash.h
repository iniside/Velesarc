// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_SpatialHash.generated.h"

/**
 * Generator that gathers Mass Entities from the spatial hash subsystem within a radius.
 */
USTRUCT(DisplayName = "Spatial Hash Radius")
struct ARCAI_API FArcTQSGenerator_SpatialHash : public FArcTQSGenerator
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Generator", meta = (ClampMin = 0.0))
	float Radius = 5000.0f;

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
};
