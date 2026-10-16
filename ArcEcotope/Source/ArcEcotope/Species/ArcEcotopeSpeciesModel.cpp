// Copyright Lukasz Baran. All Rights Reserved.

#include "Species/ArcEcotopeSpeciesModel.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/StringConv.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"

namespace ArcForestSpeciesModel
{
	class FCanonicalHash64
	{
	public:
		void AddUInt8(const uint8 Value)
		{
			Hash ^= Value;
			Hash *= Prime;
		}

		void AddUInt32(const uint32 Value)
		{
			AddUInt8(static_cast<uint8>(Value));
			AddUInt8(static_cast<uint8>(Value >> 8));
			AddUInt8(static_cast<uint8>(Value >> 16));
			AddUInt8(static_cast<uint8>(Value >> 24));
		}

		void AddInt32(const int32 Value)
		{
			AddUInt32(static_cast<uint32>(Value));
		}

		void AddUInt64(const uint64 Value)
		{
			AddUInt32(static_cast<uint32>(Value));
			AddUInt32(static_cast<uint32>(Value >> 32));
		}

		void AddFloat(const float Value)
		{
			AddUInt32(Value == 0.0f ? 0u : FPlatformMath::AsUInt(Value));
		}

		void AddDouble(const double Value)
		{
			AddUInt64(Value == 0.0 ? 0ull : FPlatformMath::AsUInt(Value));
		}

		void AddString(const FString& Value)
		{
			const FTCHARToUTF8 Utf8(*Value);
			AddUInt64(static_cast<uint64>(Utf8.Length()));
			for (int32 Index = 0; Index < Utf8.Length(); ++Index)
			{
				AddUInt8(static_cast<uint8>(Utf8.Get()[Index]));
			}
		}

		void AddGuid(const FGuid& Value)
		{
			AddUInt32(Value.A);
			AddUInt32(Value.B);
			AddUInt32(Value.C);
			AddUInt32(Value.D);
		}

		void AddObjectPath(const UObject* Object)
		{
			AddString(Object ? FSoftObjectPath(Object).ToString() : FString());
		}

		uint64 Finalize() const
		{
			return Hash;
		}

	private:
		static constexpr uint64 OffsetBasis = 14695981039346656037ull;
		static constexpr uint64 Prime = 1099511628211ull;
		uint64 Hash = OffsetBasis;
	};

	void AddResponse(FCanonicalHash64& Hash, const FArcForestResponseRange& Response)
	{
		Hash.AddFloat(Response.Minimum);
		Hash.AddFloat(Response.Optimum);
		Hash.AddFloat(Response.Maximum);
	}

	template <typename VisualType>
	bool HasBandVisual(const TArray<FArcForestStageBand>& Bands)
	{
		for (const FArcForestStageBand& Band : Bands)
		{
			if (Band.Visual.GetPtr<VisualType>() != nullptr)
			{
				return true;
			}
		}
		return false;
	}

	bool DeriveEcology(
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
		FText& OutError)
	{
		// The habitat a real stand settles at, and the time-weighted mean of the size slowdown across the growth span.
		constexpr double ReferenceResource = 0.40;
		constexpr double MeanSizeLimitation = 0.88;

		// Centimetres of height per centimetre of diameter at the two ends of Slenderness, bracketing with
		// headroom what the six shipped species author: 25 on the covers, 29 beech, 31 maple, 43 pine, 56 aspen.
		constexpr double StoutCoefficient = 20.0;
		constexpr double SlenderCoefficient = 60.0;
		constexpr double AllometryExponent = 1.0;

		// Weight against trunk thickness; the classic forestry pair, and no shipped species ever moved it.
		constexpr double BiomassCoefficient = 0.1;
		constexpr double BiomassExponent = 2.5;

		// Lifespan against size and Toughness, fitted to the four shipped trees within 11%. A form that never
		// individualizes takes its own near-linear curve because the tree one puts a 1.2 m plant at 0.02 years.
		constexpr double ReferenceAgeYears = 270.0;
		constexpr double ReferenceSizeM = 30.0;
		constexpr double SizeAgeExponent = 2.9;
		constexpr double CoverReferenceAgeYears = 28.0;
		constexpr double CoverReferenceSizeM = 1.0;
		constexpr double CoverSizeAgeExponent = 1.0;
		constexpr double ShortLifeFactor = 0.15;
		constexpr double LongLifeFactor = 1.85;

		constexpr double MaturityFraction = 0.12;

		// The promotion height is clamped well under the 80% top of the growth span, so shrinking TreeSizeM
		// collapses neither the span the growth solve covers nor the run-time height validation.
		constexpr double IndividualizationCeilingFraction = 0.6;

		// Starvation shape and canopy protection: no shipped species ever moved either, and both are
		// calibrated against a stand vigor that is a run outcome rather than an authored number.
		constexpr double StressSteepness = 25.0;
		constexpr double StressCanopyFloor = 0.2;
		constexpr double VigorRetention = 0.8;

		// A patch charges its neighbours the same crown budget a stem does; no shipped species differed.
		constexpr double CrownOccupancy = 1.0;

		// Pioneer syndrome: the seed output a species gets on top of, or off, its authored recruit rate.
		// Both ramps leave the neutral 0.5 preference at exactly the authored figure.
		constexpr double PioneerSeedGain = 2.0;
		constexpr double ClimaxSeedCut = 0.5;

		// The surviving site bounds, fitted to the six shipped assets. Only the restrictive half of each
		// preference does anything: the permissive half would have to place a bound the terrain never
		// crosses, and EvaluateResponse reads zero AT a bound rather than beyond it.
		constexpr double MoistureFloorAtFullHumidity = 0.40;
		constexpr double FertilityFloorAtFullCold = 0.90;
		constexpr double FertilityFloorAtFullHumidity = 0.30;
		// The reference 260-year run reports fertility min/mean/max 0.0295 / 0.8974 / 1.0000, so a floor
		// above this excludes the typical cell rather than the poor tail.
		constexpr double MaximumFertilityFloor = 0.60;
		constexpr double WindCeilingDropAtFullHumidity = 0.45;
		// Level ground with nothing upwind bakes exposure exactly 0.5, which a dry volume's bias raises to
		// 0.60, and the ceiling excludes at and above itself. A ceiling below that empties every flat cell
		// and fails the bootstrap outright, so the drop stops short of it.
		constexpr double MinimumWindCeiling = 0.65;
		constexpr double SunLoadFloorAtFullWarmth = 0.30;

		if (!FMath::IsFinite(TreeSizeM) || TreeSizeM <= 0.0f
			|| !FMath::IsFinite(Slenderness)
			|| !FMath::IsFinite(PreferredTemperature) || !FMath::IsFinite(TemperatureTolerance)
			|| !FMath::IsFinite(PreferredHumidity)
			|| !FMath::IsFinite(MaxSlopeDeg)
			|| !FMath::IsFinite(RecruitsPerMatureStemPerYear) || RecruitsPerMatureStemPerYear < 0.0f
			|| !FMath::IsFinite(IndividualizationHeightCm)
			|| !FMath::IsFinite(GrowthSpeed) || !FMath::IsFinite(Toughness) || !FMath::IsFinite(ShadeTolerance))
		{
			OutError = FText::FromString(TEXT("Species tuning needs a finite, positive TreeSizeM, a finite IndividualizationHeightCm, a non-negative RecruitsPerMatureStemPerYear and finite knobs."));
			return false;
		}

		// Every knob is clamped here, not only in editor meta: ClampMin is a numeric-entry filter that text
		// import, ImportText and a direct caller all bypass.
		const double Target = FMath::Max(static_cast<double>(TargetStemsPerHectare), MinimumTargetStemsPerHectare);
		const double Speed = FMath::Clamp(static_cast<double>(GrowthSpeed), 0.0, 1.0);
		const double Tough = FMath::Clamp(static_cast<double>(Toughness), 0.0, 1.0);
		const double Shade = FMath::Clamp(static_cast<double>(ShadeTolerance), 0.0, 1.0);
		const double Size = FMath::Clamp(static_cast<double>(TreeSizeM), MinimumTreeSizeM, MaximumTreeSizeM);
		const double Slender = FMath::Clamp(static_cast<double>(Slenderness), 0.0, 1.0);
		const double Warmth = FMath::Clamp(static_cast<double>(PreferredTemperature), 0.0, 1.0);
		const double Tolerance = FMath::Clamp(static_cast<double>(TemperatureTolerance), 0.05, 1.0);
		const double Moisture = FMath::Clamp(static_cast<double>(PreferredHumidity), 0.0, 1.0);
		const double SlopeCeilingDeg = FMath::Clamp(static_cast<double>(MaxSlopeDeg), 0.0, 90.0);
		const double Recruits = FMath::Max(0.0, static_cast<double>(RecruitsPerMatureStemPerYear));

		// One-sided ramps out of the neutral 0.5, so a species that declares no preference derives the
		// permissive default of every bound and the two ground covers need no re-authoring at all.
		const double WarmSide = FMath::Max(0.0, Warmth * 2.0 - 1.0);
		const double ColdSide = FMath::Max(0.0, 1.0 - Warmth * 2.0);
		const double MoistSide = FMath::Max(0.0, Moisture * 2.0 - 1.0);
		const double ShadeSide = FMath::Max(0.0, Shade * 2.0 - 1.0);
		const double PioneerSide = FMath::Max(0.0, 1.0 - Shade * 2.0);

		const double MaxHeightCm = Size * 100.0;
		const double AllometryCoefficient = FMath::Lerp(StoutCoefficient, SlenderCoefficient, Slender);
		const double MaxDbhCm = FMath::Pow(MaxHeightCm / AllometryCoefficient, 1.0 / AllometryExponent);
		const double LifespanFactor = FMath::Lerp(ShortLifeFactor, LongLifeFactor, Tough);
		const double AgeYears = bIndividualizes
			? ReferenceAgeYears * FMath::Pow(Size / ReferenceSizeM, SizeAgeExponent) * LifespanFactor
			: CoverReferenceAgeYears * FMath::Pow(Size / CoverReferenceSizeM, CoverSizeAgeExponent) * LifespanFactor;
		const int32 DerivedMaxAgeYears = FMath::Max(1, FMath::RoundToInt32(AgeYears));
		const float DerivedIndividualizationHeightCm = bIndividualizes
			? FMath::Min(IndividualizationHeightCm, static_cast<float>(IndividualizationCeilingFraction * MaxHeightCm))
			: IndividualizationHeightCm;

		if (bIndividualizes && static_cast<double>(DerivedIndividualizationHeightCm) <= UE::ArcForest::RecruitHeightCm)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("IndividualizationHeightCm resolves to %.1f cm, from an authored %.1f cm against the %.0f%% ceiling of the derived MaxHeightCm (%.1f cm), and does not clear the %.0f cm recruitment boundary."),
				DerivedIndividualizationHeightCm,
				IndividualizationHeightCm,
				IndividualizationCeilingFraction * 100.0,
				MaxHeightCm,
				UE::ArcForest::RecruitHeightCm));
			return false;
		}

		const double GrowthSpanCm = UE::ArcForest::CalculateGrowthSpanCm(
			MaxHeightCm,
			bIndividualizes
				? static_cast<double>(DerivedIndividualizationHeightCm)
				: UE::ArcForest::RecruitHeightCm);
		if (GrowthSpanCm <= 0.0)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("TreeSizeM (%.2f m) must stay above %.3f m on a form that never individualizes: the growth derivation covers the span from the %.0f cm recruit height to 80%% of the derived MaxHeightCm and has none here."),
				Size,
				UE::ArcForest::RecruitHeightCm / (UE::ArcForest::GrowthSpanTopFraction * 100.0),
				UE::ArcForest::RecruitHeightCm));
			return false;
		}

		OutDerived.MaxHeightCm = static_cast<float>(MaxHeightCm);
		OutDerived.MaxDbhCm = static_cast<float>(MaxDbhCm);
		OutDerived.MaxAgeYears = DerivedMaxAgeYears;
		OutDerived.MaturityAgeYears = FMath::Clamp(
			FMath::RoundToInt32(static_cast<double>(DerivedMaxAgeYears) * MaturityFraction), 1, DerivedMaxAgeYears);
		OutDerived.HeightDbhAllometryCoefficient = static_cast<float>(AllometryCoefficient);
		OutDerived.HeightDbhAllometryExponent = static_cast<float>(AllometryExponent);
		OutDerived.IndividualizationHeightCm = DerivedIndividualizationHeightCm;
		OutDerived.BiomassAllometryCoefficient = static_cast<float>(BiomassCoefficient);
		OutDerived.BiomassAllometryExponent = static_cast<float>(BiomassExponent);
		OutDerived.CrownOccupancyWeight = static_cast<float>(CrownOccupancy);

		const double CrownRadiusM = FMath::Sqrt(ReferenceCanopyPacking * 10000.0 / (UE_DOUBLE_PI * Target));
		OutDerived.CrownRadiusHeightRatio = static_cast<float>(CrownRadiusM / Size);

		const double YearsToEightyPercent = FMath::Lerp(0.50, 0.20, Speed) * static_cast<double>(DerivedMaxAgeYears);
		const double HeightGrowthCm = GrowthSpanCm / (YearsToEightyPercent * ReferenceResource * MeanSizeLimitation);
		OutDerived.MaxAnnualHeightGrowthCm = static_cast<float>(HeightGrowthCm);
		OutDerived.MaxAnnualDbhGrowthCm = static_cast<float>(HeightGrowthCm / AllometryCoefficient);

		// A starting calibration against the stand's measured vigor, not a derivation from anything.
		OutDerived.BaseAnnualMortalityProbability = static_cast<float>(FMath::Lerp(0.030, 0.002, Tough));
		OutDerived.StressMortalityMaximum = FMath::Lerp(0.80, 0.30, Tough);
		OutDerived.StressMortalityVigorThreshold = FMath::Lerp(0.45, 0.12, Tough);
		OutDerived.OldAgeMortalityProbabilityAtMaxAge = FMath::Lerp(0.40, 0.12, Tough);
		OutDerived.StressMortalitySteepness = StressSteepness;
		OutDerived.StressMortalityCanopyFloor = StressCanopyFloor;
		OutDerived.VigorSmoothingRetention = VigorRetention;

		OutDerived.AnnualPropaguleProduction = static_cast<float>(
			Recruits * FMath::Lerp(1.0, PioneerSeedGain, PioneerSide) * FMath::Lerp(1.0, ClimaxSeedCut, ShadeSide));

		OutDerived.LightResponse.Minimum = static_cast<float>(FMath::Lerp(0.35, 0.00, Shade));
		if (bIndividualizes)
		{
			// Bare ground is light exactly 1, so a tree peaks there and succession has an opening move.
			OutDerived.LightResponse.Optimum = 1.0f;
			OutDerived.LightResponse.Maximum = 1.0f;
		}
		else
		{
			// Optimum falls as 1 - OptimumDrop*Shade and the full-sun score as 1 - FullSunDrop*Shade;
			// solving (Maximum - 1)/(Maximum - Optimum) for that score gives the off-scale anchor below.
			constexpr double CoverOptimumDrop = 0.75;
			constexpr double CoverFullSunDrop = 0.95;
			OutDerived.LightResponse.Optimum = static_cast<float>(1.0 - CoverOptimumDrop * Shade);
			OutDerived.LightResponse.Maximum = static_cast<float>(
				(CoverOptimumDrop + CoverFullSunDrop) / CoverFullSunDrop - CoverOptimumDrop * Shade);
		}

		// A shoulder, not a band: moisture reads exactly 1 on ordinary and on wet ground, and
		// EvaluateResponse short-circuits to 1 at the optimum while reading zero at any Maximum below it.
		OutDerived.MoistureResponse.Minimum = static_cast<float>(MoistureFloorAtFullHumidity * MoistSide);
		OutDerived.MoistureResponse.Optimum = 1.0f;
		OutDerived.MoistureResponse.Maximum = 1.0f;

		OutDerived.TemperatureResponse.Minimum = static_cast<float>(Warmth - Tolerance);
		OutDerived.TemperatureResponse.Optimum = static_cast<float>(Warmth);
		OutDerived.TemperatureResponse.Maximum = static_cast<float>(Warmth + Tolerance);

		OutDerived.MaxSlopeDeg = bIndividualizes ? static_cast<float>(SlopeCeilingDeg) : 90.0f;
		OutDerived.SunLoadMin = static_cast<float>(SunLoadFloorAtFullWarmth * WarmSide);
		OutDerived.WindExposureMax = static_cast<float>(
			FMath::Max(MinimumWindCeiling, 1.0 - WindCeilingDropAtFullHumidity * MoistSide));
		OutDerived.FertilityMin = static_cast<float>(FMath::Min(
			MaximumFertilityFloor,
			FertilityFloorAtFullCold * ColdSide + FertilityFloorAtFullHumidity * MoistSide));

		OutError = FText::GetEmpty();
		return true;
	}

	bool DeriveEcology(
		const UArcEcotopeSpeciesModel& Species,
		FArcForestDerivedEcology& OutDerived,
		FText& OutError)
	{
		return DeriveEcology(
			Species.TargetStemsPerHectare,
			Species.TreeSizeM,
			Species.Slenderness,
			Species.GrowthSpeed,
			Species.Toughness,
			Species.ShadeTolerance,
			Species.PreferredTemperature,
			Species.TemperatureTolerance,
			Species.PreferredHumidity,
			Species.MaxSlopeDeg,
			Species.GetRecruitsPerMatureStemPerYear(),
			Species.Individualizes(),
			Species.GetIndividualizationHeightCm(),
			OutDerived,
			OutError);
	}

#if WITH_EDITOR
	void AddError(FDataValidationContext& Context, EDataValidationResult& Result, const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	}

	bool IsValidGridSize(const EArcForestGridSize GridSize)
	{
		return StaticEnum<EArcForestGridSize>()->IsValidEnumValue(static_cast<int64>(GridSize));
	}

	bool IsFiniteInRange(const float Value, const float Minimum, const float Maximum)
	{
		return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
	}

	void ValidateRotation(
		const TCHAR* Prefix,
		const FArcForestInstanceRotation& Rotation,
		FDataValidationContext& Context,
		EDataValidationResult& Result)
	{
		if (!IsFiniteInRange(Rotation.MaxYawDeg, 0.0f, 180.0f))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s.Rotation.MaxYawDeg must be finite and inside [0, 180]."), Prefix));
		}
		if (!IsFiniteInRange(Rotation.MaxTiltDeg, 0.0f, 90.0f))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s.Rotation.MaxTiltDeg must be finite and inside [0, 90]."), Prefix));
		}
		if (!IsFiniteInRange(Rotation.MaxAlignDeg, 0.0f, 90.0f))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s.Rotation.MaxAlignDeg must be finite and inside [0, 90]."), Prefix));
		}
	}

	void ValidateScale(
		const TCHAR* Prefix,
		const FArcForestInstanceScale& Scale,
		FDataValidationContext& Context,
		EDataValidationResult& Result)
	{
		if (!FMath::IsFinite(Scale.MinScale) || Scale.MinScale <= 0.0f
			|| !FMath::IsFinite(Scale.MaxScale) || Scale.MaxScale <= 0.0f
			|| Scale.MinScale > Scale.MaxScale)
		{
			AddError(Context, Result, FString::Printf(TEXT("%s.Scale bounds must be finite, greater than zero, and ordered MinScale <= MaxScale."), Prefix));
		}
		if (!FMath::IsFinite(Scale.RandomJitter) || Scale.RandomJitter < 0.0f)
		{
			AddError(Context, Result, FString::Printf(TEXT("%s.Scale.RandomJitter must be finite and non-negative."), Prefix));
		}
	}
#endif
}

UArcEcotopeSpeciesModel::UArcEcotopeSpeciesModel()
{
	StaticMeshDescriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshDescriptor.bCanEverAffectNavigation = false;
	StaticMeshDescriptor.ComponentClass = UInstancedStaticMeshComponent::StaticClass();
	StaticMeshDescriptor.Mobility = EComponentMobility::Static;
	SkinnedMeshDescriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkinnedMeshDescriptor.bCanEverAffectNavigation = false;
}

void UArcEcotopeSpeciesModel::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) && !SpeciesId.IsValid())
	{
		SpeciesId = FGuid::NewGuid();
	}
}

uint64 UArcEcotopeSpeciesModel::ComputeEcologyHash() const
{
	using namespace ArcForestSpeciesModel;

	FArcForestDerivedEcology Derived;
	FText DerivationError;
	if (!DeriveEcology(*this, Derived, DerivationError))
	{
		return 0;
	}

	FCanonicalHash64 Hash;
	Hash.AddGuid(SpeciesId);
	Hash.AddUInt8(Individualizes() ? 1 : 0);
	// Field-for-field twin of ComputeFrozenSpeciesEcologyHash; only a runtime cross-check keeps the two in step.
	Hash.AddInt32(TargetStemsPerHectare);
	Hash.AddFloat(TreeSizeM);
	Hash.AddFloat(Slenderness);
	Hash.AddFloat(GrowthSpeed);
	Hash.AddFloat(Toughness);
	Hash.AddFloat(ShadeTolerance);
	Hash.AddFloat(PreferredTemperature);
	Hash.AddFloat(TemperatureTolerance);
	Hash.AddFloat(PreferredHumidity);
	Hash.AddFloat(GetMeanDispersalDistanceCm());
	Hash.AddFloat(ExclusionSensitivity);
	Hash.AddFloat(GetRecruitsPerMatureStemPerYear());
	Hash.AddFloat(MobilityMortalitySensitivity);
	Hash.AddFloat(GetLateralSpreadRatePerYear());
	Hash.AddFloat(Derived.IndividualizationHeightCm);
	Hash.AddFloat(Derived.MaxHeightCm);
	Hash.AddFloat(Derived.MaxDbhCm);
	Hash.AddInt32(Derived.MaxAgeYears);
	Hash.AddInt32(Derived.MaturityAgeYears);
	Hash.AddFloat(Derived.MaxAnnualHeightGrowthCm);
	Hash.AddFloat(Derived.MaxAnnualDbhGrowthCm);
	Hash.AddFloat(Derived.HeightDbhAllometryCoefficient);
	Hash.AddFloat(Derived.HeightDbhAllometryExponent);
	Hash.AddFloat(Derived.CrownRadiusHeightRatio);
	Hash.AddFloat(Derived.BiomassAllometryCoefficient);
	Hash.AddFloat(Derived.BiomassAllometryExponent);
	Hash.AddFloat(Derived.CrownOccupancyWeight);
	Hash.AddFloat(Derived.BaseAnnualMortalityProbability);
	Hash.AddDouble(Derived.OldAgeMortalityProbabilityAtMaxAge);
	Hash.AddDouble(Derived.StressMortalityMaximum);
	Hash.AddDouble(Derived.StressMortalityVigorThreshold);
	Hash.AddDouble(Derived.StressMortalitySteepness);
	Hash.AddDouble(Derived.StressMortalityCanopyFloor);
	Hash.AddDouble(Derived.VigorSmoothingRetention);
	Hash.AddFloat(Derived.AnnualPropaguleProduction);
	AddResponse(Hash, Derived.LightResponse);
	AddResponse(Hash, Derived.MoistureResponse);
	AddResponse(Hash, Derived.TemperatureResponse);
	Hash.AddFloat(Derived.MaxSlopeDeg);
	Hash.AddFloat(Derived.SunLoadMin);
	Hash.AddFloat(Derived.WindExposureMax);
	Hash.AddFloat(Derived.FertilityMin);
	return Hash.Finalize();
}

uint64 UArcEcotopeSpeciesModel::ComputeVisualHash() const
{
	using namespace ArcForestSpeciesModel;

	FCanonicalHash64 Hash;
	Hash.AddGuid(SpeciesId);
	// Layer order is the layer axis of the placement keys, so it is hashed as authored.
	const TConstArrayView<FArcForestCoverClusterLayer> AuthoredClusterLayers = GetCoverClusterLayers();
	Hash.AddInt32(AuthoredClusterLayers.Num());
	for (const FArcForestCoverClusterLayer& Layer : AuthoredClusterLayers)
	{
		Hash.AddFloat(Layer.Weight);
		Hash.AddFloat(Layer.ClusterRadiusCm);
		Hash.AddInt32(Layer.InstancesPerCluster);
		Hash.AddFloat(Layer.SiteBiasStrength);
	}
	// A descriptor no band reads must not move the hash: it would re-bake every instance of a species
	// over a field nothing consumes.
	Hash.AddUInt32(HasBandVisual<FArcForestBandVisual_StaticMesh>(Bands) ? StaticMeshDescriptor.ComputeHash() : 0u);
	Hash.AddUInt32(HasBandVisual<FArcForestBandVisual_SkinnedMesh>(Bands) ? SkinnedMeshDescriptor.ComputeHash() : 0u);
	Hash.AddUInt32(static_cast<uint32>(GridSize));
	Hash.AddString(RuntimeGridName.ToString());
	Hash.AddInt32(Bands.Num());
	for (const FArcForestStageBand& Band : Bands)
	{
		Hash.AddUInt64(Band.ComputeHash());
	}
	Hash.AddString(TerrainCoverChannel.ToString());
	return Hash.Finalize();
}

#if WITH_EDITOR
void UArcEcotopeSpeciesModel::PostDuplicate(const bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE && !HasAnyFlags(RF_ClassDefaultObject))
	{
		SpeciesId = FGuid::NewGuid();
	}
}

EDataValidationResult UArcEcotopeSpeciesModel::IsDataValid(FDataValidationContext& Context) const
{
	using namespace ArcForestSpeciesModel;

	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!SpeciesId.IsValid())
	{
		AddError(Context, Result, TEXT("SpeciesId must be a valid GUID."));
	}

	if (TargetStemsPerHectare <= 0)
	{
		AddError(Context, Result, TEXT("TargetStemsPerHectare must be greater than zero."));
	}

	const TPair<const TCHAR*, float> UnitIntervalKnobs[] =
	{
		{TEXT("Slenderness"), Slenderness},
		{TEXT("GrowthSpeed"), GrowthSpeed},
		{TEXT("Toughness"), Toughness},
		{TEXT("ShadeTolerance"), ShadeTolerance},
		{TEXT("PreferredTemperature"), PreferredTemperature},
		{TEXT("PreferredHumidity"), PreferredHumidity},
		{TEXT("ExclusionSensitivity"), ExclusionSensitivity},
		{TEXT("MobilityMortalitySensitivity"), MobilityMortalitySensitivity},
		{TEXT("LateralSpreadRatePerYear"), GetLateralSpreadRatePerYear()},
	};
	for (const TPair<const TCHAR*, float>& Knob : UnitIntervalKnobs)
	{
		if (!IsFiniteInRange(Knob.Value, 0.0f, 1.0f))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s must be finite and inside [0, 1]."), Knob.Key));
		}
	}

	if (!IsFiniteInRange(TreeSizeM, 0.3f, 120.0f))
	{
		AddError(Context, Result, TEXT("TreeSizeM must be finite and inside [0.3, 120] m."));
	}

	if (!IsFiniteInRange(TemperatureTolerance, 0.05f, 1.0f))
	{
		AddError(Context, Result, TEXT("TemperatureTolerance must be finite and inside [0.05, 1]."));
	}

	if (!IsFiniteInRange(MaxSlopeDeg, 0.0f, 90.0f))
	{
		AddError(Context, Result, TEXT("MaxSlopeDeg must be finite and inside [0, 90] degrees."));
	}

	if (!FMath::IsFinite(GetMeanDispersalDistanceCm()) || GetMeanDispersalDistanceCm() < 0.0f)
	{
		AddError(Context, Result, TEXT("MeanDispersalDistanceCm must be finite and non-negative."));
	}

	if (!FMath::IsFinite(GetRecruitsPerMatureStemPerYear()) || GetRecruitsPerMatureStemPerYear() < 0.0f)
	{
		AddError(Context, Result, TEXT("RecruitsPerMatureStemPerYear must be finite and non-negative."));
	}

	FArcForestDerivedEcology Derived;
	FText DerivationError;
	if (!DeriveEcology(*this, Derived, DerivationError))
	{
		AddError(Context, Result, DerivationError.ToString());
	}

	if (Bands.IsEmpty())
	{
		AddError(Context, Result, TEXT("Bands must carry at least one height band."));
	}
	if (!IsValidGridSize(GridSize))
	{
		AddError(Context, Result, TEXT("GridSize must be one of the authored cell sizes."));
	}
	// The FastGeo rules only bind a species that some band actually draws through a static mesh.
	if (HasBandVisual<FArcForestBandVisual_StaticMesh>(Bands))
	{
		if (StaticMeshDescriptor.ComponentClass == nullptr
			|| StaticMeshDescriptor.ComponentClass->IsChildOf(UHierarchicalInstancedStaticMeshComponent::StaticClass()))
		{
			AddError(Context, Result, TEXT("StaticMeshDescriptor.ComponentClass must be a plain UInstancedStaticMeshComponent: the cook-time FastGeo conversion rejects a non-Nanite HISM with more than one LOD."));
		}
		if (StaticMeshDescriptor.Mobility != EComponentMobility::Static)
		{
			AddError(Context, Result, TEXT("StaticMeshDescriptor.Mobility must be Static for the cook-time FastGeo conversion."));
		}
	}
	for (int32 BandIndex = 0; BandIndex < Bands.Num(); ++BandIndex)
	{
		const FArcForestStageBand& Band = Bands[BandIndex];
		const FString Prefix = FString::Printf(TEXT("Bands[%d]"), BandIndex);
		const bool bBandOrdered = FMath::IsFinite(Band.MinHeightCm) && Band.MinHeightCm >= 0.0f
			&& FMath::IsFinite(Band.MaxHeightCm) && Band.MaxHeightCm >= 0.0f
			&& Band.MinHeightCm <= Band.MaxHeightCm;
		if (!bBandOrdered)
		{
			AddError(Context, Result, Prefix + TEXT(" height band must be finite, non-negative and ordered MinHeightCm <= MaxHeightCm."));
		}
		if (bBandOrdered)
		{
			for (int32 EarlierIndex = 0; EarlierIndex < BandIndex; ++EarlierIndex)
			{
				const FArcForestStageBand& Earlier = Bands[EarlierIndex];
				if (Earlier.Kind == Band.Kind
					&& Earlier.MinHeightCm <= Earlier.MaxHeightCm
					&& Band.MinHeightCm <= Earlier.MaxHeightCm
					&& Earlier.MinHeightCm <= Band.MaxHeightCm)
				{
					AddError(
						Context,
						Result,
						FString::Printf(
							TEXT("%s overlaps Bands[%d]: bands of the same Kind must not share a height range."),
							*Prefix,
							EarlierIndex));
				}
			}
		}

		ValidateRotation(*Prefix, Band.Rotation, Context, Result);
		ValidateScale(*Prefix, Band.Scale, Context, Result);
		if (!FMath::IsFinite(Band.OffsetAlongUpCm))
		{
			AddError(Context, Result, Prefix + TEXT(".OffsetAlongUpCm must be finite."));
		}

		const FArcForestBandVisual* Visual = Band.Visual.GetPtr();
		if (Visual == nullptr)
		{
			AddError(Context, Result, Prefix + TEXT(" must select a concrete visual type."));
			continue;
		}
		FText VisualError;
		if (!Visual->IsValid(VisualError))
		{
			AddError(Context, Result, Prefix + TEXT(".Visual ") + VisualError.ToString());
		}
	}

	return Result;
}
#endif

#if WITH_EDITOR
EDataValidationResult UArcEcotopeTreeSpeciesModel::IsDataValid(FDataValidationContext& Context) const
{
	using namespace ArcForestSpeciesModel;

	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!FMath::IsFinite(IndividualizationHeightCm) || IndividualizationHeightCm <= 30.0f)
	{
		AddError(Context, Result, TEXT("IndividualizationHeightCm must be finite and strictly greater than 30 cm."));
	}

	return Result;
}
#endif
