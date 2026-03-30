// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcMassPersistenceSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ArcMass Persistence"))
class ARCMASS_API UArcMassPersistenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName("ArcMass"); }
	virtual FName GetSectionName() const override { return FName("Persistence"); }

	/** Grid cell size for spatial persistence partitioning (world units). */
	UPROPERTY(config, EditAnywhere, Category = "Grid", meta = (ClampMin = "1.0"))
	float CellSize = 10000.f;

	/** Radius around source positions within which cells are loaded (world units). */
	UPROPERTY(config, EditAnywhere, Category = "Grid", meta = (ClampMin = "0.0"))
	float LoadRadius = 50000.f;
};
