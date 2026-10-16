// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestPatchField.h"

#include "Forest/ArcForestField.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSpeciesDefinition.h"

namespace ArcForestPatchFieldLocal
{
	/** Query/truncation radius = this many σ; exp(−4.5) ≈ 0.011 is a negligible tail. */
	static constexpr float TruncationSigmas = 3.0f;

	/** Bounds are world units (cm); PatchDensity is parents per m². 1 m = 100 cm. */
	static constexpr float CmPerMeter = 100.0f;

	/** Below this σ (cm) a species has no meaningful patch field — skip it. */
	static constexpr float MinSigma = KINDA_SMALL_NUMBER;

	// Suitability sampling lives in ArcForestField::SampleFieldNearest — the single authority
	// shared with the solver (an empty grid means "no habitat mask supplied" -> uniform 1.0, so
	// every candidate is accepted).
}

void FArcForestPatchField::Build(
	const UArcForestSpeciesDefinition& Species,
	const FArcForestSimParams& Params,
	TArrayView<const float> SuitabilityGrid,
	FIntPoint GridDims,
	const FArcForestHabitatInputs& Habitat)
{
	const TArray<FArcForestSpeciesEntry>& Table = Species.Species;
	const int32 NumSpecies = Table.Num();

	// Size the per-species store to its final length up front, then never resize it — the
	// spatial hashes built below hold non-owning views into each ParentPositions array, so a
	// reallocation after Build would dangle them (class LIFETIME note).
	PerSpecies.Reset();
	PerSpecies.SetNum(NumSpecies);

	const FBox2f& Bounds = Params.Bounds2D;
	const FVector2f BoundsSize = Bounds.GetSize();

	// Expected parents = PatchDensity(parents/m²) · Area(m²). Bounds are cm → convert.
	const float AreaMeters2 =
		(BoundsSize.X / ArcForestPatchFieldLocal::CmPerMeter) *
		(BoundsSize.Y / ArcForestPatchFieldLocal::CmPerMeter);

	// Globally-unique, stable, Build-order parent ids across all species.
	int32 NextPatchId = 0;

	for (int32 SpeciesIndex = 0; SpeciesIndex < NumSpecies; ++SpeciesIndex)
	{
		const FArcForestSpeciesEntry& Entry = Table[SpeciesIndex];
		FSpeciesPatches& Patches = PerSpecies[SpeciesIndex];

		Patches.Sigma = Entry.PatchRadiusSigma;
		Patches.QueryRadius = ArcForestPatchFieldLocal::TruncationSigmas * Entry.PatchRadiusSigma;

		// A species with no meaningful σ or no density has an empty parent set — its affinity
		// is 0 everywhere, so seeding falls back to pure suitability (the "uniform mix" end of
		// the w_patch dial). Still occupies its SpeciesIndex slot.
		if (Entry.PatchRadiusSigma <= ArcForestPatchFieldLocal::MinSigma ||
			Entry.PatchDensity <= 0.0f ||
			AreaMeters2 <= 0.0f)
		{
			continue;
		}

		// Decorrelate parent RNG per species (see class header). +1 keeps species 0 off the
		// raw seed value; HashCombine mixes seed and index into independent streams.
		const int32 SpeciesSeed =
			static_cast<int32>(HashCombine(
				static_cast<uint32>(Params.Seed),
				static_cast<uint32>(SpeciesIndex + 1)));
		FRandomStream Rng(SpeciesSeed);

		// Deterministic candidate count = round(κ·A); each is thinned by suitability.
		const int32 CandidateCount =
			FMath::Max(0, FMath::RoundToInt(Entry.PatchDensity * AreaMeters2));

		Patches.ParentPositions.Reserve(CandidateCount);
		Patches.ParentIds.Reserve(CandidateCount);

		for (int32 Candidate = 0; Candidate < CandidateCount; ++Candidate)
		{
			const FVector2f Pos(
				static_cast<float>(Rng.FRandRange(Bounds.Min.X, Bounds.Max.X)),
				static_cast<float>(Rng.FRandRange(Bounds.Min.Y, Bounds.Max.Y)));

			// INHOMOGENEOUS thinning: keep the candidate with probability
			// suitability(Pos) * habitatResponse(species, Pos).
			//
			// THIS is the site that moves clumps to habitat. The dart-acceptance term in
			// SeedPopulation only thins WITHIN a clump once its centre is already placed; a species
			// whose parents are scattered uniformly stays uniformly scattered no matter how sharp its
			// curves are. Biasing the PARENT process is what makes ferns appear in the wet hollows
			// rather than everywhere at a lower density.
			//
			// RNG-neutral by construction: EvaluateHabitatResponse takes no stream and draws nothing,
			// so this still consumes exactly ONE FRand() per candidate, as it did before. An empty
			// response set returns exactly 1.0f, making the product an IEEE-754 identity and the
			// accepted parent set bit-identical to the pre-habitat build.
			const float Suitability = ArcForestField::SampleFieldNearest(
				Pos, Bounds, SuitabilityGrid, GridDims);
			const float HabitatResponse =
				ArcForestField::EvaluateHabitatResponse(Habitat, SpeciesIndex, Pos, Params.Seed);
			const float AcceptProb = FMath::Clamp(Suitability * HabitatResponse, 0.0f, 1.0f);

			if (Rng.FRand() < AcceptProb)
			{
				Patches.ParentPositions.Add(Pos);
				Patches.ParentIds.Add(NextPatchId++);
			}
		}
	}

	// Second pass — all parent arrays are now at their final size, so building the hashes
	// (which store views into them) can no longer be invalidated by a reallocation.
	for (int32 SpeciesIndex = 0; SpeciesIndex < NumSpecies; ++SpeciesIndex)
	{
		FSpeciesPatches& Patches = PerSpecies[SpeciesIndex];
		if (Patches.ParentPositions.Num() == 0)
		{
			continue;
		}

		// Cell ≈ σ keeps buckets small; the query then scans ceil(3σ/σ)=3 cells each way.
		Patches.Hash.Build(Patches.ParentPositions, Patches.Sigma);
	}
}

float FArcForestPatchField::SampleAffinity(int32 SpeciesIndex, FVector2f Pos) const
{
	int32 UnusedDominant = INDEX_NONE;
	return SampleAffinity(SpeciesIndex, Pos, UnusedDominant);
}

int32 FArcForestPatchField::SampleDominantPatchId(int32 SpeciesIndex, FVector2f Pos) const
{
	int32 Dominant = INDEX_NONE;
	SampleAffinity(SpeciesIndex, Pos, Dominant);
	return Dominant;
}

float FArcForestPatchField::SampleAffinity(int32 SpeciesIndex, FVector2f Pos, int32& OutDominantPatchId) const
{
	OutDominantPatchId = INDEX_NONE;

	if (!PerSpecies.IsValidIndex(SpeciesIndex))
	{
		return 0.0f;
	}

	const FSpeciesPatches& Patches = PerSpecies[SpeciesIndex];
	if (Patches.ParentPositions.Num() == 0 || Patches.Sigma <= ArcForestPatchFieldLocal::MinSigma)
	{
		return 0.0f;
	}

	TArray<int32> Nearby;
	Patches.Hash.QueryRadius(Pos, Patches.QueryRadius, Nearby);
	if (Nearby.Num() == 0)
	{
		return 0.0f;
	}

	const float TwoSigmaSq = 2.0f * Patches.Sigma * Patches.Sigma;

	float Affinity = 0.0f;
	float BestDistSq = TNumericLimits<float>::Max();

	for (int32 LocalIndex : Nearby)
	{
		const FVector2f Delta = Patches.ParentPositions[LocalIndex] - Pos;
		const float DistSq = Delta.SizeSquared();

		Affinity += FMath::Exp(-DistSq / TwoSigmaSq);

		// Dominant == largest bump == nearest parent (shared σ). Tie-break: smaller PatchId.
		const int32 CandidateId = Patches.ParentIds[LocalIndex];
		if (DistSq < BestDistSq ||
			(DistSq == BestDistSq && CandidateId < OutDominantPatchId))
		{
			BestDistSq = DistSq;
			OutDominantPatchId = CandidateId;
		}
	}

	return Affinity;
}

TArrayView<const FVector2f> FArcForestPatchField::GetParentPositions(int32 SpeciesIndex) const
{
	if (!PerSpecies.IsValidIndex(SpeciesIndex))
	{
		return TArrayView<const FVector2f>();
	}
	return PerSpecies[SpeciesIndex].ParentPositions;
}

TArrayView<const int32> FArcForestPatchField::GetParentIds(int32 SpeciesIndex) const
{
	if (!PerSpecies.IsValidIndex(SpeciesIndex))
	{
		return TArrayView<const int32>();
	}
	return PerSpecies[SpeciesIndex].ParentIds;
}

float FArcForestPatchField::GetPatchRadiusSigma(int32 SpeciesIndex) const
{
	if (!PerSpecies.IsValidIndex(SpeciesIndex))
	{
		return 0.0f;
	}
	return PerSpecies[SpeciesIndex].Sigma;
}
