// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Catalog/ArcTownBuildingVisual.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "Simulation/ArcTownTraffic.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcTownBuildingDefinition.generated.h"

UCLASS()
class ARCTOWN_API UArcTownBuildingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Visual", meta = (ExcludeBaseStruct))
	TInstancedStruct<FArcTownBuildingVisual> Visual;

	/** Footprint of the building's walls, in cm — authored, not derived from mesh bounds (those include eaves/porches). */
	UPROPERTY(EditAnywhere)
	FVector2D FootprintXY = FVector2D(400.0, 400.0);

	/** Plot size lower bound, in cm. Zero on a component means "equal to FootprintXY" on that axis. */
	UPROPERTY(EditAnywhere, Category = "Plot", meta = (ClampMin = "0.0", Units = "cm"))
	FVector2D PlotSizeMinXY = FVector2D::ZeroVector;

	/** Plot size upper bound, in cm. Zero on a component means "equal to FootprintXY" on that axis. */
	UPROPERTY(EditAnywhere, Category = "Plot", meta = (ClampMin = "0.0", Units = "cm"))
	FVector2D PlotSizeMaxXY = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere)
	double FloorHeightAbovePivot = 0.0;

	/** Override of where the door faces, interpreted by whichever visual variant is authored: a mesh socket, or a component of the actor class. */
	UPROPERTY(EditAnywhere, Category = "Visual")
	FName DoorSocket = TEXT("Door");

	UPROPERTY(EditAnywhere, Category = "Visual", meta = (InlineEditConditionToggle))
	bool bOverrideDoorLocation = false;

	/** Mesh-local point on the floor plane where the door sits. Drawn in the preview; it does not turn the building. */
	UPROPERTY(EditAnywhere, Category = "Visual", meta = (EditCondition = "bOverrideDoorLocation", Units = "cm"))
	FVector2D DoorLocationXY = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Visual", meta = (InlineEditConditionToggle))
	bool bOverrideDoorForward = false;

	/** Which way the door faces, mesh-local, in the editor's yaw: 0 is +X, and the import convention -Y is -90. */
	UPROPERTY(EditAnywhere, Category = "Visual", meta = (EditCondition = "bOverrideDoorForward", ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
	double DoorForwardYawDeg = -90.0;

	UPROPERTY(EditAnywhere, meta = (Categories = "Town.Building"))
	FGameplayTagContainer BuildingTags;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0", Units = "cm"))
	double InfluenceRadius = 2000.0;

	UPROPERTY(EditAnywhere, meta = (Categories = "Town.District"))
	FGameplayTagContainer PreferredDistricts;

	UPROPERTY(EditAnywhere, meta = (Categories = "Town.Building"))
	FGameplayTagContainer PreferredBuildings;

	UPROPERTY(EditAnywhere, meta = (Categories = "Town.Building"))
	FGameplayTagContainer DenyBuildings;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", Units = "cm"))
	double DenyMinDistance = 0.0;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	int32 EarliestYear = 0;

	UPROPERTY(EditAnywhere, Category = "Plot", meta = (ClampMin = "0", ClampMax = "3"))
	uint8 MinHierarchy = 0;

	UPROPERTY(EditAnywhere, Category = "Plot", meta = (ClampMin = "0", ClampMax = "3"))
	uint8 MaxHierarchy = ArcTownTraffic::MaxRoadHierarchy;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

namespace ArcTownBuildingDefinition
{
	ARCTOWN_API void ResolvePlotSize(
		const UArcTownBuildingDefinition& Definition,
		FVector2D& OutPlotSizeMinXY,
		FVector2D& OutPlotSizeMaxXY);

	ARCTOWN_API FVector2D ResolveDoorForwardXY(
		const UArcTownBuildingDefinition& Definition,
		const FArcTownBuildingVisual& Visual);
}
