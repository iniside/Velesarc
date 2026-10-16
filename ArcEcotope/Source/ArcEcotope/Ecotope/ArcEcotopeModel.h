// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"
#include "Misc/DataValidation.h"
#include "Simulation/ArcEcotopeDeadwoodSettings.h"
#include "Simulation/ArcForestDisturbanceSettings.h"
#include "Species/ArcEcotopeSpeciesModel.h"

#include "ArcEcotopeModel.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeSpeciesSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species")
	TObjectPtr<UArcEcotopeSpeciesModel> Species;

	/** How many plants of this species per hectare the volume starts with, all of them 30 cm tall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species", meta = (ClampMin = "0.0"))
	float InitialDensityPerHectare = 0.0f;

	/** How many plants per hectare arrive each year from outside the volume. They are already 30 cm tall, so the species' own seedling survival does not apply to them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species", meta = (ClampMin = "0.0"))
	float RegionalArrivalPerHectarePerYear = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species")
	bool bEnabled = true;
};

UCLASS(BlueprintType)
class ARCECOTOPE_API UArcEcotopeModel : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species")
	TArray<FArcEcotopeSpeciesSetup> SpeciesSetups;

	/** Ground foliage drawn by the runtime GPU scatter. It enters no hash, so editing it never invalidates a bake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy")
	TArray<FArcEcotopeGroundFoliageEntry> GroundFoliage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ShowOnlyInnerProperties))
	FArcEcotopeDeadwoodSettings SoilPoolSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ShowOnlyInnerProperties))
	FArcForestDisturbanceSettings DisturbanceSettings;

	/**
	 * How much crown a patch of ground will carry from the stems rooted in it before the stand starts
	 * thinning itself out: 1.0 is a single closed layer, 2.0 is two layers overlapping. Over the limit the
	 * weakest stems are removed until the cell fits again. It is a property of the ground, so it is
	 * authored per ecotope rather than per species.
	 *
	 * Only rooted stems count. Crowns reaching in from neighbouring cells shade the ground without being
	 * charged here, so a cell can visibly carry more cover than the number says.
	 *
	 * A real closed canopy sits near 1.0-1.3. A 1000-year run at 2.0 pinned every cell to the ceiling
	 * and left self-thinning killing more than everything else combined.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy",
		meta = (DisplayName = "Canopy Packing", ClampMin = "0.01"))
	double MaximumCrownOccupancy = 1.3;

	/**
	 * How dark a closed canopy makes the ground beneath it. A plant is only shaded by crowns standing
	 * above its own, so raising this darkens the forest floor and barely touches a canopy tree: in a
	 * closed cell the default leaves a seedling about a quarter of full sun, and 2.0 leaves it under a
	 * tenth, while the tallest stem reads near open sky either way.
	 *
	 * Canopy Packing decides how many crowns fit; this decides how hard they darken what is under them.
	 * It moves growth, death, seeding, what can establish and how much ground cover survives, all
	 * together — change it in small steps and re-run. It does not touch self-thinning, which counts
	 * crowns rather than reading light.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy",
		meta = (DisplayName = "Understory Shade", ClampMin = "0.05", ClampMax = "4.0"))
	double UnderstoryShade = 1.0;

	/**
	 * How thickly ground cover fills the floor: 1.0 is today's density, 2.0 doubles it, 0.0 admits no
	 * new cover and lets what stands die back over years. Trees are untouched.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy",
		meta = (DisplayName = "Ground Cover Density", ClampMin = "0.0", ClampMax = "4.0"))
	double GroundCoverDensity = 1.0;

	/**
	 * How much chance decides which cover species wins a patch of floor, rather than which one suits it
	 * best: 0.0 is pure competition, 0.95 is near-random. Authored and hashed here, but nothing reads it
	 * yet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy",
		meta = (DisplayName = "Ground Cover Patchiness", ClampMin = "0.0", ClampMax = "0.95"))
	double GroundCoverPatchiness = 0.6;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
