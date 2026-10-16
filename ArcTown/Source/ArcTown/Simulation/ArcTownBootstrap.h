// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "Simulation/ArcTownTensorField.h"

#include "ArcTownBootstrap.generated.h"

struct FArcTownBootstrapReport;
struct FArcTownFieldSettings;
struct FArcTownRoadSettings;
struct FArcTownSiteFields;
struct FArcTownSiteSnapshot;
struct FArcTownWorldSnapshot;

USTRUCT()
struct ARCTOWN_API FArcTownBootstrapSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Bootstrap", meta = (ClampMin = "1.0", Units = "cm"))
	double SeedRoadHalfWidthCm = 300.0;
};

namespace ArcTownBootstrap
{
	ARCTOWN_API bool AreFieldsUsable(const FArcTownSiteSnapshot& Site, const FArcTownSiteFields& Fields);

	ARCTOWN_API double SiteTerm(
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		int32 CellIndex);

	ARCTOWN_API bool ChooseCore(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		TConstArrayView<FArcTownFrozenAttractor> Attractors,
		const FGuid& SimulationId,
		int32 Seed,
		FVector2f& OutCoreTownLocalXY,
		FArcTownBootstrapReport& OutReport,
		FText& OutError);

	ARCTOWN_API bool Seed(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownBootstrapSettings& BootstrapSettings,
		const FArcTownRoadSettings& RoadSettings,
		const FArcTownTensorFieldSettings& TensorSettings,
		TConstArrayView<FArcTownFrozenAttractor> Attractors,
		const FBox2D& VolumeBoundsTownLocalXY,
		FArcTownWorldSnapshot& Snapshot,
		FVector2f& OutCoreTownLocalXY,
		FArcTownBootstrapReport& OutReport,
		FText& OutError);
}
