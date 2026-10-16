// Copyright Lukasz Baran. All Rights Reserved.

#include "Sampler/ArcForestSamplerSizeClasses.h"

#include "Sampler/ArcForestSamplerInput.h"
#include "Simulation/ArcForestBootstrap.h"

namespace ArcForestSamplerSizeClassesPrivate
{
	constexpr double MinimumBandMass = 0.05;

	struct FEmittingSpecies
	{
		int32 SpeciesIndex = INDEX_NONE;
		const FArcForestSamplerFrozenBands* Bands = nullptr;
		double RawWeight = 0.0;
		double MaxHeightCm = 0.0;
		double CrownRadiusCm = 0.0;
		double MassSum = 0.0;
		TArray<double> BandMass;
		TArray<double> BandMidU;
	};

	const FArcForestSamplerFrozenBands* FindBands(
		const TArray<FArcForestSamplerFrozenBands>& SpeciesBands,
		const FGuid& SpeciesId)
	{
		for (const FArcForestSamplerFrozenBands& Candidate : SpeciesBands)
		{
			if (Candidate.SpeciesId == SpeciesId)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	double SanitizeWeight(double Weight)
	{
		if (!FMath::IsFinite(Weight))
		{
			return 0.0;
		}
		return FMath::Max(0.0, Weight);
	}

	double MakeShape(float UnevenAgedness)
	{
		return FMath::Lerp(3.6, 1.0, FMath::Clamp(static_cast<double>(UnevenAgedness), 0.0, 1.0));
	}

	double MakeScale(double Shape)
	{
		return 1.0 / FMath::Pow(-FMath::Loge(0.01), 1.0 / Shape);
	}

	double EvaluateCdf(double U, double Scale, double Shape)
	{
		if (U <= 0.0)
		{
			return 0.0;
		}
		return 1.0 - FMath::Exp(-FMath::Pow(U / Scale, Shape));
	}
}

void UE::ArcForest::Sampler::SolveBandTargets(
	const FArcForestSamplerInput& Input,
	TArrayView<const double> SpeciesWeights,
	double RecordCellAreaCm2,
	TArray<FArcForestSamplerBandTarget>& OutTargets,
	TArray<int32>& OutUnsolvedSpeciesIndices)
{
	using namespace ArcForestSamplerSizeClassesPrivate;

	OutTargets.Reset();
	OutUnsolvedSpeciesIndices.Reset();

	const TArray<FArcForestFrozenSpeciesSetup>& Setups = Input.SpeciesSetups;
	if (!ensureMsgf(SpeciesWeights.Num() == Setups.Num(), TEXT("SpeciesWeights must be parallel to Input.SpeciesSetups")))
	{
		return;
	}

	if (RecordCellAreaCm2 <= 0.0)
	{
		return;
	}

	const double Shape = MakeShape(Input.Settings.UnevenAgedness);
	const double Scale = MakeScale(Shape);

	// The weights arrive as per-species site suitability, not as a distribution; unnormalized they would make total crown occupancy scale with the number of species instead of honouring MaximumCrownOccupancy.
	double WeightSum = 0.0;
	TArray<FEmittingSpecies> Emitting;

	for (int32 SpeciesIndex = 0; SpeciesIndex < Setups.Num(); ++SpeciesIndex)
	{
		const double RawWeight = SanitizeWeight(SpeciesWeights[SpeciesIndex]);
		if (RawWeight <= 0.0)
		{
			continue;
		}

		const FArcForestFrozenSpeciesSetup& Setup = Setups[SpeciesIndex];
		const FArcForestSamplerFrozenBands* Bands = FindBands(Input.SpeciesBands, Setup.SpeciesId);
		if (Bands == nullptr || Bands->Bands.Num() == 0)
		{
			continue;
		}

		const double MaxHeightCm = static_cast<double>(Setup.MaxHeightCm);
		const double CrownRadiusCm = static_cast<double>(Setup.CrownRadiusHeightRatio) * MaxHeightCm;
		if (MaxHeightCm <= 0.0 || CrownRadiusCm <= 0.0)
		{
			OutUnsolvedSpeciesIndices.Add(SpeciesIndex);
			continue;
		}

		FEmittingSpecies Candidate;
		Candidate.SpeciesIndex = SpeciesIndex;
		Candidate.Bands = Bands;
		Candidate.RawWeight = RawWeight;
		Candidate.MaxHeightCm = MaxHeightCm;
		Candidate.CrownRadiusCm = CrownRadiusCm;

		for (const FArcForestSamplerFrozenBand& Band : Bands->Bands)
		{
			const double MinU = FMath::Clamp(static_cast<double>(Band.MinHeightCm) / MaxHeightCm, 0.0, 1.0);
			const double MaxU = FMath::Clamp(static_cast<double>(Band.MaxHeightCm) / MaxHeightCm, 0.0, 1.0);
			const double Mass = FMath::Max(0.0, EvaluateCdf(MaxU, Scale, Shape) - EvaluateCdf(MinU, Scale, Shape));

			Candidate.BandMass.Add(Mass);
			Candidate.BandMidU.Add(0.5 * (MinU + MaxU));
			Candidate.MassSum += Mass;
		}

		if (Candidate.MassSum < MinimumBandMass)
		{
			OutUnsolvedSpeciesIndices.Add(SpeciesIndex);
			continue;
		}

		WeightSum += RawWeight;
		Emitting.Add(MoveTemp(Candidate));
	}

	if (WeightSum <= 0.0)
	{
		return;
	}

	for (FEmittingSpecies& Species : Emitting)
	{
		const double Weight = Species.RawWeight / WeightSum;

		// E[u^2] sums the renormalized band masses because the placed distribution is the Weibull truncated to the authored bands; the Gamma moment describes the untruncated one.
		double ExpectedUSquared = 0.0;
		for (int32 MassIndex = 0; MassIndex < Species.BandMass.Num(); ++MassIndex)
		{
			Species.BandMass[MassIndex] /= Species.MassSum;
			ExpectedUSquared += Species.BandMass[MassIndex] * FMath::Square(Species.BandMidU[MassIndex]);
		}

		const double ExpectedCrownAreaCm2 = UE_DOUBLE_PI * FMath::Square(Species.CrownRadiusCm) * ExpectedUSquared;
		if (ExpectedCrownAreaCm2 <= 0.0)
		{
			OutUnsolvedSpeciesIndices.Add(Species.SpeciesIndex);
			continue;
		}

		// Band midpoints drop the within-band variance w^2/12, so the expected crown area is low and the stem count a few percent high; the placer's height jitter spreads stems across the band, so this is a known bias rather than an error.
		const double SpeciesStemsPerCell = Input.MaximumCrownOccupancy * RecordCellAreaCm2 * Weight / ExpectedCrownAreaCm2;

		for (int32 BandIndex = 0; BandIndex < Species.BandMass.Num(); ++BandIndex)
		{
			if (Species.BandMass[BandIndex] <= 0.0)
			{
				continue;
			}

			FArcForestSamplerBandTarget& Target = OutTargets.AddDefaulted_GetRef();
			Target.SpeciesIndex = Species.SpeciesIndex;
			Target.BandIndex = Species.Bands->Bands[BandIndex].BandIndex;
			Target.MeanHeightCm = Species.BandMidU[BandIndex] * Species.MaxHeightCm;
			Target.StemsPerCell = SpeciesStemsPerCell * Species.BandMass[BandIndex];
		}
	}
}
