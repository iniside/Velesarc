// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"
#include "Morphology/ArcTownMorphologyTypes.h"
#include "Simulation/ArcTownBootstrap.h"
#include "Simulation/ArcTownFieldSettings.h"
#include "Simulation/ArcTownPlacement.h"
#include "Simulation/ArcTownRoadExpansion.h"
#include "Simulation/ArcTownTensorField.h"
#include "Simulation/ArcTownTraffic.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownSiteSnapshot.h"

#include "ArcTownSimulationInput.generated.h"

USTRUCT()
struct ARCTOWN_API FArcTownMorphologySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Morphology")
	EArcTownPeriod Period = EArcTownPeriod::Default;

	/**
	 * Bok wielokąta frontowego krótszy od tego nie jest parcelowany — narożniki i skrawki.
	 * Pomnożony przez najgłębszą parcelę daje też minimalne pole kwartału i pasa przyulicznego,
	 * więc kwartał o mniejszym polu w ogóle nie powstaje.
	 */
	UPROPERTY(EditAnywhere, Category = "Morphology", meta = (ClampMin = "0.0", Units = "cm"))
	double MinFrontageRunCm = 800.0;

	/** Odsunięcie fasady od granicy działki. Zero w Roman i Medieval — fasada JEST granicą. */
	UPROPERTY(EditAnywhere, Category = "Morphology", meta = (ClampMin = "0.0", Units = "cm"))
	double SetbackCm = 400.0;

	/** Szum obrotu wokół normalnej boku. Zero w Roman i Medieval — nieregularność idzie z kształtu działki. */
	UPROPERTY(EditAnywhere, Category = "Morphology", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	double YawJitterDeg = 4.0;

	/** Prześwit między sąsiadami w Default. 200 cm = dystans, który właściciel opisał jako dzisiejszy. */
	UPROPERTY(EditAnywhere, Category = "Morphology", meta = (ClampMin = "0.0", Units = "cm"))
	double DefaultPlotGapCm = 200.0;
};

USTRUCT()
struct ARCTOWN_API FArcTownGrowthSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownFieldSettings Fields;

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownBootstrapSettings Bootstrap;

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownTensorFieldSettings TensorField;

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownRoadSettings Roads;

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownTrafficSettings Traffic;

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownPlacementSettings Placement;

	UPROPERTY(EditAnywhere, Category = "Growth")
	FArcTownMorphologySettings Morphology;
};

struct FArcTownSimulationInput
{
	FGuid SimulationId;

	int32 Seed = 0;

	uint64 SiteSnapshotHash = 0;

	uint64 SettingsHash = 0;

	uint32 RuleVersion = 0;

	FArcTownSiteSnapshot Site;

	FArcTownGrowthSettings Settings;

	FBox VolumeBoundsTownLocal = FBox(ForceInit);

	TArray<FArcTownFrozenBuildingEntry> Catalog;

	TArray<FArcTownFrozenAttractor> Attractors;

	int32 YearsToSimulate = 0;

	FArcTownWorldSnapshot SourceSnapshot;
};
