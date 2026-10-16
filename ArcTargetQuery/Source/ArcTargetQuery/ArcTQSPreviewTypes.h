// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcTQSQueryReference.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "ArcTQSPreviewTypes.generated.h"

/** Synthetic entity placed in a TQS preview. */
USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSPreviewMock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Preview")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Preview")
	FTransform Transform;

	UPROPERTY(EditAnywhere, Category = "Preview")
	FGameplayTagContainer OwnedTags;

	UPROPERTY(EditAnywhere, Category = "Preview")
	FArcMassSpatialHashIndexDesc SpatialIndex;
};

/** Editor-only data that drives a query's asset preview. */
USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSPreviewSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Preview")
	FArcTQSPreviewMock Querier;

	UPROPERTY(EditAnywhere, Category = "Preview")
	TArray<FArcTQSPreviewMock> Targets;

	UPROPERTY(EditAnywhere, Category = "Preview")
	FArcTQSQueryReference Reference;

	UPROPERTY(EditAnywhere, Category = "Preview")
	TMap<FGuid, FName> EntityParameterSources;

	UPROPERTY(EditAnywhere, Category = "Preview")
	int32 Seed = 0;
};
