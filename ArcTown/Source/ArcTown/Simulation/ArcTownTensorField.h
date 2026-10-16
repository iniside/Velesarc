// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ArcTownTensorField.generated.h"

struct FArcTownSiteFields;
struct FArcTownSiteSnapshot;

USTRUCT()
struct ARCTOWN_API FArcTownTensorFieldSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tensor Field", meta = (ClampMin = "0.0"))
	double RadialWeight = 1.0;

	UPROPERTY(EditAnywhere, Category = "Tensor Field", meta = (ClampMin = "0.0"))
	double GridWeight = 1.0;

	UPROPERTY(EditAnywhere, Category = "Tensor Field", meta = (ClampMin = "0.0"))
	double BoundaryWeight = 2.5;

	UPROPERTY(EditAnywhere, Category = "Tensor Field", meta = (ClampMin = "0.1", ClampMax = "60.0", Units = "deg"))
	double TerrainHalfWeightSlopeDeg = 3.0;

	UPROPERTY(EditAnywhere, Category = "Tensor Field", meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
	double GridAngleDeg = 0.0;

	UPROPERTY(EditAnywhere, Category = "Tensor Field", meta = (ClampMin = "1.0", Units = "cm"))
	double CoreFalloffCm = 8000.0;
};

struct FArcTownFrozenAttractor
{
	FVector2f CenterTownLocalXY = FVector2f::ZeroVector;

	float Strength = 0.0f;

	double InfluenceRadiusCm = 0.0;

	FGameplayTagContainer DistrictTags;

	FString PathName;
};

struct FArcTownTensorField
{
	const FArcTownSiteSnapshot* Site = nullptr;

	const FArcTownSiteFields* Fields = nullptr;

	TConstArrayView<FArcTownFrozenAttractor> Attractors;

	FArcTownTensorFieldSettings Settings;

	FVector2f CoreTownLocalXY = FVector2f::ZeroVector;
};

namespace ArcTownSiteFrame
{
	ARCTOWN_API FVector2f CellToTownLocalXY(const FArcTownSiteSnapshot& Site, int32 SiteX, int32 SiteY);

	ARCTOWN_API bool TownLocalXYToCell(const FArcTownSiteSnapshot& Site, FVector2f TownLocalXY, FIntPoint& OutCell);

	ARCTOWN_API bool IsInsideSite(const FArcTownSiteSnapshot& Site, FVector2f TownLocalXY);

	ARCTOWN_API FVector2D TownLocalOriginInTerrainLocal(const FArcTownSiteSnapshot& Site);

	ARCTOWN_API FVector2D TownLocalToTerrainLocalXY(const FArcTownSiteSnapshot& Site, const FVector2D& TownLocalXY);

	ARCTOWN_API FVector2D TerrainLocalToTownLocalXY(const FArcTownSiteSnapshot& Site, const FVector2D& TerrainLocalXY);

	ARCTOWN_API bool HasUsableExtentXY(const FBox2D& Bounds);

	ARCTOWN_API bool HasUsableExtentXY(const FBox& Bounds);
}

namespace ArcTownTensorField
{
	ARCTOWN_API double Falloff(double DistanceCm, double RadiusCm);

	// bOutDegenerate: the tensor sum has no dominant axis here, so the returned zero vector carries no direction.
	ARCTOWN_API FVector2f SampleMajorEigenvector(
		const FArcTownTensorField& Field, FVector2f TownLocalXY, bool& bOutDegenerate);

	// Both take IncomingDirection as a hard precondition: zero or non-finite asserts.
	ARCTOWN_API FVector2f SampleMajorDirection(
		const FArcTownTensorField& Field, FVector2f TownLocalXY, FVector2f IncomingDirection);

	ARCTOWN_API FVector2f SampleMinorDirection(
		const FArcTownTensorField& Field, FVector2f TownLocalXY, FVector2f IncomingDirection);
}
