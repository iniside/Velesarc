// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSamplerVolume.h"

#include "Sampler/ArcForestSamplerInput.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Species/ArcForestStageBand.h"

#if WITH_EDITOR
namespace ArcEcotopeSamplerVolumePrivate
{
	const UArcEcotopeSpeciesModel* FindSpeciesModel(
		const TArray<FArcEcotopeSpeciesSetup>& Setups,
		const FGuid& SpeciesId)
	{
		for (const FArcEcotopeSpeciesSetup& Setup : Setups)
		{
			if (Setup.Species != nullptr && Setup.Species->SpeciesId == SpeciesId)
			{
				return Setup.Species;
			}
		}
		return nullptr;
	}

	FString DescribeSpecies(const UArcEcotopeSpeciesModel* Model, const FGuid& SpeciesId)
	{
		return Model != nullptr ? Model->GetPathName() : SpeciesId.ToString();
	}
}

bool AArcEcotopeSamplerVolume::FreezeSamplerInput(
	FArcForestSamplerInput& OutInput,
	FText& OutError) const
{
	using namespace ArcEcotopeSamplerVolumePrivate;

	if (!IsInGameThread())
	{
		OutError = FText::FromString(TEXT("ArcForest sampler inputs must be frozen on the game thread."));
		return false;
	}
	if (!VolumeId.IsValid())
	{
		OutError = FText::FromString(TEXT("ArcEcotope volume has no valid VolumeId."));
		return false;
	}
	const UArcEcotopeModel& EcotopeModel = GetEcotopeOrDefault();
	if (!FMath::IsFinite(EcotopeModel.MaximumCrownOccupancy) || EcotopeModel.MaximumCrownOccupancy <= 0.0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest sampler requires a finite, strictly positive Ecotope.MaximumCrownOccupancy."));
		return false;
	}
	if (!FMath::IsFinite(EcotopeModel.UnderstoryShade) || EcotopeModel.UnderstoryShade < 0.05 || EcotopeModel.UnderstoryShade > 4.0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest sampler requires a finite Ecotope.UnderstoryShade in [0.05, 4.0]."));
		return false;
	}

	FArcForestSamplerInput Candidate;
	Candidate.VolumeId = VolumeId;
	Candidate.Seed = Seed;
	Candidate.ChunkBaseCellSpan = ChunkBaseCellSpan;
	Candidate.SettingsHash = ComputeSettingsHash();
	Candidate.MaximumCrownOccupancy = EcotopeModel.MaximumCrownOccupancy;
	Candidate.UnderstoryShade = EcotopeModel.UnderstoryShade;
	Candidate.Settings = SamplerSettings;

	if (!FreezeSiteChunkSource(Candidate.SiteSource, OutError))
	{
		return false;
	}
	if (!Candidate.SiteSource.IsValid())
	{
		OutError = FText::FromString(TEXT("ArcForest sampler could not select immutable site storage."));
		return false;
	}
	const FArcEcotopeSiteMetadata& FrozenMetadata = Candidate.SiteSource->Metadata;
	if (FrozenMetadata.RecordGridLevel != RecordGridLevel)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("The baked site records at grid level %d while the volume authors level %d. Re-apply the volume's RecordGridLevel to fix the disagreement."),
			static_cast<int32>(FrozenMetadata.RecordGridLevel),
			static_cast<int32>(RecordGridLevel)));
		return false;
	}

	FArcForestBootstrapInput BootstrapInput;
	if (!FreezeBootstrapInput(EArcForestStartMode::BareGround, BootstrapInput, OutError))
	{
		return false;
	}
	Candidate.SpeciesSetups = MoveTemp(BootstrapInput.SpeciesSetups);

	for (int32 SetupIndex = 0; SetupIndex < Candidate.SpeciesSetups.Num(); ++SetupIndex)
	{
		const FArcForestFrozenSpeciesSetup& Frozen = Candidate.SpeciesSetups[SetupIndex];
		if (!Frozen.bIndividualizes)
		{
			continue;
		}

		const UArcEcotopeSpeciesModel* Model = FindSpeciesModel(EcotopeModel.SpeciesSetups, Frozen.SpeciesId);
		if (Model == nullptr)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Tree species %s no longer resolves against Ecotope.SpeciesSetups."),
				*DescribeSpecies(Model, Frozen.SpeciesId)));
			return false;
		}

		FArcForestSamplerFrozenBands& SpeciesBands = Candidate.SpeciesBands.AddDefaulted_GetRef();
		SpeciesBands.SpeciesId = Frozen.SpeciesId;
		for (int32 BandIndex = 0; BandIndex < Model->Bands.Num(); ++BandIndex)
		{
			const FArcForestStageBand& Band = Model->Bands[BandIndex];
			if (Band.Kind != EArcForestVisualKind::Alive)
			{
				continue;
			}
			// Snapshot::Validate rejects any individual below 30 cm, so a band reaching lower cannot be sampled from.
			const float MinHeightCm = FMath::Max(30.0f, Band.MinHeightCm);
			if (MinHeightCm >= Band.MaxHeightCm)
			{
				continue;
			}
			FArcForestSamplerFrozenBand& FrozenBand = SpeciesBands.Bands.AddDefaulted_GetRef();
			FrozenBand.MinHeightCm = MinHeightCm;
			FrozenBand.MaxHeightCm = Band.MaxHeightCm;
			FrozenBand.BandIndex = BandIndex;
		}

		if (SpeciesBands.Bands.IsEmpty())
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Tree species %s has no live stage band above 30 cm, so the sampler cannot place it."),
				*DescribeSpecies(Model, Frozen.SpeciesId)));
			return false;
		}

		SpeciesBands.Bands.Sort(
			[](const FArcForestSamplerFrozenBand& Left, const FArcForestSamplerFrozenBand& Right)
			{
				return Left.MinHeightCm < Right.MinHeightCm;
			});
	}

	OutInput = MoveTemp(Candidate);
	OutError = FText::GetEmpty();
	return true;
}
#endif
