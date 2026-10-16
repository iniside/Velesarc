// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBank.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "Misc/DataValidation.h"
#include "Species/ArcForestStageBand.h"

#include "ArcEcotopeSpeciesModel.generated.h"

namespace UE::ArcForest
{
	inline constexpr double GrowthSpanTopFraction = 0.8;

	/** Recruits enter the simulation at 30 cm, which is where the growth span of a form that never promotes starts. */
	inline constexpr double RecruitHeightCm = 30.0;

	/** The height span the derived annual growth rate is solved across; strictly positive is a validity requirement. */
	inline double CalculateGrowthSpanCm(const double MaxHeightCm, const double SpanBaseCm)
	{
		return GrowthSpanTopFraction * MaxHeightCm - SpanBaseCm;
	}

	/** Light never exceeds 1, so a light Maximum above 1 is an off-scale anchor whose falling limb is still partial at full sun instead of reaching zero. */
	inline constexpr float MaximumLightResponseBound = 2.5f;

	/** The temperature band is centred on a preference and may reach off the measured [0, 1] scale at either end. */
	inline constexpr float MinimumTemperatureResponseBound = -1.0f;
	inline constexpr float MaximumTemperatureResponseBound = 2.0f;
}

/**
 * A triangular response: best at Optimum, tapering to nothing at Minimum and Maximum, and exactly zero
 * outside them. Setting Optimum equal to an endpoint makes it a ramp rather than a peak.
 *
 * Every field of it is derived from the species' authored knobs; none of them is authored directly.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestResponseRange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Response")
	float Minimum = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Response")
	float Optimum = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Response")
	float Maximum = 1.0f;
};

/**
 * One clonal clumping pattern of a patch-form species. Only the patch path draws
 * these — individuals, cohorts and deadwood are always scattered independently.
 *
 * An EMPTY array is not "no clustering" — it means the species takes the
 * arcecotope.Instances.PatchCluster* console defaults.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestCoverClusterLayer
{
	GENERATED_BODY()

	/**
	 * Relative share of the record's instances this layer draws, against the
	 * other layers of the same species. Only the ratios matter, exactly as
	 * FArcForestStageMeshVariant::SelectionWeight: a layer that should never
	 * appear is deleted, not weighted to zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0001"))
	float Weight = 1.0f;

	/** Radius of one clump of this layer, measured from its centre, and half the spacing of the clump site lattice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float ClusterRadiusCm = 250.0f;

	/** Instances a clump of this layer is worth: it divides the local instance density to give the chance one clump site activates. One or less scatters uniformly with no clumping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1"))
	int32 InstancesPerCluster = 12;

	/** How strongly fertility, moisture and bare rock at a clump site decide whether it activates. Zero places clumps without regard to the ground under them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SiteBiasStrength = 0.0f;
};

UCLASS(Abstract, BlueprintType)
class ARCECOTOPE_API UArcEcotopeSpeciesModel : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identity used by snapshots and deterministic random keys. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SpeciesId;

	/**
	 * How crowded a mature stand of this species alone should look, in plants per hectare. It is what
	 * sets how wide a crown of this species spreads.
	 *
	 * Read it as a look, not a prediction. Crowns grow with each plant, so a real stand fits more plants
	 * than this and re-running is the only way to know how many — trust the picture, not the number.
	 *
	 * The floor is where a crown stops being representable: below 20 a full-grown plant is handed a crown
	 * wider than the simulation can spread it over. For ground cover this is also what a patch fills a
	 * cell to, so the crown width and the cover density cannot disagree.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "20"))
	int32 TargetStemsPerHectare = 120;

	/**
	 * How tall a full-grown plant of this species stands, in metres — the number a designer says out
	 * loud. Maximum trunk thickness, lifespan and age of first reproduction all follow from it: a bigger
	 * plant takes longer to build and lives proportionally longer.
	 *
	 * On a species that stays in patches this is frond or tussock height, and the lifespan comes off a
	 * separate short-life curve — one rule cannot cover both a 35 m tree and a 1.2 m fern.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.3", ClampMax = "120.0", Units = "m"))
	float TreeSizeM = 30.0f;

	/**
	 * Trunk build, from stocky at 0 to whip-thin at 1: how many centimetres of height this species
	 * carries per centimetre of trunk, 20 at 0 and 60 at 1. Maximum trunk thickness and the yearly
	 * thickening both follow from it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Slenderness = 0.5f;

	/**
	 * How fast this species climbs to 80% of its full height, as a fraction of its lifespan: 0 spends
	 * half of its life getting there, 1 spends a fifth. The trunk thickens to match.
	 *
	 * It does NOT move the age of first reproduction, which only gates seeding.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrowthSpeed = 0.5f;

	/**
	 * How well a plant survives a bad year, from a short-lived pioneer at 0 to a long-lived climax tree
	 * at 1. It sets the everyday risk of dying, how hard starvation bites, how poor conditions have to
	 * get before it does, and how sharply old age arrives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Toughness = 0.5f;

	/**
	 * How little light this species survives on. 0 is a pioneer that dies under a closed canopy; 1 is a
	 * climax species that hangs on in deep shade.
	 *
	 * On an individualizing form the peak stays pinned at full sun and the knob moves only the minimum.
	 * On a patch form it also moves the peak down, so a shade plant is genuinely worse off in the open
	 * than under a canopy. It also decides seed output: a pioneer sows far more than a climax species.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShadeTolerance = 0.5f;

	/**
	 * The centre of the temperature band this species lives in, from boreal at 0 through temperate at
	 * 0.5 to hot at 1. It is a preference, not a climate: the volume states the actual climate, and the
	 * same asset scores differently in a hot volume and a cold one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreferredTemperature = 0.5f;

	/**
	 * Half-width of that band: the species scores zero at PreferredTemperature plus or minus this, and
	 * best at the centre. The default spans the whole measured range, so a species excludes no ground on
	 * temperature until this is narrowed deliberately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TemperatureTolerance = 1.0f;

	/**
	 * How much water this species demands, from arid-tolerant at 0 through indifferent at 0.5 to
	 * rainforest at 1. Only the wet half restricts anything: it raises the dryness floor below which the
	 * species will not grow, and pulls the wind exposure ceiling down. An arid preference is expressed by
	 * the ABSENCE of that floor, so the bottom half of the range is flat.
	 *
	 * There is no upper limit on dampness: the ground reads 1 along every watercourse and on ordinary
	 * flat soil, and a species that scored zero there would empty most of the volume.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreferredHumidity = 0.5f;

	/** The steepest ground this species holds. Above it the species is excluded outright. Applies only to a form with a stem to hold up; a form with none derives the full 90 regardless of what is authored here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float MaxSlopeDeg = 62.0f;

	/**
	 * How far the authored exclusion masks — road cuts, bare rock, watercourses, trampling and moving
	 * substrate — push this species back. 0 ignores them entirely and 1 takes the full authored cut, so
	 * ground cover set low survives right to the edge of a road that clears every tree over it.
	 *
	 * It decides what may be placed, and on a ground-cover species it also shrinks what already stands.
	 * Trees already standing are untouched by it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExclusionSensitivity = 1.0f;

	/**
	 * How much moving substrate — scree, a slumping face — kills plants of this species that already
	 * stand on it. It is a mortality term, not a placement mask: 0 lets a species hold ground that is
	 * sliding out from under it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MobilityMortalitySensitivity = 1.0f;

	/**
	 * Shared by every band whose visual is a static mesh. It is authored with its mesh field empty; the
	 * builder stamps each variant's mesh onto its own component.
	 */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	FISMComponentDescriptor StaticMeshDescriptor;

	/** Shared by every band whose visual is a skinned mesh. See StaticMeshDescriptor for the empty-descriptor idiom. */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	FSkinnedMeshComponentDescriptor SkinnedMeshDescriptor;

	/** See EArcForestGridSize. */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	EArcForestGridSize GridSize = EArcForestGridSize::Cm12800;

	/** Runtime grid the partition actors join. It also salts their grid guid, so changing it re-homes every actor of this species. */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	FName RuntimeGridName;

	/**
	 * The size bands this species draws in, each choosing its own visual: static mesh, skinned mesh or
	 * entity.
	 *
	 * A kind with no band at all draws nothing, silently — the builder logs how many instances it
	 * skipped and carries on. The five dead kinds are the ones forgotten in practice, and a long run
	 * accumulates far more dead wood than living plants: one 238-year run reached 45k dead records
	 * against 3k live ones, all of it invisible because the species had no mesh for any of them.
	 *
	 * Gaps between bands are legal — a plant of an uncovered size draws with the nearest band, and the
	 * builder counts how often that happened. A large count means the bands do not cover the sizes this
	 * species actually grows to.
	 */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	TArray<FArcForestStageBand> Bands;

	/** Terrain weight channel this species' cover paints into. None paints nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	FName TerrainCoverChannel;

	UArcEcotopeSpeciesModel();

	virtual void PostInitProperties() override;

	// The subclass fields are read through these so the ecology hash stays one base implementation, field for field with its frozen twin.
	virtual bool Individualizes() const PURE_VIRTUAL(Individualizes, return false;);
	virtual float GetMeanDispersalDistanceCm() const { return 0.0f; }
	virtual float GetRecruitsPerMatureStemPerYear() const { return 0.0f; }
	virtual float GetLateralSpreadRatePerYear() const { return 0.0f; }
	virtual float GetIndividualizationHeightCm() const { return static_cast<float>(UE::ArcForest::RecruitHeightCm); }
	virtual TConstArrayView<FArcForestCoverClusterLayer> GetCoverClusterLayers() const { return {}; }

	/** Canonical hash of identity, the authored knobs and everything they derive. */
	uint64 ComputeEcologyHash() const;

	/** Canonical hash of identity and visual fields only. */
	uint64 ComputeVisualHash() const;

#if WITH_EDITOR
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS(BlueprintType)
class ARCECOTOPE_API UArcEcotopeTreeSpeciesModel : public UArcEcotopeSpeciesModel
{
	GENERATED_BODY()

public:
	/**
	 * How far a seed travels on average, in a random direction. Most land within about this distance of
	 * the parent, but a few go two or three times further, and those rare long throws are what colonise
	 * open ground.
	 *
	 * 0 drops every seedling in the parent's own cell, giving tight clumps that never spread. Compare it
	 * against the volume's CellSizeCm — anything well below one cell is effectively 0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", Units = "cm"))
	float MeanDispersalDistanceCm = 1000.0f;

	/**
	 * New plants one full-grown, healthy mature plant of this species adds per year, after everything
	 * that kills a seed first. It is the knob to reach for when a species seeds itself too thickly or too
	 * thinly. A smaller or stressed plant contributes proportionally less, and ShadeTolerance scales the
	 * whole figure: a pioneer sows more than this and a climax species less.
	 *
	 * It applies only to seed made here. Regional arrivals set on the volume skip it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0"))
	float RecruitsPerMatureStemPerYear = 1.0f;

	/**
	 * The height at which a group of seedlings stops being one anonymous entry and becomes plants with
	 * their own positions on the ground.
	 *
	 * This is the biggest cost knob in the whole system. Below it thousands of seedlings cost a handful
	 * of records; above it every plant costs one, for the rest of its life. Halving this value can
	 * multiply the record count tenfold, and that shows up in run time, save size and instance count.
	 *
	 * Set it where individual placement first starts to matter to the eye — for a forest tree that is
	 * well above knee height, not at the seedling stage. It must sit above 30 cm, and it is pulled down
	 * to 60% of the full height if TreeSizeM shrinks under it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size and Lifespan", meta = (ClampMin = "30.001", Units = "cm"))
	float IndividualizationHeightCm = 100.0f;

	virtual bool Individualizes() const override { return true; }
	virtual float GetMeanDispersalDistanceCm() const override { return MeanDispersalDistanceCm; }
	virtual float GetRecruitsPerMatureStemPerYear() const override { return RecruitsPerMatureStemPerYear; }
	virtual float GetIndividualizationHeightCm() const override { return IndividualizationHeightCm; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS(BlueprintType)
class ARCECOTOPE_API UArcEcotopeLowGroundCoverSpeciesModel : public UArcEcotopeSpeciesModel
{
	GENERATED_BODY()

public:
	/**
	 * How fast a patch creeps sideways into the four cells around it each year, as
	 * a share of its own cover. These species make no seed at all, so this is the
	 * only way they reach new ground — at 0 a patch stays exactly where it started.
	 *
	 * It sets the speed of the spread, not how thick the result is: an arriving
	 * patch still fills only to what TargetStemsPerHectare and the ground allow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LateralSpreadRatePerYear = 0.05f;

	/**
	 * How this species clumps on the ground, one entry per pattern running at once
	 * — a fine scatter and a few broad drifts, say. Empty takes the console
	 * defaults rather than scattering evenly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest",
		meta = (TitleProperty = "{InstancesPerCluster} per clump, {ClusterRadiusCm} cm"))
	TArray<FArcForestCoverClusterLayer> CoverClusterLayers;

	virtual bool Individualizes() const override { return false; }
	virtual float GetLateralSpreadRatePerYear() const override { return LateralSpreadRatePerYear; }
	virtual TConstArrayView<FArcForestCoverClusterLayer> GetCoverClusterLayers() const override { return CoverClusterLayers; }
};

namespace ArcForestSpeciesModel
{
	/**
	 * Crown area per unit ground the density solve targets. It is a constant and
	 * deliberately not the volume's MaximumCrownOccupancy: one species asset
	 * serves several volumes and must not derive differently in each.
	 */
	inline constexpr double ReferenceCanopyPacking = 1.3;

	/** The density solve honours this whatever the caller passes; it mirrors the TargetStemsPerHectare ClampMin meta. */
	inline constexpr double MinimumTargetStemsPerHectare = 20.0;

	/** Likewise for TreeSizeM: ClampMin is a numeric-entry filter that text import and a direct caller both bypass. */
	inline constexpr double MinimumTreeSizeM = 0.3;
	inline constexpr double MaximumTreeSizeM = 120.0;

	/** Everything the authored knobs solve for. No field of it is authored, stored or serialized anywhere. */
	struct FArcForestDerivedEcology
	{
		float MaxHeightCm = 3000.0f;
		float MaxDbhCm = 75.0f;
		int32 MaxAgeYears = 270;
		int32 MaturityAgeYears = 32;
		float HeightDbhAllometryCoefficient = 40.0f;
		float HeightDbhAllometryExponent = 1.0f;
		/** Clamped down against the derived MaxHeightCm; authored otherwise, and passed straight back on a form that never promotes. */
		float IndividualizationHeightCm = 100.0f;
		float CrownRadiusHeightRatio = 0.2f;
		float MaxAnnualHeightGrowthCm = 50.0f;
		float MaxAnnualDbhGrowthCm = 1.0f;
		float BiomassAllometryCoefficient = 0.1f;
		float BiomassAllometryExponent = 2.5f;
		float CrownOccupancyWeight = 1.0f;
		float BaseAnnualMortalityProbability = 0.01f;
		double OldAgeMortalityProbabilityAtMaxAge = 0.25;
		double StressMortalityMaximum = 0.6;
		double StressMortalityVigorThreshold = 0.24;
		double StressMortalitySteepness = 25.0;
		double StressMortalityCanopyFloor = 0.2;
		double VigorSmoothingRetention = 0.8;
		/** RecruitsPerMatureStemPerYear after the ShadeTolerance pioneer scaling. */
		float AnnualPropaguleProduction = 1.0f;
		FArcForestResponseRange LightResponse = {0.0f, 1.0f, 1.0f};
		/** A one-sided shoulder: ordinary and wet ground read exactly 1 and must stay at the optimum. */
		FArcForestResponseRange MoistureResponse = {0.0f, 1.0f, 1.0f};
		/** A band centred on the preference; its bounds may sit off the measured [0, 1] scale. */
		FArcForestResponseRange TemperatureResponse = {-0.5f, 0.5f, 1.5f};
		float MaxSlopeDeg = 62.0f;
		float SunLoadMin = 0.0f;
		float WindExposureMax = 1.0f;
		float FertilityMin = 0.0f;
	};

	/**
	 * Solves the whole detailed ecology from the authored knobs, in two phases: size, age and allometry
	 * first, then the growth and mortality that read them.
	 *
	 * The one failure is a non-positive growth span: an individualizing form needs a TreeSizeM whose 60%
	 * clamp of IndividualizationHeightCm still leaves it above the 30 cm recruitment boundary, and a form
	 * that never promotes spans from that boundary instead, so it needs a TreeSizeM above 0.375 m.
	 *
	 * The curves are a pure function of the species' own preferences and never of a volume: one asset
	 * serves N volumes with N climates, and the freeze stamps the ecology hash that ValidateSpecies
	 * recomputes, so a curve shifted per volume aborts every run.
	 */
	ARCECOTOPE_API bool DeriveEcology(
		const int32 TargetStemsPerHectare,
		const float TreeSizeM,
		const float Slenderness,
		const float GrowthSpeed,
		const float Toughness,
		const float ShadeTolerance,
		const float PreferredTemperature,
		const float TemperatureTolerance,
		const float PreferredHumidity,
		const float MaxSlopeDeg,
		const float RecruitsPerMatureStemPerYear,
		const bool bIndividualizes,
		const float IndividualizationHeightCm,
		FArcForestDerivedEcology& OutDerived,
		FText& OutError);

	/** The same solve, reading the knobs straight off an asset. */
	ARCECOTOPE_API bool DeriveEcology(
		const UArcEcotopeSpeciesModel& Species,
		FArcForestDerivedEcology& OutDerived,
		FText& OutError);
}
