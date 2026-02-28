// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcSettlementTypes.h"
#include "ArcSettlementData.generated.h"

/**
 * Runtime data for a settlement. Owned by the subsystem, not a Mass entity.
 * Settlements are knowledge scopes — a grouping mechanism for entries.
 */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcSettlementData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FArcSettlementHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	float BoundingRadius = 5000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FArcFactionHandle OwnerFaction;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FArcRegionHandle Region;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FGameplayTagContainer Tags;
};

/**
 * Runtime data for a region. Owned by the subsystem.
 * Regions group settlements — distance queries always work,
 * regions provide optional designer-controlled grouping.
 */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcRegionData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	FArcRegionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	float Radius = 50000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	TArray<FArcSettlementHandle> Settlements;
};
