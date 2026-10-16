// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestBootstrap.h"

#include "ArcEcotopeModule.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/ArcEcotopeBudgetSettings.h"
#include "Simulation/ArcForestGrowthEngine.h"
#include "Simulation/ArcForestHabitatResponse.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Species/ArcEcotopeSpeciesModel.h"

namespace ArcForestBootstrapPrivate
{
	using UE::ArcForest::Habitat::AggregateHabitat;
	using UE::ArcForest::Habitat::EvaluateHabitatResponseTerms;

	constexpr int32 HabitatResponseCount = UE::ArcForest::Habitat::HabitatResponseCount;
	constexpr double SquareCentimetersPerHectare = 100000000.0;
	constexpr float InitialHeightCm = 30.0f;
	constexpr int32 InitialHeightBin = 0;

	TAutoConsoleVariable<int32> CVarSiteValidationBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.ValidationBudgetMiB"),
		64,
		TEXT("Memory budget in MiB for mandatory frozen bulk site validation before editor simulation."),
		ECVF_Default);

	struct FCellWeight
	{
		int32 CellIndex = INDEX_NONE;
		double Weight = 0.0;
	};

	struct FCellAllocation
	{
		int32 CellIndex = INDEX_NONE;
		int64 StemCount = 0;
		double Fraction = 0.0;
		double SelectionScore = 0.0;
	};

	struct FCohortCandidate
	{
		FGuid SpeciesId;
		bool bIndividualizes = false;
		int32 CellIndex = INDEX_NONE;
		int64 StemCount = 0;
		float BiomassPerStemKg = 0.0f;
		uint64 AdmissionPriority = 0;
	};

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		UE_LOGF(LogArcEcotope, Error,"ArcForest bootstrap: %ls", *Message);
		return false;
	}

	bool CheckCancellation(
		const FArcForestCancellationToken* Cancellation,
		FText& OutError)
	{
		if (Cancellation != nullptr && Cancellation->IsCancellationRequested())
		{
			return SetError(OutError, TEXT("ArcForest bootstrap cancelled."));
		}
		return true;
	}

	bool CalculateValidAreaSquareCentimeters(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeSiteChunkReader* SiteReader,
		const FArcEcotopeChunkLayout* SiteLayout,
		double& OutArea,
		FText& OutError,
		const FArcForestCancellationToken* Cancellation)
	{
		OutArea = 0.0;
		if (SiteReader != nullptr && SiteReader->IsOpen() && SiteLayout != nullptr)
		{
			const FIntPoint ChunkGrid = SiteLayout->GetChunkGridDimensions();
			const int32 Width = Site.GridDimensions.X;
			constexpr uint32 ValidSurfaceMask =
				1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface);
			for (int32 ChunkY = 0; ChunkY < ChunkGrid.Y; ++ChunkY)
			{
				FArcEcotopeChunkRect FirstChunk;
				if (!SiteLayout->GetChunk(FArcEcotopeChunkId{0, ChunkY}, FirstChunk))
				{
					return SetError(OutError, TEXT("Bootstrap valid-area layout has an invalid chunk row."));
				}
				const int32 RowMinY = FirstChunk.BaseCoreRect.Min.Y;
				const int32 RowMaxY = FirstChunk.BaseCoreRect.Max.Y;
				TArray<uint8> RowBlock;
				RowBlock.SetNumUninitialized(Width * (RowMaxY - RowMinY));
				int32 ExpectedMinX = 0;
				for (int32 ChunkX = 0; ChunkX < ChunkGrid.X; ++ChunkX)
				{
					if (!CheckCancellation(Cancellation, OutError))
					{
						return false;
					}
					FArcEcotopeChunkRect Chunk;
					FArcEcotopeSiteChunkLease Lease;
					FString ReaderError;
					if (!SiteLayout->GetChunk(FArcEcotopeChunkId{ChunkX, ChunkY}, Chunk)
						|| Chunk.BaseCoreRect.Min.X != ExpectedMinX
						|| Chunk.BaseCoreRect.Min.Y != RowMinY || Chunk.BaseCoreRect.Max.Y != RowMaxY
						|| !SiteReader->AcquireFields(Chunk.Id, 0, ValidSurfaceMask, Lease, ReaderError))
					{
						return SetError(
							OutError,
							FString::Printf(TEXT("Bootstrap valid area could not read chunk (%d,%d): %s"), ChunkX, ChunkY, *ReaderError));
					}
					const FArcEcotopeSiteByteFieldView* View =
						Lease.GetView().FindByteField(EArcEcotopeSiteField::ValidSurface);
					if (View == nullptr)
					{
						return SetError(OutError, TEXT("Bootstrap valid-area view is missing ValidSurface."));
					}
					for (int32 Y = RowMinY; Y < RowMaxY; ++Y)
					{
						const uint8* Values = View->TryGet(Chunk.BaseCoreRect.Min.X, Y);
						if (Values == nullptr)
						{
							return SetError(OutError, TEXT("Bootstrap valid-area row is incomplete."));
						}
						FMemory::Memcpy(
							RowBlock.GetData() + (Y - RowMinY) * Width + Chunk.BaseCoreRect.Min.X,
							Values,
							Chunk.BaseCoreRect.Width());
					}
					ExpectedMinX = Chunk.BaseCoreRect.Max.X;
				}
				if (ExpectedMinX != Width)
				{
					return SetError(OutError, TEXT("Bootstrap valid-area chunk row does not cover the base grid."));
				}
				for (int32 Y = RowMinY; Y < RowMaxY; ++Y)
				{
					for (int32 X = 0; X < Width; ++X)
					{
						if (!CheckCancellation(Cancellation, OutError))
						{
							return false;
						}
						const int32 CellIndex = X + Y * Width;
						if (RowBlock[X + (Y - RowMinY) * Width] != 0)
						{
							OutArea += Site.GetLevelCellAreaSquareCentimeters(CellIndex, 0);
						}
					}
				}
			}
			return true;
		}

		return SetError(OutError, TEXT("Bootstrap valid-area calculation requires an open site reader."));
	}

	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A)
		{
			return Left.A < Right.A;
		}
		if (Left.B != Right.B)
		{
			return Left.B < Right.B;
		}
		if (Left.C != Right.C)
		{
			return Left.C < Right.C;
		}
		return Left.D < Right.D;
	}

	bool IsValidResponse(
		const FArcForestFrozenResponseRange& Response,
		const float AllowedMinimum,
		const float AllowedMaximum)
	{
		return FMath::IsFinite(Response.Minimum)
			&& FMath::IsFinite(Response.Optimum)
			&& FMath::IsFinite(Response.Maximum)
			&& Response.Minimum >= AllowedMinimum
			&& Response.Maximum <= AllowedMaximum
			&& Response.Minimum <= Response.Optimum
			&& Response.Optimum <= Response.Maximum;
	}

	bool ValidateFrozenInput(
		const FArcForestBootstrapInput& Input,
		TArray<FArcForestFrozenSpeciesSetup>& OutCanonicalSpecies,
		FArcEcotopeSiteChunkReader& OutSiteReader,
		FText& OutError,
		const FArcForestCancellationToken* Cancellation)
	{
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		if (!Input.VolumeId.IsValid())
		{
			return SetError(OutError, TEXT("ArcForest bootstrap requires a valid VolumeId."));
		}
		if (Input.MaxLiveStems <= 0
			|| Input.RecruitmentOversamplingFactor <= 0
			|| Input.MaxStateRecords <= 0
			|| Input.MaxCoverRecords < 0
			|| Input.MaxDeadwoodRecords < 0
			|| Input.MaxBranchRecords < 0
			|| Input.SettingsHash == 0
			|| !FMath::IsFinite(Input.MaximumCrownOccupancy)
			|| Input.MaximumCrownOccupancy <= 0.0
			|| !FMath::IsFinite(Input.UnderstoryShade)
			|| Input.UnderstoryShade < 0.05
			|| Input.UnderstoryShade > 4.0
			|| !FMath::IsFinite(Input.GroundCoverDensity)
			|| Input.GroundCoverDensity < 0.0
			|| Input.GroundCoverDensity > 4.0
			|| !FMath::IsFinite(Input.GroundCoverPatchiness)
			|| Input.GroundCoverPatchiness < 0.0
			|| Input.GroundCoverPatchiness > 0.95)
		{
			return SetError(
				OutError,
				TEXT("ArcForest bootstrap limits, recruitment oversampling factor, settings hash, maximum crown occupancy, understory shade or ground cover are invalid."));
		}
		if (static_cast<int64>(Input.MaxDeadwoodRecords) + Input.MaxBranchRecords > MAX_int32)
		{
			return SetError(
				OutError,
				TEXT("ArcForest bootstrap MaxDeadwoodRecords plus MaxBranchRecords must not exceed 2147483647."));
		}
		if (static_cast<int64>(Input.MaxLiveRecords)
				+ Input.MaxCoverRecords
				+ Input.MaxDeadwoodRecords
				+ Input.MaxBranchRecords != Input.MaxStateRecords)
		{
			return SetError(
				OutError,
				TEXT("ArcForest bootstrap MaxStateRecords must equal MaxLiveRecords plus MaxCoverRecords plus MaxDeadwoodRecords plus MaxBranchRecords."));
		}
		if (static_cast<uint8>(Input.StartMode) > static_cast<uint8>(EArcForestStartMode::ContinueSnapshot))
		{
			return SetError(OutError, TEXT("ArcForest bootstrap start mode is invalid."));
		}
		if (!Input.RegionalRecruitmentConfig.IsValid())
		{
			return SetError(OutError, TEXT("ArcForest regional recruitment configuration is invalid."));
		}

		FText DeadwoodError;
		if (!Input.DeadwoodSettings.IsValid(DeadwoodError))
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("ArcForest bootstrap deadwood settings are invalid: %s"),
					*DeadwoodError.ToString()));
		}

		FText DisturbanceError;
		if (!Input.DisturbanceSettings.IsValid(DisturbanceError))
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("ArcForest bootstrap disturbance settings are invalid: %s"),
					*DisturbanceError.ToString()));
		}

		if (!Input.SiteSource.IsValid())
		{
			return SetError(OutError, TEXT("ArcForest bootstrap requires an immutable site source."));
		}
		const FArcEcotopeSiteMetadata& SiteMetadata = Input.SiteSource->Metadata;
		const int64 ValidationBudgetBytes = static_cast<int64>(FMath::Max(
			CVarSiteValidationBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
#if WITH_EDITORONLY_DATA
		FArcEcotopeSiteBulkValidationResult ValidationResult;
		FArcEcotopeSiteBulkReaderStats ValidationStats;
		FString ValidationError;
		const double ValidationStart = FPlatformTime::Seconds();
		if (!FArcEcotopeSiteChunkReader::ValidateEditorBulkSource(
				Input.SiteSource,
				ValidationBudgetBytes,
				OutSiteReader,
				ValidationResult,
				ValidationStats,
				ValidationError,
				Cancellation))
		{
			return SetError(
				OutError,
				FString::Printf(TEXT("ArcForest bootstrap frozen site source is invalid: %s"), *ValidationError));
		}
		UE_LOGF(
			LogArcEcotope,
			Log,"ArcForest site source validation | status=PASS hash=%llu values=%lld acquires=%lld elapsed=%.2fms",
			ValidationResult.CanonicalHash,
			ValidationResult.LogicalValueCount,
			ValidationResult.AcquireCount,
			(FPlatformTime::Seconds() - ValidationStart) * 1000.0);
#else
		FString ValidationError;
		if (!OutSiteReader.OpenFrozen(Input.SiteSource, ValidationBudgetBytes, ValidationError))
		{
			return SetError(
				OutError,
				FString::Printf(TEXT("ArcForest bootstrap frozen site source is invalid: %s"), *ValidationError));
		}
#endif
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}

		OutCanonicalSpecies = Input.SpeciesSetups;
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		OutCanonicalSpecies.Sort(
			[](const FArcForestFrozenSpeciesSetup& Left, const FArcForestFrozenSpeciesSetup& Right)
			{
				return GuidLess(Left.SpeciesId, Right.SpeciesId);
			});
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}

		FGuid PreviousSpeciesId;
		bool bHasPreviousSpecies = false;
		for (int32 SpeciesIndex = 0; SpeciesIndex < OutCanonicalSpecies.Num(); ++SpeciesIndex)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			const FArcForestFrozenSpeciesSetup& Species = OutCanonicalSpecies[SpeciesIndex];
			if (!Species.SpeciesId.IsValid()
				|| Species.EcologyHash == 0
				|| (bHasPreviousSpecies && Species.SpeciesId == PreviousSpeciesId))
			{
				return SetError(
					OutError,
					FString::Printf(TEXT("Frozen species setup %d has invalid or duplicate identity."), SpeciesIndex));
			}
			if (Species.EcologyHash != UE::ArcForest::ComputeFrozenSpeciesEcologyHash(Species))
			{
				return SetError(
					OutError,
					FString::Printf(
						TEXT("Frozen species setup %d ecology hash does not match its frozen fields."),
						SpeciesIndex));
			}
			if (!FMath::IsFinite(Species.InitialDensityPerHectare)
				|| Species.InitialDensityPerHectare < 0.0f
				|| !FMath::IsFinite(Species.RegionalArrivalPerHectarePerYear)
				|| Species.RegionalArrivalPerHectarePerYear < 0.0f
				|| Species.TargetStemsPerHectare <= 0
				|| !FMath::IsFinite(Species.GrowthSpeed)
				|| !FMath::IsFinite(Species.LateralSpreadRatePerYear)
				|| Species.LateralSpreadRatePerYear < 0.0f
				|| !FMath::IsFinite(Species.ExclusionSensitivity)
				|| Species.ExclusionSensitivity < 0.0f
				|| Species.ExclusionSensitivity > 1.0f
				|| !FMath::IsFinite(Species.MobilityMortalitySensitivity)
				|| Species.MobilityMortalitySensitivity < 0.0f
				|| Species.MobilityMortalitySensitivity > 1.0f
				|| !FMath::IsFinite(Species.CrownOccupancyWeight)
				|| Species.CrownOccupancyWeight < 0.0f)
			{
				return SetError(
					OutError,
					FString::Printf(TEXT("Frozen species setup %d has invalid setup values."), SpeciesIndex));
			}
			if (!IsValidResponse(Species.LightResponse, 0.0f, UE::ArcForest::MaximumLightResponseBound)
				|| !IsValidResponse(Species.MoistureResponse, 0.0f, 1.0f)
				|| !IsValidResponse(
					Species.TemperatureResponse,
					UE::ArcForest::MinimumTemperatureResponseBound,
					UE::ArcForest::MaximumTemperatureResponseBound)
				|| !FMath::IsFinite(Species.MaxSlopeDeg)
				|| Species.MaxSlopeDeg < 0.0f
				|| Species.MaxSlopeDeg > 90.0f
				|| !FMath::IsFinite(Species.SunLoadMin)
				|| Species.SunLoadMin < 0.0f
				|| Species.SunLoadMin > 1.0f
				|| !FMath::IsFinite(Species.TemperatureTolerance)
				|| Species.TemperatureTolerance <= 0.0f
				|| !FMath::IsFinite(Species.WindExposureMax)
				|| Species.WindExposureMax < 0.0f
				|| Species.WindExposureMax > 1.0f
				|| !FMath::IsFinite(Species.FertilityMin)
				|| Species.FertilityMin < 0.0f
				|| Species.FertilityMin > 1.0f)
			{
				return SetError(
					OutError,
					FString::Printf(TEXT("Frozen species setup %d has an invalid habitat response."), SpeciesIndex));
			}
			UE_LOGF(LogArcEcotope, Log,"ArcForest species %ls: habitat gate moisture min/opt/max=%.4f/%.4f/%.4f temperature min/opt/max=%.4f/%.4f/%.4f lightMin=%.4f maxSlopeDeg=%.2f sunLoadMin=%.4f windExposureMax=%.4f fertilityMin=%.4f",
				*Species.SpeciesId.ToString(),
				Species.MoistureResponse.Minimum, Species.MoistureResponse.Optimum, Species.MoistureResponse.Maximum,
				Species.TemperatureResponse.Minimum, Species.TemperatureResponse.Optimum, Species.TemperatureResponse.Maximum,
				Species.LightResponse.Minimum,
				Species.MaxSlopeDeg, Species.SunLoadMin, Species.WindExposureMax, Species.FertilityMin);

			const float PositiveAllometryFields[] =
			{
				Species.IndividualizationHeightCm,
				Species.MaxHeightCm,
				Species.MaxDbhCm,
				Species.MaxAnnualHeightGrowthCm,
				Species.MaxAnnualDbhGrowthCm,
				Species.HeightDbhAllometryCoefficient,
				Species.HeightDbhAllometryExponent,
				Species.CrownRadiusHeightRatio,
				Species.BiomassAllometryCoefficient,
				Species.BiomassAllometryExponent,
			};
			for (const float Value : PositiveAllometryFields)
			{
				if (!CheckCancellation(Cancellation, OutError))
				{
					return false;
				}
				if (!FMath::IsFinite(Value) || Value <= 0.0f)
				{
					return SetError(
						OutError,
						FString::Printf(TEXT("Frozen species setup %d has invalid allometry."), SpeciesIndex));
				}
			}
			const bool bIndividualizes = Species.bIndividualizes;
			const bool bInvalidHeights = bIndividualizes
				? (Species.IndividualizationHeightCm <= InitialHeightCm
					|| Species.MaxHeightCm < Species.IndividualizationHeightCm)
				: UE::ArcForest::CalculateGrowthSpanCm(
					Species.MaxHeightCm,
					UE::ArcForest::RecruitHeightCm) <= 0.0;

			if (bInvalidHeights
				|| Species.MaxAgeYears <= 0
				|| !FMath::IsFinite(Species.BaseAnnualMortalityProbability)
				|| Species.BaseAnnualMortalityProbability < 0.0f
				|| Species.BaseAnnualMortalityProbability > 1.0f
				|| !FMath::IsFinite(Species.OldAgeMortalityProbabilityAtMaxAge)
				|| Species.OldAgeMortalityProbabilityAtMaxAge < 0.0
				|| Species.OldAgeMortalityProbabilityAtMaxAge > 1.0
				|| !FMath::IsFinite(Species.StressMortalityMaximum)
				|| Species.StressMortalityMaximum < 0.0
				|| Species.StressMortalityMaximum > 1.0
				|| !FMath::IsFinite(Species.StressMortalityVigorThreshold)
				|| Species.StressMortalityVigorThreshold < 0.0
				|| Species.StressMortalityVigorThreshold > 1.0
				|| !FMath::IsFinite(Species.StressMortalitySteepness)
				|| Species.StressMortalitySteepness <= 0.0
				|| !FMath::IsFinite(Species.StressMortalityCanopyFloor)
				|| Species.StressMortalityCanopyFloor < 0.0
				|| Species.StressMortalityCanopyFloor > 1.0
				|| !FMath::IsFinite(Species.VigorSmoothingRetention)
				|| Species.VigorSmoothingRetention < 0.0
				|| Species.VigorSmoothingRetention >= 1.0
				|| Species.MaturityAgeYears <= 0
				|| (bIndividualizes && Species.MaturityAgeYears > Species.MaxAgeYears)
				|| !FMath::IsFinite(Species.RecruitsPerMatureStemPerYear)
				|| Species.RecruitsPerMatureStemPerYear < 0.0f
				|| !FMath::IsFinite(Species.AnnualPropaguleProduction)
				|| Species.AnnualPropaguleProduction < 0.0f
				|| !FMath::IsFinite(Species.MeanDispersalDistanceCm)
				|| Species.MeanDispersalDistanceCm < 0.0f)
			{
				return SetError(
					OutError,
					FString::Printf(TEXT("Frozen species setup %d has invalid ecology values."), SpeciesIndex));
			}

			PreviousSpeciesId = Species.SpeciesId;
			bHasPreviousSpecies = true;
		}

		TArray<UE::ArcForest::FSettingsHashSpecies> SettingsSpecies;
		SettingsSpecies.Reserve(OutCanonicalSpecies.Num());
		for (const FArcForestFrozenSpeciesSetup& Species : OutCanonicalSpecies)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			UE::ArcForest::FSettingsHashSpecies& Entry = SettingsSpecies.AddDefaulted_GetRef();
			Entry.SpeciesId = Species.SpeciesId;
			Entry.RegionalArrivalPerHectarePerYear = Species.RegionalArrivalPerHectarePerYear;
		}
		const FArcEcotopeBudgetSettings RecomputedBudgetSettings = UE::ArcEcotope::MakeBudgetSettings(Input);
		const uint64 RecomputedSettingsHash = UE::ArcForest::ComputeSettingsHash(
			Input.Seed,
			SiteMetadata.CellSizeCm,
			SiteMetadata.RecordGridLevel,
			RecomputedBudgetSettings,
			Input.DeadwoodSettings,
			Input.MaximumCrownOccupancy,
			Input.UnderstoryShade,
			Input.GroundCoverDensity,
			Input.GroundCoverPatchiness,
			SettingsSpecies,
			Input.DisturbanceSettings);
		if (Input.SettingsHash != RecomputedSettingsHash)
		{
			return SetError(
				OutError,
				TEXT("ArcForest bootstrap settings hash does not match the frozen values."));
		}
		return true;
	}

	bool CalculateInitialBiomassPerStem(
		const FArcForestFrozenSpeciesSetup& Species,
		float& OutBiomass,
		FText& OutError)
	{
		const double DbhCm = FMath::Pow(
			static_cast<double>(InitialHeightCm) / Species.HeightDbhAllometryCoefficient,
			1.0 / Species.HeightDbhAllometryExponent);
		const double BiomassKg = Species.BiomassAllometryCoefficient
			* FMath::Pow(DbhCm, Species.BiomassAllometryExponent);
		if (!FMath::IsFinite(DbhCm) || DbhCm < 0.0
			|| !FMath::IsFinite(BiomassKg) || BiomassKg < 0.0
			|| BiomassKg > MAX_flt)
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("Species %s cannot produce finite 30 cm allometry."),
					*Species.SpeciesId.ToString()));
		}
		OutBiomass = static_cast<float>(BiomassKg);
		return true;
	}

	bool BuildSpeciesCandidates(
		const FArcForestBootstrapInput& Input,
		const FArcEcotopeSiteChunkReader* SiteReader,
		const FGuid& SimulationId,
		const FArcForestFrozenSpeciesSetup& Species,
		const double ValidAreaHectares,
		TArray<FCohortCandidate>& OutCandidates,
		FText& OutError,
		const FArcForestCancellationToken* Cancellation)
	{
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		if (Species.InitialDensityPerHectare == 0.0f)
		{
			return true;
		}
		if (ValidAreaHectares <= 0.0)
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("Species %s requests initial density but the site has no valid surface area."),
					*Species.SpeciesId.ToString()));
		}

		const double ExpectedStemCount =
			static_cast<double>(Species.InitialDensityPerHectare) * ValidAreaHectares;
		int64 DesiredStemCount = 0;
		const uint64 RoundingKey = UE::ArcForest::MakeKey(
			SimulationId,
			Input.Seed,
			UE::ArcForest::EDeterministicDomain::BootstrapTotalRounding,
			Species.SpeciesId);
		if (!UE::ArcForest::TryRoundNonNegative(ExpectedStemCount, RoundingKey, DesiredStemCount))
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("Species %s initial density overflows the supported stem count."),
					*Species.SpeciesId.ToString()));
		}
		if (DesiredStemCount == 0)
		{
			return true;
		}

		TArray<FCellWeight> CellWeights;
		const FArcEcotopeSiteMetadata& SiteMetadata = Input.SiteSource->Metadata;
		CellWeights.Reserve(SiteMetadata.GetCellCount());
		UE::ArcForest::Habitat::FHabitatResponseTable Responses;
		UE::ArcForest::Habitat::GetHabitatResponses(Species, Responses.Responses);
		const auto AddCellWeight = [&](const int32 CellIndex, const UE::ArcForest::Habitat::FHabitatSiteSample& SiteSample)
		{
			const double CellArea = SiteMetadata.GetLevelCellAreaSquareCentimeters(CellIndex, 0);
			double ResponseOutputs[HabitatResponseCount];
			EvaluateHabitatResponseTerms(SiteSample, Responses, 1.0, ResponseOutputs);
			const double Suitability = AggregateHabitat(
				ResponseOutputs,
				UE::ArcForest::Habitat::EvaluateHabitatThresholdMask(SiteSample, Species));
			// EstablishmentWear is absent on purpose: erosion filters seeds and seedlings, and this is
			// the mature initial stand, so applying it would strip an eroding slope of the trees holding it.
			const double CutFactor = UE::ArcForest::Habitat::MakeExclusionFactor(
					static_cast<double>(SiteSample.ModificationCut),
					Species.ExclusionSensitivity)
				* UE::ArcForest::Habitat::MakeExclusionFactor(
					static_cast<double>(SiteSample.BareRock),
					Species.ExclusionSensitivity)
				* UE::ArcForest::Habitat::MakeExclusionFactor(
					static_cast<double>(SiteSample.ChannelBuffer),
					Species.ExclusionSensitivity);
			const double Weight = CellArea * Suitability * CutFactor;
			if (!FMath::IsFinite(Weight) || Weight < 0.0)
			{
				return SetError(
					OutError,
					FString::Printf(
						TEXT("Species %s produces non-finite bootstrap suitability."),
						*Species.SpeciesId.ToString()));
			}
			if (Weight > 0.0)
			{
				FCellWeight& CellWeight = CellWeights.AddDefaulted_GetRef();
				CellWeight.CellIndex = CellIndex;
				CellWeight.Weight = Weight;
			}
			return true;
		};

		if (SiteReader != nullptr && SiteReader->IsOpen())
		{
			constexpr uint32 HabitatFieldMask =
				(1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::Slope))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::Moisture))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::Fertility))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::SunLoad))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::WindExposure))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::Temperature))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::ModificationCut))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::BareRock))
				| (1u << static_cast<uint32>(EArcEcotopeSiteField::ChannelBuffer));
			for (int32 ChunkIndex = 0; ChunkIndex < Input.SiteSource->Layout.GetChunkCount(); ++ChunkIndex)
			{
				if (!CheckCancellation(Cancellation, OutError))
				{
					return false;
				}
				FArcEcotopeChunkRect Chunk;
				FArcEcotopeSiteChunkLease Lease;
				FString ReaderError;
				if (!Input.SiteSource->Layout.GetChunk(ChunkIndex, Chunk)
					|| !SiteReader->AcquireFields(Chunk.Id, 0, HabitatFieldMask, Lease, ReaderError))
				{
					return SetError(
						OutError,
						FString::Printf(TEXT("Bootstrap habitat could not read chunk %d: %s"), ChunkIndex, *ReaderError));
				}
				const FArcEcotopeSiteChunkView& View = Lease.GetView();
				const FArcEcotopeSiteByteFieldView* ValidSurface =
					View.FindByteField(EArcEcotopeSiteField::ValidSurface);
				for (int32 Y = Chunk.BaseCoreRect.Min.Y; Y < Chunk.BaseCoreRect.Max.Y; ++Y)
				{
					for (int32 X = Chunk.BaseCoreRect.Min.X; X < Chunk.BaseCoreRect.Max.X; ++X)
					{
						if (!CheckCancellation(Cancellation, OutError))
						{
							return false;
						}
						const uint8* bValid = ValidSurface != nullptr ? ValidSurface->TryGet(X, Y) : nullptr;
						if (bValid == nullptr)
						{
							return SetError(OutError, TEXT("Bootstrap habitat valid-surface view is incomplete."));
						}
						if (*bValid == 0)
						{
							continue;
						}
						UE::ArcForest::Habitat::FHabitatSiteSample SiteSample;
						if (!UE::ArcForest::Habitat::TryReadHabitatSiteSample(View, X, Y, SiteSample))
						{
							return SetError(
								OutError,
								FString::Printf(TEXT("Bootstrap habitat fields are incomplete at base cell (%d,%d)."), X, Y));
						}
						const int32 CellIndex = X + Y * SiteMetadata.GridDimensions.X;
						if (!AddCellWeight(CellIndex, SiteSample))
						{
							return false;
						}
					}
				}
			}
		}
		else
		{
			return SetError(OutError, TEXT("Bootstrap habitat sampling requires an open site reader."));
		}

		CellWeights.Sort([](const FCellWeight& Left, const FCellWeight& Right)
		{
			return Left.CellIndex < Right.CellIndex;
		});
		double TotalWeight = 0.0;
		for (const FCellWeight& CellWeight : CellWeights)
		{
			TotalWeight += CellWeight.Weight;
			if (!FMath::IsFinite(TotalWeight))
			{
				return SetError(OutError, TEXT("Bootstrap suitability weights overflowed."));
			}
		}

		if (CellWeights.IsEmpty() || TotalWeight <= 0.0)
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("Species %s requests initial density but has zero suitability on every valid site cell."),
					*Species.SpeciesId.ToString()));
		}

		TArray<FCellAllocation> Allocations;
		Allocations.Reserve(CellWeights.Num());
		int64 AssignedStemCount = 0;
		for (const FCellWeight& CellWeight : CellWeights)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			const double ExactAllocation =
				static_cast<double>(DesiredStemCount) * (CellWeight.Weight / TotalWeight);
			if (!FMath::IsFinite(ExactAllocation) || ExactAllocation < 0.0
				|| ExactAllocation > static_cast<double>(MAX_int64))
			{
				return SetError(OutError, TEXT("Bootstrap cell allocation overflowed."));
			}

			FCellAllocation& Allocation = Allocations.AddDefaulted_GetRef();
			Allocation.CellIndex = CellWeight.CellIndex;
			Allocation.StemCount = static_cast<int64>(FMath::FloorToDouble(ExactAllocation));
			Allocation.Fraction = ExactAllocation - static_cast<double>(Allocation.StemCount);
			if (AssignedStemCount > MAX_int64 - Allocation.StemCount)
			{
				return SetError(OutError, TEXT("Bootstrap assigned stem count overflowed."));
			}
			AssignedStemCount += Allocation.StemCount;

			if (Allocation.Fraction > 0.0)
			{
				const uint64 SelectionKey = UE::ArcForest::MakeKey(
					SimulationId,
					Input.Seed,
					UE::ArcForest::EDeterministicDomain::BootstrapCellSelection,
					Species.SpeciesId,
					Allocation.CellIndex);
				Allocation.SelectionScore =
					-FMath::Loge(UE::ArcForest::KeyToUnitOpen(SelectionKey)) / Allocation.Fraction;
			}
			else
			{
				Allocation.SelectionScore = TNumericLimits<double>::Max();
			}
		}

		if (AssignedStemCount > DesiredStemCount)
		{
			return SetError(OutError, TEXT("Bootstrap floating-point allocation exceeded the desired stem count."));
		}
		const int64 Remainder = DesiredStemCount - AssignedStemCount;
		Allocations.Sort(
			[](const FCellAllocation& Left, const FCellAllocation& Right)
			{
				if (Left.SelectionScore != Right.SelectionScore)
				{
					return Left.SelectionScore < Right.SelectionScore;
				}
				return Left.CellIndex < Right.CellIndex;
			});
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		if (Remainder > Allocations.Num()
			|| (Remainder > 0 && Allocations[static_cast<int32>(Remainder - 1)].Fraction <= 0.0))
		{
			return SetError(OutError, TEXT("Bootstrap fractional cell allocation is not representable."));
		}
		for (int64 AllocationIndex = 0; AllocationIndex < Remainder; ++AllocationIndex)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			++Allocations[static_cast<int32>(AllocationIndex)].StemCount;
		}

		float BiomassPerStemKg = 0.0f;
		if (!CalculateInitialBiomassPerStem(Species, BiomassPerStemKg, OutError))
		{
			return false;
		}

		// Suitability is weighted on the base grid but a record occupies a record cell, so
		// allocations sharing a record cell are folded into one candidate before admission.
		TMap<int32, int32> CandidateIndexByRecordCell;
		for (const FCellAllocation& Allocation : Allocations)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			if (Allocation.StemCount <= 0)
			{
				continue;
			}

			const int32 RecordCellIndex = SiteMetadata.MapCellIndexBetweenLevels(
				Allocation.CellIndex, 0, SiteMetadata.RecordGridLevel);
			if (RecordCellIndex == INDEX_NONE)
			{
				return SetError(OutError, TEXT("Bootstrap cell allocation could not be mapped onto the record grid."));
			}
			if (int32* ExistingCandidateIndex = CandidateIndexByRecordCell.Find(RecordCellIndex))
			{
				FCohortCandidate& Existing = OutCandidates[*ExistingCandidateIndex];
				if (Existing.StemCount > MAX_int64 - Allocation.StemCount)
				{
					return SetError(OutError, TEXT("Bootstrap record cell stem count overflowed."));
				}
				Existing.StemCount += Allocation.StemCount;
				continue;
			}

			CandidateIndexByRecordCell.Add(RecordCellIndex, OutCandidates.Num());
			FCohortCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.SpeciesId = Species.SpeciesId;
			Candidate.bIndividualizes = Species.bIndividualizes;
			Candidate.CellIndex = RecordCellIndex;
			Candidate.StemCount = Allocation.StemCount;
			Candidate.BiomassPerStemKg = BiomassPerStemKg;
			Candidate.AdmissionPriority = UE::ArcForest::MakeKey(
				SimulationId,
				Input.Seed,
				UE::ArcForest::EDeterministicDomain::BootstrapAdmission,
				Species.SpeciesId,
				RecordCellIndex);
		}
		return true;
	}

	bool CheckSnapshotBudgets(
		const FArcForestWorldSnapshot& Snapshot,
		int32 MaxLiveStems,
		int32 MaxLiveRecords,
		int32 MaxCoverRecords,
		int32 MaxDeadwoodRecords,
		int32 MaxBranchRecords,
		int32 MaxStateRecords,
		FText& OutError,
		const FArcForestCancellationToken* Cancellation)
	{
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		const int64 LiveRecordCount =
			static_cast<int64>(Snapshot.Individuals.Num())
			+ Snapshot.Cohorts.Num();
		const int64 CoverRecordCount = Snapshot.Patches.Num();
		if (LiveRecordCount > MaxLiveRecords)
		{
			return SetError(OutError, TEXT("Continued snapshot exceeds MaxLiveRecords."));
		}
		if (CoverRecordCount > MaxCoverRecords)
		{
			return SetError(OutError, TEXT("Continued snapshot exceeds MaxCoverRecords."));
		}

		int64 BranchRecordCount = 0;
		for (const FArcForestDeadwoodRecord& Deadwood : Snapshot.Deadwood)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			if (Deadwood.Type == EArcForestDeadwoodType::Branch)
			{
				++BranchRecordCount;
			}
		}
		const int64 NonBranchDeadwoodRecordCount = Snapshot.Deadwood.Num() - BranchRecordCount;
		if (NonBranchDeadwoodRecordCount > MaxDeadwoodRecords)
		{
			return SetError(OutError, TEXT("Continued snapshot exceeds MaxDeadwoodRecords."));
		}
		if (BranchRecordCount > MaxBranchRecords)
		{
			return SetError(OutError, TEXT("Continued snapshot exceeds MaxBranchRecords."));
		}

		double LiveStemCount = 0.0;
		for (const FArcForestIndividualRecord& Individual : Snapshot.Individuals)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			if (Individual.LifeStatus == EArcForestLifeStatus::Alive)
			{
				LiveStemCount += 1.0;
			}
		}
		for (const FArcForestCohortRecord& Cohort : Snapshot.Cohorts)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			LiveStemCount += static_cast<double>(Cohort.StemCount);
		}
		for (const FArcForestPatchRecord& Patch : Snapshot.Patches)
		{
			if (!CheckCancellation(Cancellation, OutError))
			{
				return false;
			}
			LiveStemCount += FMath::CeilToDouble(Patch.StemOrCoverEquivalent);
		}
		if (!FMath::IsFinite(LiveStemCount) || LiveStemCount > MaxLiveStems)
		{
			return SetError(OutError, TEXT("Continued snapshot exceeds MaxLiveStems."));
		}
		const int64 TotalRecordCount = LiveRecordCount + CoverRecordCount + Snapshot.Deadwood.Num();
		if (TotalRecordCount > MaxStateRecords)
		{
			return SetError(OutError, TEXT("Continued snapshot exceeds MaxStateRecords."));
		}
		return true;
	}
}

bool FArcForestStartRequest::MakeBootstrapAndSpinUp(
	const EArcForestStartMode InitializationMode,
	const int32 SpinUpYears,
	FArcForestStartRequest& OutRequest,
	FText& OutError)
{
	if (InitializationMode == EArcForestStartMode::ContinueSnapshot)
	{
		OutError = FText::FromString(
			TEXT("Bootstrap & Spin Up requires BareGround or BootstrapCohorts initialization."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: MakeBootstrapAndSpinUp rejected InitializationMode=%d (ContinueSnapshot is not valid here).",
			static_cast<int32>(InitializationMode));
		return false;
	}
	if (SpinUpYears < 0)
	{
		OutError = FText::FromString(TEXT("SpinUpYears cannot be negative."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: MakeBootstrapAndSpinUp rejected SpinUpYears=%d (mode=%d).",
			SpinUpYears,
			static_cast<int32>(InitializationMode));
		return false;
	}

	FArcForestStartRequest Candidate;
	Candidate.StartMode = InitializationMode;
	Candidate.AnnualStepsAfterInitialization = SpinUpYears;
	OutRequest = Candidate;
	OutError = FText::GetEmpty();
	return true;
}

bool FArcForestStartRequest::MakeContinue(
	const int32 YearsToSimulate,
	FArcForestStartRequest& OutRequest,
	FText& OutError)
{
	if (YearsToSimulate <= 0)
	{
		OutError = FText::FromString(TEXT("Continue requires at least one annual simulation step."));
		return false;
	}

	FArcForestStartRequest Candidate;
	Candidate.StartMode = EArcForestStartMode::ContinueSnapshot;
	Candidate.AnnualStepsAfterInitialization = YearsToSimulate;
	OutRequest = Candidate;
	OutError = FText::GetEmpty();
	return true;
}

bool FArcForestBootstrap::CreateInitialSnapshot(
	const FArcForestBootstrapInput& Input,
	FArcForestWorldSnapshot& OutSnapshot,
	FText& OutError,
	const FArcForestCancellationToken* Cancellation)
{
	using namespace ArcForestBootstrapPrivate;
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_CreateInitialSnapshot);
	UE::ArcForest::FDiagnosticScopeReport Report(TEXT("ArcForest CreateInitialSnapshot"));
	UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics = Report.GetPhases();

	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest bootstrap: CreateInitialSnapshot invoked (VolumeId=%ls, StartMode=%d, Seed=%d, MaxLiveStems=%d, MaxStateRecords=%d, SiteSourceValid=%ls).",
		*Input.VolumeId.ToString(),
		static_cast<int32>(Input.StartMode),
		Input.Seed,
		Input.MaxLiveStems,
		Input.MaxStateRecords,
		Input.SiteSource.IsValid() ? TEXT("true") : TEXT("false"));

	if (!CheckCancellation(Cancellation, OutError))
	{
		Report.SetOutcome(TEXT("cancelled"));
		return false;
	}

	TArray<FArcForestFrozenSpeciesSetup> CanonicalSpecies;
	FArcEcotopeSiteChunkReader SiteReader;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_ValidateFrozenInput);
		if (!ValidateFrozenInput(Input, CanonicalSpecies, SiteReader, OutError, Cancellation))
		{
			Report.SetOutcome(TEXT("invalid frozen input"));
			return false;
		}
	}
	const FArcEcotopeSiteMetadata& SiteMetadata = Input.SiteSource->Metadata;
	const FArcEcotopeChunkLayout* SiteLayout = &Input.SiteSource->Layout;
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest bootstrap: CreateInitialSnapshot validated frozen input (SpeciesCount=%d, ChunkCount=%d, SiteReaderOpen=%ls).",
		CanonicalSpecies.Num(),
		SiteLayout->GetChunkCount(),
		SiteReader.IsOpen() ? TEXT("true") : TEXT("false"));
	const int64 CompatibilityScheduleBudgetBytes = static_cast<int64>(FMath::Max(
		CVarSiteValidationBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;

	const FGuid ExpectedSimulationId = UE::ArcForest::MakeSimulationId(Input.VolumeId, Input.Seed);
	TArray<FArcForestSpeciesEcologyHash> EcologyHashes;
	EcologyHashes.Reserve(CanonicalSpecies.Num());
	for (const FArcForestFrozenSpeciesSetup& Species : CanonicalSpecies)
	{
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		FArcForestSpeciesEcologyHash& Hash = EcologyHashes.AddDefaulted_GetRef();
		Hash.SpeciesId = Species.SpeciesId;
		Hash.EcologyHash = Species.EcologyHash;
	}

	if (Input.StartMode == EArcForestStartMode::ContinueSnapshot)
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_ContinueValidation);
		if (!CheckCancellation(Cancellation, OutError))
		{
			Report.SetOutcome(TEXT("cancelled"));
			return false;
		}
		FText SnapshotError;
		const EArcForestSimulationStateStatus Status = Input.SourceSnapshot.Validate(
			SnapshotError,
			Input.SettingsHash,
			&EcologyHashes);
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		if (Status != EArcForestSimulationStateStatus::Valid)
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("ContinueSnapshot requires exact compatible state: %s"),
					*SnapshotError.ToString()));
		}
		FArcForestWorldSiteValidationStats SiteStats;
		if (!Input.SourceSnapshot.ValidateSiteCompatibility(
				SiteMetadata,
				*SiteLayout,
				SiteReader,
				CompatibilityScheduleBudgetBytes,
				SiteStats,
				SnapshotError))
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("ContinueSnapshot site compatibility failed: %s"),
					*SnapshotError.ToString()));
		}
		if (Input.SourceSnapshot.Seed != Input.Seed
			|| Input.SourceSnapshot.SimulationId != ExpectedSimulationId)
		{
			return SetError(
				OutError,
				TEXT("ContinueSnapshot state does not belong to this exact VolumeId and seed."));
		}
		if (!(Input.SourceSnapshot.RegionalRecruitmentConfig == Input.RegionalRecruitmentConfig))
		{
			return SetError(
				OutError,
				TEXT("ContinueSnapshot regional recruitment configuration differs from the authoritative snapshot lineage. Clear State and bootstrap again after changing RegionalRecruitmentMaxProposalRounds."));
		}
		if (!CheckSnapshotBudgets(
			Input.SourceSnapshot,
			Input.MaxLiveStems,
			Input.MaxLiveRecords,
			Input.MaxCoverRecords,
			Input.MaxDeadwoodRecords,
			Input.MaxBranchRecords,
			Input.MaxStateRecords,
			OutError,
			Cancellation))
		{
			return false;
		}
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}

		OutSnapshot = Input.SourceSnapshot;
		OutError = FText::GetEmpty();
		Report.SetOutcome(FString::Printf(
			TEXT("continued snapshot, individuals=%d cohorts=%d patches=%d deadwood=%d"),
			OutSnapshot.Individuals.Num(),
			OutSnapshot.Cohorts.Num(),
			OutSnapshot.Patches.Num(),
			OutSnapshot.Deadwood.Num()));
		return true;
	}

	FArcForestWorldSnapshot Candidate;
	Candidate.SchemaVersion = FArcForestWorldSnapshot::GetCurrentSchemaVersion();
	Candidate.SimulationId = ExpectedSimulationId;
	Candidate.CurrentYear = 0;
	Candidate.Seed = Input.Seed;
	Candidate.SiteSnapshotHash = SiteMetadata.SiteSnapshotHash;
	Candidate.SettingsHash = Input.SettingsHash;
	Candidate.RegionalRecruitmentConfig = Input.RegionalRecruitmentConfig;
	Candidate.SpeciesEcologyHashes = EcologyHashes;
	Candidate.StateRevision = 1;

	if (Input.StartMode == EArcForestStartMode::BootstrapCohorts)
	{
		double ValidAreaSquareCentimeters = 0.0;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_ValidArea);
			if (!CalculateValidAreaSquareCentimeters(
					SiteMetadata,
					&SiteReader,
					SiteLayout,
					ValidAreaSquareCentimeters,
					OutError,
					Cancellation))
			{
				return false;
			}
		}
		if (!FMath::IsFinite(ValidAreaSquareCentimeters))
		{
			return SetError(OutError, TEXT("Valid site surface area overflowed during bootstrap."));
		}
		const double ValidAreaHectares = ValidAreaSquareCentimeters / SquareCentimetersPerHectare;

		TArray<FCohortCandidate> CohortCandidates;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_SpeciesCandidates);
			for (const FArcForestFrozenSpeciesSetup& Species : CanonicalSpecies)
			{
				if (!CheckCancellation(Cancellation, OutError))
				{
					return false;
				}
				if (!BuildSpeciesCandidates(
					Input,
					&SiteReader,
					ExpectedSimulationId,
					Species,
					ValidAreaHectares,
					CohortCandidates,
					OutError,
					Cancellation))
				{
					return false;
				}
			}
#if WITH_EDITORONLY_DATA
			if (SiteReader.IsOpen())
			{
				SiteReader.LogBulkStats(TEXT("bootstrap-habitat"));
			}
#endif
		}

		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_CandidateSort);
			CohortCandidates.Sort(
				[](const FCohortCandidate& Left, const FCohortCandidate& Right)
				{
					if (Left.AdmissionPriority != Right.AdmissionPriority)
					{
						return Left.AdmissionPriority < Right.AdmissionPriority;
					}
					if (Left.SpeciesId != Right.SpeciesId)
					{
						return GuidLess(Left.SpeciesId, Right.SpeciesId);
					}
					return Left.CellIndex < Right.CellIndex;
				});
		}
		if (!CheckCancellation(Cancellation, OutError))
		{
			return false;
		}

		TSet<uint64> RecordIds;
		int64 RemainingLiveStems = Input.MaxLiveStems;
		int32 RemainingLiveRecords = Input.MaxLiveRecords;
		int32 RemainingCoverRecords = Input.MaxCoverRecords;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_Admission);
			for (const FCohortCandidate& CohortCandidate : CohortCandidates)
			{
				if (!CheckCancellation(Cancellation, OutError))
				{
					return false;
				}
				if (RemainingLiveStems <= 0)
				{
					break;
				}
				if (RemainingLiveRecords <= 0 && RemainingCoverRecords <= 0)
				{
					break;
				}

				const bool bIndividualizingCandidate = CohortCandidate.bIndividualizes;
				const int32 RemainingRecordsForCandidate =
					bIndividualizingCandidate ? RemainingLiveRecords : RemainingCoverRecords;
				if (RemainingRecordsForCandidate <= 0)
				{
					continue;
				}

				const int64 AdmittedStems = FMath::Min(CohortCandidate.StemCount, RemainingLiveStems);
				if (AdmittedStems <= 0)
				{
					continue;
				}

				uint64 RecordId = UE::ArcForest::MakeKey(
					ExpectedSimulationId,
					Input.Seed,
					UE::ArcForest::EDeterministicDomain::BootstrapRecordIdentity,
					CohortCandidate.SpeciesId,
					CohortCandidate.CellIndex);
				if (RecordId == 0 || RecordIds.Contains(RecordId))
				{
					return SetError(OutError, TEXT("Bootstrap produced a zero or colliding stable record identifier."));
				}
				RecordIds.Add(RecordId);

				const double CohortBiomass =
					static_cast<double>(CohortCandidate.BiomassPerStemKg) * AdmittedStems;
				if (!FMath::IsFinite(CohortBiomass) || CohortBiomass > MAX_flt)
				{
					return SetError(OutError, TEXT("Bootstrap cohort biomass overflowed."));
				}
				const float SerializedCohortBiomass = static_cast<float>(CohortBiomass);
				if (!FMath::IsFinite(SerializedCohortBiomass))
				{
					return SetError(OutError, TEXT("Bootstrap cohort biomass is not representable as serialized float state."));
				}

				if (bIndividualizingCandidate)
				{
					FArcForestCohortRecord& Record = Candidate.Cohorts.AddDefaulted_GetRef();
					Record.Id = RecordId;
					Record.SpeciesId = CohortCandidate.SpeciesId;
					Record.CellIndex = CohortCandidate.CellIndex;
					Record.BirthYear = 0;
					Record.HeightBin = InitialHeightBin;
					Record.StemCount = AdmittedStems;
					Record.MeanHeightCm = InitialHeightCm;
					Record.MeanAgeYears = 0.0f;
					Record.MeanVigor = 1.0f;
					Record.BiomassKg = SerializedCohortBiomass;
				}
				else
				{
					const float SerializedEquivalent = static_cast<float>(AdmittedStems);
					if (!FMath::IsFinite(SerializedEquivalent)
						|| static_cast<int64>(SerializedEquivalent) != AdmittedStems)
					{
						return SetError(
							OutError,
							TEXT("Bootstrap patch cover-equivalent is not exactly representable."));
					}
					FArcForestPatchRecord& Record = Candidate.Patches.AddDefaulted_GetRef();
					Record.Id = RecordId;
					Record.SpeciesId = CohortCandidate.SpeciesId;
					Record.CellIndex = CohortCandidate.CellIndex;
					Record.StemOrCoverEquivalent = SerializedEquivalent;
					Record.MeanHeightCm = InitialHeightCm;
					Record.MeanAgeYears = 0.0f;
					Record.MeanVigor = 1.0f;
					Record.BiomassKg = SerializedCohortBiomass;
				}

				RemainingLiveStems -= AdmittedStems;
				if (bIndividualizingCandidate)
				{
					--RemainingLiveRecords;
				}
				else
				{
					--RemainingCoverRecords;
				}
			}
		}
	}

	if (!CheckCancellation(Cancellation, OutError))
	{
		return false;
	}
	Candidate.Cohorts.Sort(
		[](const FArcForestCohortRecord& Left, const FArcForestCohortRecord& Right)
		{
			return Left.Id < Right.Id;
		});
	if (!CheckCancellation(Cancellation, OutError))
	{
		return false;
	}
	if (!CheckCancellation(Cancellation, OutError))
	{
		return false;
	}
	Candidate.Patches.Sort(
		[](const FArcForestPatchRecord& Left, const FArcForestPatchRecord& Right)
		{
			return Left.Id < Right.Id;
		});
	if (!CheckCancellation(Cancellation, OutError))
	{
		return false;
	}
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_Canonicalize);
		Candidate.CanonicalizeAndRefreshChecksum();
	}
	if (!CheckCancellation(Cancellation, OutError))
	{
		return false;
	}
	FText CandidateError;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Bootstrap_Validate);
		if (Candidate.Validate(
			CandidateError,
			Input.SettingsHash,
			&EcologyHashes) != EArcForestSimulationStateStatus::Valid)
		{
			return SetError(
				OutError,
				FString::Printf(TEXT("ArcForest bootstrap produced invalid state: %s"), *CandidateError.ToString()));
		}
		FArcForestWorldSiteValidationStats SiteStats;
		if (!Candidate.ValidateSiteCompatibility(
				SiteMetadata,
				*SiteLayout,
				SiteReader,
				CompatibilityScheduleBudgetBytes,
				SiteStats,
				CandidateError))
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("ArcForest bootstrap produced site-incompatible state: %s"),
					*CandidateError.ToString()));
		}
	}
	if (!CheckCancellation(Cancellation, OutError))
	{
		return false;
	}

	OutSnapshot = MoveTemp(Candidate);
	OutError = FText::GetEmpty();
	return true;
}
