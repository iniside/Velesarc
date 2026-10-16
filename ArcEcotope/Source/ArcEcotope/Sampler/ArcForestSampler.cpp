// Copyright Lukasz Baran. All Rights Reserved.

#include "Sampler/ArcForestSampler.h"

#include "ArcEcotopeModule.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Sampler/ArcForestSamplerDiagnostics.h"
#include "Sampler/ArcForestSamplerInput.h"
#include "Sampler/ArcForestSamplerKeys.h"
#include "Sampler/ArcForestSamplerSizeClasses.h"
#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcForestHabitatResponse.h"
#include "Species/ArcEcotopeSpeciesModel.h"

namespace ArcForestSamplerPrivate
{
	constexpr double DispersalFloor = 0.05;
	constexpr double CandidateOversample = 8.0;
	constexpr double IndividualizationBandFactor = 4.0;
	constexpr double DispersalKernelCutoffSigmas = 3.0;
	constexpr int32 MaximumCandidatesPerRecordCell = 65536;
	constexpr int64 ReaderCacheBudgetBytes = 64ll * 1024ll * 1024ll;
	constexpr int64 SurvivorWorkingSetBudgetBytes = 16ll * ReaderCacheBudgetBytes;

	constexpr uint32 HabitatFieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::LocalSurfaceZ))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::RecordValid))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Slope))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Moisture))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Fertility))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::SunLoad))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::WindExposure))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Temperature))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ModificationCut))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::BareRock))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ChannelBuffer));

	constexpr uint32 PlacementFieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::LocalSurfaceZ))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::RecordValid));

	enum class ESamplerKeySlot : int32
	{
		PositionX = 0,
		PositionY = 1,
		SpeciesChoice = 2,
		HeightJitter = 3,
		Acceptance = 4,
		Priority = 5,
	};

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

	bool Fail(FArcForestSamplerResult& OutResult, const EArcForestRunStatus Status, const FString& Message)
	{
		OutResult.Status = Status;
		OutResult.Error = Message;
		OutResult.Snapshot = FArcForestWorldSnapshot();
		UE_LOGF(LogArcEcotope, Error,"ArcForest sampler: %ls", *Message);
		return false;
	}

	int64 MakeFieldOrdinal(const int32 Pass, const int32 Attempt, const ESamplerKeySlot Slot)
	{
		return (static_cast<int64>(Pass) * 1048576ll + static_cast<int64>(Attempt)) * 8ll
			+ static_cast<int64>(Slot);
	}

	uint64 MakeTargetKey(const int32 SetupIndex, const int32 BandIndex)
	{
		return (static_cast<uint64>(static_cast<uint32>(SetupIndex)) << 32)
			| static_cast<uint64>(static_cast<uint32>(BandIndex));
	}

	double CircleAntiderivative(const double U, const double Radius)
	{
		return 0.5 * (U * FMath::Sqrt(FMath::Max(0.0, Radius * Radius - U * U))
			+ Radius * Radius * FMath::Asin(FMath::Clamp(U / Radius, -1.0, 1.0)));
	}

	double QuarterDiskArea(const double Radius, const double InX, const double InY)
	{
		const double X = FMath::Min(InX, Radius);
		const double Y = FMath::Min(InY, Radius);
		if (X <= 0.0 || Y <= 0.0)
		{
			return 0.0;
		}
		if (X * X + Y * Y <= Radius * Radius)
		{
			return X * Y;
		}
		const double Split = FMath::Sqrt(FMath::Max(0.0, Radius * Radius - Y * Y));
		return Split * Y + CircleAntiderivative(X, Radius) - CircleAntiderivative(Split, Radius);
	}

	double SignedQuadrantArea(const double Radius, const double X, const double Y)
	{
		const double SignX = X < 0.0 ? -1.0 : 1.0;
		const double SignY = Y < 0.0 ? -1.0 : 1.0;
		return SignX * SignY * QuarterDiskArea(Radius, FMath::Abs(X), FMath::Abs(Y));
	}

	double DiskRectangleOverlapArea(
		const double Radius,
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		return SignedQuadrantArea(Radius, MaxX, MaxY)
			- SignedQuadrantArea(Radius, MinX, MaxY)
			- SignedQuadrantArea(Radius, MaxX, MinY)
			+ SignedQuadrantArea(Radius, MinX, MinY);
	}

	struct FTreeSpecies
	{
		int32 SetupIndex = INDEX_NONE;
		const FArcForestFrozenSpeciesSetup* Setup = nullptr;
		double CrownRadiusPerHeight = 0.0;
	};

	struct FBandEntry
	{
		int32 TreeIndex = INDEX_NONE;
		int32 BandIndex = INDEX_NONE;
		float MinHeightCm = 0.0f;
		float MaxHeightCm = 0.0f;
	};

	struct FPlacedStem
	{
		FVector2D Position = FVector2D::ZeroVector;
		double HeightCm = 0.0;
		double CrownRadiusCm = 0.0;
		double SurfaceZ = 0.0;
		int32 TreeIndex = INDEX_NONE;
		int32 EntryOrdinal = INDEX_NONE;
		int32 PassIndex = INDEX_NONE;
		int32 RecordCellIndex = INDEX_NONE;
	};

	struct FCandidate
	{
		FVector2D Position = FVector2D::ZeroVector;
		double SurfaceZ = 0.0;
		double HeightCm = 0.0;
		double CrownRadiusCm = 0.0;
		double InhibitionRadiusCm = 0.0;
		double Priority = 0.0;
		int32 TreeIndex = INDEX_NONE;
		int32 EntryOrdinal = INDEX_NONE;
		int32 RecordCellIndex = INDEX_NONE;
		int32 Attempt = 0;
	};

	bool HasHigherPriority(const FCandidate& Left, const FCandidate& Right)
	{
		if (Left.Priority != Right.Priority)
		{
			return Left.Priority > Right.Priority;
		}
		if (Left.RecordCellIndex != Right.RecordCellIndex)
		{
			return Left.RecordCellIndex < Right.RecordCellIndex;
		}
		return Left.Attempt < Right.Attempt;
	}

	class FStemLookupGrid
	{
	public:
		void Initialize(const FVector2D& InOriginCm, const double InCellSizeCm, const double InExtentX, const double InExtentY)
		{
			OriginCm = InOriginCm;
			CellSizeCm = FMath::Max(1.0, InCellSizeCm);
			CellsX = FMath::Max(1, FMath::CeilToInt32(InExtentX / CellSizeCm));
			CellsY = FMath::Max(1, FMath::CeilToInt32(InExtentY / CellSizeCm));
			const int32 BucketCount = CellsX * CellsY;
			if (Buckets.Num() == BucketCount)
			{
				for (TArray<int32>& Bucket : Buckets)
				{
					Bucket.Reset();
				}
				return;
			}
			Buckets.Empty(BucketCount);
			Buckets.SetNum(BucketCount);
		}

		void Add(const FVector2D& Position, const int32 Payload)
		{
			Buckets[FindCellIndex(Position)].Add(Payload);
		}

		void GatherInRadius(const FVector2D& Centre, const double RadiusCm, TArray<int32>& OutPayloads) const
		{
			OutPayloads.Reset();
			if (RadiusCm < 0.0)
			{
				return;
			}
			const int32 Span = FMath::Max(0, FMath::CeilToInt32(RadiusCm / CellSizeCm));
			const int32 CentreX = FindAxisCell(Centre.X - OriginCm.X, CellsX);
			const int32 CentreY = FindAxisCell(Centre.Y - OriginCm.Y, CellsY);
			for (int32 CellY = FMath::Max(0, CentreY - Span); CellY <= FMath::Min(CellsY - 1, CentreY + Span); ++CellY)
			{
				for (int32 CellX = FMath::Max(0, CentreX - Span); CellX <= FMath::Min(CellsX - 1, CentreX + Span); ++CellX)
				{
					OutPayloads.Append(Buckets[CellX + CellY * CellsX]);
				}
			}
		}

	private:
		int32 FindAxisCell(const double Offset, const int32 CellCount) const
		{
			return FMath::Clamp(FMath::FloorToInt32(Offset / CellSizeCm), 0, CellCount - 1);
		}

		int32 FindCellIndex(const FVector2D& Position) const
		{
			return FindAxisCell(Position.X - OriginCm.X, CellsX)
				+ FindAxisCell(Position.Y - OriginCm.Y, CellsY) * CellsX;
		}

		FVector2D OriginCm = FVector2D::ZeroVector;
		double CellSizeCm = 1.0;
		int32 CellsX = 1;
		int32 CellsY = 1;
		TArray<TArray<int32>> Buckets;
	};

	int32 FindAxisBaseCell(const double LocalCoordinate, const double BoundsMin, const double CellSizeCm, const int32 Dimension)
	{
		return FMath::Clamp(
			FMath::FloorToInt32((LocalCoordinate - BoundsMin) / CellSizeCm), 0, Dimension - 1);
	}

	int32 FindAxisBaseCellInSpan(
		const double LocalCoordinate,
		const double BoundsMin,
		const double CellSizeCm,
		const int32 SpanMin,
		const int32 SpanMaxExclusive)
	{
		return FMath::Clamp(
			FMath::FloorToInt32((LocalCoordinate - BoundsMin) / CellSizeCm), SpanMin, SpanMaxExclusive - 1);
	}

	bool IsSurfaceUsable(
		const FArcEcotopeSiteByteFieldView* ValidSurface,
		const FArcEcotopeSiteByteFieldView* RecordValid,
		const int32 BaseX,
		const int32 BaseY)
	{
		if (ValidSurface == nullptr || RecordValid == nullptr)
		{
			return false;
		}
		const uint8* Surface = ValidSurface->TryGet(BaseX >> ValidSurface->Level, BaseY >> ValidSurface->Level);
		const uint8* Record = RecordValid->TryGet(BaseX >> RecordValid->Level, BaseY >> RecordValid->Level);
		return Surface != nullptr && *Surface != 0 && Record != nullptr && *Record != 0;
	}
}

bool FArcForestSampler::Sample(
	const FArcForestSamplerInput& Input,
	const FArcForestCancellationToken& Cancellation,
	FArcForestProgressState& Progress,
	FArcForestSamplerResult& OutResult)
{
	using namespace ArcForestSamplerPrivate;

	OutResult = FArcForestSamplerResult();
	Progress.Reset(1);
	Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Validation));

	if (!Input.SiteSource.IsValid())
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(OutResult, EArcForestRunStatus::InvalidInput, TEXT("The sampler requires an immutable site source."));
	}

	const FArcEcotopeSiteMetadata& Metadata = Input.SiteSource->Metadata;
	const int32 SetupCount = Input.SpeciesSetups.Num();
	if (!Input.VolumeId.IsValid() || Input.SettingsHash == 0 || Metadata.SiteSnapshotHash == 0
		|| SetupCount == 0 || Input.ChunkBaseCellSpan <= 0
		|| !FMath::IsFinite(Metadata.CellSizeCm) || Metadata.CellSizeCm <= 0.0f
		|| Metadata.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel)
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(OutResult, EArcForestRunStatus::InvalidInput, TEXT("The sampler input identity or site geometry is incomplete."));
	}

	const double BaseCellSizeCm = static_cast<double>(Metadata.CellSizeCm);
	const double BaseCellAreaCm2 = BaseCellSizeCm * BaseCellSizeCm;
	const int32 RecordDivisor = 1 << Metadata.RecordGridLevel;
	const double RecordCellAreaCm2 = FMath::Square(BaseCellSizeCm * RecordDivisor);
	const FIntPoint BaseDimensions = Metadata.GridDimensions;
	const FIntPoint RecordDimensions = Metadata.GetLevelDimensions(Metadata.RecordGridLevel);
	const FGuid SimulationId = UE::ArcForest::MakeSimulationId(Input.VolumeId, Input.Seed);

	TArray<UE::ArcForest::Habitat::FHabitatResponseTable> ResponsesBySetup;
	ResponsesBySetup.SetNum(SetupCount);
	for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
	{
		UE::ArcForest::Habitat::GetHabitatResponses(
			Input.SpeciesSetups[SetupIndex], ResponsesBySetup[SetupIndex].Responses);
	}

	TArray<FTreeSpecies> TreeSpecies;
	TArray<FBandEntry> MergedOrder;
	double MaximumCrownRadiusCm = 0.0;
	for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
	{
		const FArcForestFrozenSpeciesSetup& Setup = Input.SpeciesSetups[SetupIndex];
		if (!Setup.bIndividualizes)
		{
			continue;
		}
		const FArcForestSamplerFrozenBands* Bands = nullptr;
		for (const FArcForestSamplerFrozenBands& Candidate : Input.SpeciesBands)
		{
			if (Candidate.SpeciesId == Setup.SpeciesId)
			{
				Bands = &Candidate;
				break;
			}
		}
		if (Bands == nullptr || Bands->Bands.IsEmpty())
		{
			continue;
		}
		const double MaxHeightCm = static_cast<double>(Setup.MaxHeightCm);
		const double CrownRadiusPerHeight = static_cast<double>(Setup.CrownRadiusHeightRatio);
		if (MaxHeightCm <= 0.0 || CrownRadiusPerHeight <= 0.0)
		{
			continue;
		}
		if (!FMath::IsFinite(Setup.HeightDbhAllometryCoefficient) || Setup.HeightDbhAllometryCoefficient <= 0.0f
			|| !FMath::IsFinite(Setup.HeightDbhAllometryExponent) || Setup.HeightDbhAllometryExponent == 0.0f
			|| !FMath::IsFinite(Setup.BiomassAllometryCoefficient) || Setup.BiomassAllometryCoefficient <= 0.0f
			|| !FMath::IsFinite(Setup.BiomassAllometryExponent))
		{
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			return Fail(
				OutResult,
				EArcForestRunStatus::InvalidInput,
				FString::Printf(
					TEXT("Species %s carries an unusable allometry (height-diameter %.4f/%.4f, biomass %.4f/%.4f), so no stem it places could satisfy the snapshot."),
					*Setup.SpeciesId.ToString(),
					Setup.HeightDbhAllometryCoefficient,
					Setup.HeightDbhAllometryExponent,
					Setup.BiomassAllometryCoefficient,
					Setup.BiomassAllometryExponent));
		}

		const int32 TreeIndex = TreeSpecies.Num();
		FTreeSpecies& Species = TreeSpecies.AddDefaulted_GetRef();
		Species.SetupIndex = SetupIndex;
		Species.Setup = &Setup;
		Species.CrownRadiusPerHeight = CrownRadiusPerHeight;
		for (const FArcForestSamplerFrozenBand& Band : Bands->Bands)
		{
			FBandEntry& Entry = MergedOrder.AddDefaulted_GetRef();
			Entry.TreeIndex = TreeIndex;
			Entry.BandIndex = Band.BandIndex;
			Entry.MinHeightCm = Band.MinHeightCm;
			Entry.MaxHeightCm = Band.MaxHeightCm;
		}
		MaximumCrownRadiusCm = FMath::Max(MaximumCrownRadiusCm, CrownRadiusPerHeight * MaxHeightCm);
	}

	if (TreeSpecies.IsEmpty() || MergedOrder.IsEmpty())
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::InvalidInput,
			TEXT("No enabled tree species carries a usable height band, so the sampler has nothing to place."));
	}

	MergedOrder.Sort(
		[&TreeSpecies](const FBandEntry& Left, const FBandEntry& Right)
		{
			if (Left.MinHeightCm != Right.MinHeightCm)
			{
				return Left.MinHeightCm > Right.MinHeightCm;
			}
			const FGuid& LeftSpecies = TreeSpecies[Left.TreeIndex].Setup->SpeciesId;
			const FGuid& RightSpecies = TreeSpecies[Right.TreeIndex].Setup->SpeciesId;
			if (LeftSpecies != RightSpecies)
			{
				return GuidLess(LeftSpecies, RightSpecies);
			}
			return Left.BandIndex < Right.BandIndex;
		});

	TArray<int32> PassStarts;
	for (int32 EntryIndex = 0; EntryIndex < MergedOrder.Num(); ++EntryIndex)
	{
		if (EntryIndex == 0 || MergedOrder[EntryIndex].MinHeightCm != MergedOrder[EntryIndex - 1].MinHeightCm)
		{
			PassStarts.Add(EntryIndex);
		}
	}
	PassStarts.Add(MergedOrder.Num());

	TMap<uint64, int32> EntryByTarget;
	EntryByTarget.Reserve(MergedOrder.Num());
	for (int32 EntryIndex = 0; EntryIndex < MergedOrder.Num(); ++EntryIndex)
	{
		const FBandEntry& Entry = MergedOrder[EntryIndex];
		EntryByTarget.Add(
			MakeTargetKey(TreeSpecies[Entry.TreeIndex].SetupIndex, Entry.BandIndex), EntryIndex);
	}

	const int32 RecordHaloCells = 0;
	const int32 AcquireHaloCells = RecordHaloCells * RecordDivisor;

	FArcEcotopeChunkLayout Layout;
	FString LayoutError;
	if (!FArcEcotopeChunkLayout::Build(Metadata, Input.ChunkBaseCellSpan, RecordHaloCells, Layout, LayoutError))
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::InvalidInput,
			FString::Printf(TEXT("The sampler chunk layout is invalid: %s"), *LayoutError));
	}
	if (Layout.GetCoreBaseCellSpan() != Input.SiteSource->Layout.GetCoreBaseCellSpan()
		|| Layout.GetChunkGridDimensions() != Input.SiteSource->Layout.GetChunkGridDimensions())
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::InvalidInput,
			FString::Printf(
				TEXT("The volume partitions the site into %d-cell chunks while the bake stored %d-cell chunks. Bake Site again to fix the disagreement."),
				Layout.GetCoreBaseCellSpan(),
				Input.SiteSource->Layout.GetCoreBaseCellSpan()));
	}

	const int32 EntryCount = MergedOrder.Num();
	const int32 PassCount = PassStarts.Num() - 1;
	const int32 TreeCount = TreeSpecies.Num();
	const int64 RecordCellCount = static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y;
	const int64 BaseCellCount = static_cast<int64>(BaseDimensions.X) * BaseDimensions.Y;
	const int64 WeightCount = RecordCellCount * SetupCount;
	const int64 StemCount = RecordCellCount * EntryCount;
	if (RecordCellCount <= 0 || BaseCellCount <= 0 || BaseCellCount > MAX_int32
		|| WeightCount > MAX_int32 || StemCount > MAX_int32)
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::InvalidInput,
			FString::Printf(
				TEXT("A %dx%d record grid over %d species and %d bands exceeds the sampler's addressable working state; shrink the volume or the species set."),
				RecordDimensions.X, RecordDimensions.Y, SetupCount, EntryCount));
	}

	const int64 SurvivorsPerRecordCellBudget =
		static_cast<int64>(MaximumCandidatesPerRecordCell / CandidateOversample);
	const int64 VolumeSurvivorBudget = FMath::Min<int64>(
		RecordCellCount * SurvivorsPerRecordCellBudget,
		SurvivorWorkingSetBudgetBytes / static_cast<int64>(sizeof(FCandidate)));

	FArcEcotopeSiteChunkReader SiteReader;
	FString ReaderError;
	if (!SiteReader.OpenFrozen(Input.SiteSource, ReaderCacheBudgetBytes, ReaderError))
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::InvalidInput,
			FString::Printf(TEXT("The sampler could not open the frozen site source: %s"), *ReaderError));
	}

	Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::CanopyAndHabitat));

	TArray<bool> SpeciesEverViable;
	SpeciesEverViable.Init(false, SetupCount);
	TArray<bool> SpeciesUnsolved;
	SpeciesUnsolved.Init(false, SetupCount);
	int64 CoreRecordCellCount = 0;
	UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsRejections Rejections;
	TArray<double> ExpectedStemsByEntry;
	ExpectedStemsByEntry.Init(0.0, EntryCount);
	TArray<UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsCell> EmittingCells;
	TArray<int32> UsableBaseCells;
	TArray<int32> CellUsableBaseCells;

	TArray<double> RecordWeights;
	RecordWeights.SetNumZeroed(static_cast<int32>(WeightCount));
	TArray<uint8> RecordUsable;
	RecordUsable.SetNumZeroed(static_cast<int32>(RecordCellCount));
	TArray<float> RecordStems;
	RecordStems.SetNumZeroed(static_cast<int32>(StemCount));
	TArray<float> Occupancy;
	Occupancy.SetNumZeroed(static_cast<int32>(BaseCellCount));

	TArray<FArcForestIndividualRecord> Records;
	TArray<double> SolveWeights;
	SolveWeights.SetNumZeroed(SetupCount);
	TArray<UE::ArcForest::Sampler::FArcForestSamplerBandTarget> Targets;
	TArray<int32> UnsolvedSpeciesIndices;
	TArray<FPlacedStem> PlacedStems;
	TArray<FCandidate> Survivors;
	TArray<int32> Neighbours;
	TArray<double> EntryScores;
	EntryScores.SetNumZeroed(EntryCount);

	const FVector2D VolumeOriginCm(Metadata.LocalBounds.Min.X, Metadata.LocalBounds.Min.Y);
	const double LookupCellSizeCm = FMath::Max(BaseCellSizeCm * RecordDivisor, MaximumCrownRadiusCm);
	FStemLookupGrid PriorGrid;
	FStemLookupGrid SurvivorGrid;
	PriorGrid.Initialize(
		VolumeOriginCm,
		LookupCellSizeCm,
		BaseDimensions.X * BaseCellSizeCm,
		BaseDimensions.Y * BaseCellSizeCm);

	for (int32 ChunkIndex = 0; ChunkIndex < Layout.GetChunkCount(); ++ChunkIndex)
	{
		if (Cancellation.IsCancellationRequested())
		{
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Cancelled));
			return Fail(OutResult, EArcForestRunStatus::Cancelled, TEXT("The sampler was cancelled."));
		}

		FArcEcotopeChunkRect Chunk;
		FArcEcotopeSiteChunkLease Lease;
		if (!Layout.GetChunk(ChunkIndex, Chunk)
			|| !SiteReader.AcquireFields(Chunk.Id, AcquireHaloCells, HabitatFieldMask, Lease, ReaderError))
		{
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			return Fail(
				OutResult,
				EArcForestRunStatus::Failed,
				FString::Printf(TEXT("The sampler could not read site chunk %d: %s"), ChunkIndex, *ReaderError));
		}

		const FArcEcotopeSiteChunkView& View = Lease.GetView();
		const FArcEcotopeSiteByteFieldView* ValidSurface = View.FindByteField(EArcEcotopeSiteField::ValidSurface);
		const FArcEcotopeSiteByteFieldView* RecordValid = View.FindByteField(EArcEcotopeSiteField::RecordValid);
		if (ValidSurface == nullptr || RecordValid == nullptr)
		{
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			return Fail(
				OutResult,
				EArcForestRunStatus::Failed,
				FString::Printf(TEXT("Site chunk %d is missing surface or validity fields."), ChunkIndex));
		}

		for (int32 RecordY = Chunk.RecordCoreRect.Min.Y; RecordY < Chunk.RecordCoreRect.Max.Y; ++RecordY)
		{
			for (int32 RecordX = Chunk.RecordCoreRect.Min.X; RecordX < Chunk.RecordCoreRect.Max.X; ++RecordX)
			{
				const int32 RecordCellIndex = RecordX + RecordY * RecordDimensions.X;
				const int32 BaseMinX = RecordX * RecordDivisor;
				const int32 BaseMinY = RecordY * RecordDivisor;
				const int32 BaseMaxX = FMath::Min(BaseMinX + RecordDivisor, BaseDimensions.X);
				const int32 BaseMaxY = FMath::Min(BaseMinY + RecordDivisor, BaseDimensions.Y);

				for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
				{
					SolveWeights[SetupIndex] = 0.0;
				}
				CellUsableBaseCells.Reset();
				for (int32 BaseY = BaseMinY; BaseY < BaseMaxY; ++BaseY)
				{
					for (int32 BaseX = BaseMinX; BaseX < BaseMaxX; ++BaseX)
					{
						if (!IsSurfaceUsable(ValidSurface, RecordValid, BaseX, BaseY))
						{
							continue;
						}
						UE::ArcForest::Habitat::FHabitatSiteSample Sample;
						if (!UE::ArcForest::Habitat::TryReadHabitatSiteSample(View, BaseX, BaseY, Sample))
						{
							Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
							return Fail(
								OutResult,
								EArcForestRunStatus::Failed,
								FString::Printf(
									TEXT("Site chunk %d has incomplete habitat fields at base cell (%d,%d)."),
									ChunkIndex, BaseX, BaseY));
						}
						for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
						{
							UE::ArcForest::Habitat::FHabitatStaticFactor Factor;
							UE::ArcForest::Habitat::BuildStaticHabitatFactor(
								Sample,
								ResponsesBySetup[SetupIndex],
								Input.SpeciesSetups[SetupIndex],
								Factor);
							if (Factor.bHardZero || !FMath::IsFinite(Factor.SiteHabitat))
							{
								continue;
							}
							SpeciesEverViable[SetupIndex] = true;
							SolveWeights[SetupIndex] += static_cast<double>(Factor.SiteHabitat);
						}
						CellUsableBaseCells.Add(BaseX + BaseY * BaseDimensions.X);
					}
				}

				if (CellUsableBaseCells.IsEmpty())
				{
					continue;
				}
				RecordUsable[RecordCellIndex] = 1;
				++CoreRecordCellCount;
				for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
				{
					SolveWeights[SetupIndex] /= static_cast<double>(CellUsableBaseCells.Num());
					RecordWeights[RecordCellIndex * SetupCount + SetupIndex] = SolveWeights[SetupIndex];
				}

				UE::ArcForest::Sampler::SolveBandTargets(
					Input, SolveWeights, RecordCellAreaCm2, Targets, UnsolvedSpeciesIndices);
				for (const int32 UnsolvedIndex : UnsolvedSpeciesIndices)
				{
					if (SolveWeights.IsValidIndex(UnsolvedIndex))
					{
						SpeciesUnsolved[UnsolvedIndex] = true;
					}
				}
				bool bEmitting = false;
				for (const UE::ArcForest::Sampler::FArcForestSamplerBandTarget& Target : Targets)
				{
					const int32* EntryIndex =
						EntryByTarget.Find(MakeTargetKey(Target.SpeciesIndex, Target.BandIndex));
					if (EntryIndex == nullptr || !FMath::IsFinite(Target.StemsPerCell) || Target.StemsPerCell <= 0.0)
					{
						continue;
					}
					RecordStems[RecordCellIndex * EntryCount + *EntryIndex] =
						static_cast<float>(Target.StemsPerCell);
					ExpectedStemsByEntry[*EntryIndex] += Target.StemsPerCell;
					bEmitting = true;
				}

				if (bEmitting)
				{
					UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsCell& EmittingCell =
						EmittingCells.AddDefaulted_GetRef();
					EmittingCell.RecordCellIndex = RecordCellIndex;
					EmittingCell.UsableStart = UsableBaseCells.Num();
					EmittingCell.UsableCount = CellUsableBaseCells.Num();
					UsableBaseCells.Append(CellUsableBaseCells);
				}
			}
		}
	}

	// Every band pass closes over the whole volume before the next one opens, because a chunk-outer loop would
	// need a halo growing by one inhibition reach per pass to see the occupancy and seed rain its neighbour saw.
	for (int32 Pass = 0; Pass < PassCount; ++Pass)
	{
		const int32 PassEntryStart = PassStarts[Pass];
		const int32 PassEntryEnd = PassStarts[Pass + 1];
		Survivors.Reset();

		for (int32 ChunkIndex = 0; ChunkIndex < Layout.GetChunkCount(); ++ChunkIndex)
		{
			if (Cancellation.IsCancellationRequested())
			{
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Cancelled));
				return Fail(OutResult, EArcForestRunStatus::Cancelled, TEXT("The sampler was cancelled."));
			}

			FArcEcotopeChunkRect Chunk;
			FArcEcotopeSiteChunkLease Lease;
			if (!Layout.GetChunk(ChunkIndex, Chunk)
				|| !SiteReader.AcquireFields(Chunk.Id, AcquireHaloCells, PlacementFieldMask, Lease, ReaderError))
			{
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				return Fail(
					OutResult,
					EArcForestRunStatus::Failed,
					FString::Printf(TEXT("The sampler could not read site chunk %d: %s"), ChunkIndex, *ReaderError));
			}

			const FArcEcotopeSiteChunkView& View = Lease.GetView();
			const FArcEcotopeSiteFloatFieldView* SurfaceZ = View.FindFloatField(EArcEcotopeSiteField::LocalSurfaceZ);
			const FArcEcotopeSiteByteFieldView* ValidSurface = View.FindByteField(EArcEcotopeSiteField::ValidSurface);
			const FArcEcotopeSiteByteFieldView* RecordValid = View.FindByteField(EArcEcotopeSiteField::RecordValid);
			if (SurfaceZ == nullptr || ValidSurface == nullptr || RecordValid == nullptr)
			{
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				return Fail(
					OutResult,
					EArcForestRunStatus::Failed,
					FString::Printf(TEXT("Site chunk %d is missing surface or validity fields."), ChunkIndex));
			}

			for (int32 RecordY = Chunk.RecordCoreRect.Min.Y; RecordY < Chunk.RecordCoreRect.Max.Y; ++RecordY)
			{
				for (int32 RecordX = Chunk.RecordCoreRect.Min.X; RecordX < Chunk.RecordCoreRect.Max.X; ++RecordX)
				{
					// The record cell index is taken in volume coordinates and the chunk id never enters the key,
					// so the same cell reconstructs identically however the site is partitioned.
					const int32 RecordCellIndex = RecordX + RecordY * RecordDimensions.X;
					if (RecordUsable[RecordCellIndex] == 0)
					{
						continue;
					}

					double TotalStems = 0.0;
					for (int32 EntryIndex = PassEntryStart; EntryIndex < PassEntryEnd; ++EntryIndex)
					{
						TotalStems += static_cast<double>(RecordStems[RecordCellIndex * EntryCount + EntryIndex]);
					}
					if (TotalStems <= 0.0)
					{
						continue;
					}
					const double DesiredCandidates = FMath::CeilToDouble(CandidateOversample * TotalStems);
					if (!FMath::IsFinite(DesiredCandidates) || DesiredCandidates > MaximumCandidatesPerRecordCell)
					{
						Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
						return Fail(
							OutResult,
							EArcForestRunStatus::Failed,
							FString::Printf(
								TEXT("Band pass %d asks for %.0f candidates in one record cell; raise the species' crown radius or lower MaximumCrownOccupancy."),
								Pass, DesiredCandidates));
					}
					const int32 CandidateCount = static_cast<int32>(DesiredCandidates);

					FVector2D CellMinLocal = FVector2D::ZeroVector;
					FVector2D CellMaxLocal = FVector2D::ZeroVector;
					if (!Metadata.TryGetLevelCellRect(RecordCellIndex, Metadata.RecordGridLevel, CellMinLocal, CellMaxLocal))
					{
						++Rejections.SkippedRecordCells;
						continue;
					}
					Rejections.GeneratedCandidates += CandidateCount;
					const int32 CellBaseMinX = RecordX * RecordDivisor;
					const int32 CellBaseMinY = RecordY * RecordDivisor;
					const int32 CellBaseMaxX = FMath::Min(CellBaseMinX + RecordDivisor, BaseDimensions.X);
					const int32 CellBaseMaxY = FMath::Min(CellBaseMinY + RecordDivisor, BaseDimensions.Y);

					for (int32 Attempt = 0; Attempt < CandidateCount; ++Attempt)
					{
						const FVector2D Position(
							FMath::Lerp(CellMinLocal.X, CellMaxLocal.X, UE::ArcForest::KeyToUnitOpen(
								UE::ArcForest::Sampler::MakeSamplerKey(
									SimulationId, Input.Seed,
									UE::ArcForest::Sampler::SamplerCandidatePosition,
									FGuid(), RecordCellIndex,
									MakeFieldOrdinal(Pass, Attempt, ESamplerKeySlot::PositionX)))),
							FMath::Lerp(CellMinLocal.Y, CellMaxLocal.Y, UE::ArcForest::KeyToUnitOpen(
								UE::ArcForest::Sampler::MakeSamplerKey(
									SimulationId, Input.Seed,
									UE::ArcForest::Sampler::SamplerCandidatePosition,
									FGuid(), RecordCellIndex,
									MakeFieldOrdinal(Pass, Attempt, ESamplerKeySlot::PositionY)))));

						const int32 BaseX = FindAxisBaseCellInSpan(
							Position.X, Metadata.LocalBounds.Min.X, BaseCellSizeCm, CellBaseMinX, CellBaseMaxX);
						const int32 BaseY = FindAxisBaseCellInSpan(
							Position.Y, Metadata.LocalBounds.Min.Y, BaseCellSizeCm, CellBaseMinY, CellBaseMaxY);
						if (!IsSurfaceUsable(ValidSurface, RecordValid, BaseX, BaseY))
						{
							++Rejections.OffUsableSurface;
							continue;
						}
						const float* SurfaceValue = SurfaceZ->TryGet(BaseX, BaseY);
						if (SurfaceValue == nullptr || !FMath::IsFinite(*SurfaceValue))
						{
							++Rejections.MissingSurfaceZ;
							continue;
						}
						const int32 BaseCellIndex = BaseX + BaseY * BaseDimensions.X;

						double ScoreTotal = 0.0;
						for (int32 EntryIndex = PassEntryStart; EntryIndex < PassEntryEnd; ++EntryIndex)
						{
							EntryScores[EntryIndex] = 0.0;
							if (RecordStems[RecordCellIndex * EntryCount + EntryIndex] <= 0.0f)
							{
								continue;
							}
							const FBandEntry& Entry = MergedOrder[EntryIndex];
							const FTreeSpecies& Species = TreeSpecies[Entry.TreeIndex];
							const double SpeciesWeight =
								RecordWeights[RecordCellIndex * SetupCount + Species.SetupIndex];
							if (SpeciesWeight <= 0.0)
							{
								continue;
							}
							EntryScores[EntryIndex] = SpeciesWeight;
							ScoreTotal += SpeciesWeight;
						}
						if (!(ScoreTotal > 0.0))
						{
							++Rejections.NoBandWithHabitat;
							continue;
						}

						const double SpeciesDraw = ScoreTotal * UE::ArcForest::KeyToUnitOpen(
							UE::ArcForest::Sampler::MakeSamplerKey(
								SimulationId, Input.Seed,
								UE::ArcForest::Sampler::SamplerSpeciesChoice,
								FGuid(), RecordCellIndex,
								MakeFieldOrdinal(Pass, Attempt, ESamplerKeySlot::SpeciesChoice)));
						int32 ChosenEntry = INDEX_NONE;
						double Cumulative = 0.0;
						for (int32 EntryIndex = PassEntryStart; EntryIndex < PassEntryEnd; ++EntryIndex)
						{
							if (EntryScores[EntryIndex] <= 0.0)
							{
								continue;
							}
							Cumulative += EntryScores[EntryIndex];
							ChosenEntry = EntryIndex;
							if (SpeciesDraw <= Cumulative)
							{
								break;
							}
						}
						if (ChosenEntry == INDEX_NONE)
						{
							++Rejections.NoBandDrawn;
							continue;
						}

						const FBandEntry& Entry = MergedOrder[ChosenEntry];
						const FTreeSpecies& Species = TreeSpecies[Entry.TreeIndex];
						const FArcForestFrozenSpeciesSetup& Setup = *Species.Setup;
						const double HeightCm = FMath::Lerp(
							static_cast<double>(Entry.MinHeightCm),
							static_cast<double>(Entry.MaxHeightCm),
							UE::ArcForest::KeyToUnitOpen(UE::ArcForest::Sampler::MakeSamplerKey(
								SimulationId, Input.Seed,
								UE::ArcForest::Sampler::SamplerHeightJitter,
								Setup.SpeciesId, RecordCellIndex,
								MakeFieldOrdinal(Pass, Attempt, ESamplerKeySlot::HeightJitter))));

						const double Light = UE::ArcForest::Habitat::EvaluateResponse(
							*ResponsesBySetup[Species.SetupIndex].Responses[0],
							static_cast<float>(FMath::Exp(
								-Input.UnderstoryShade * static_cast<double>(Occupancy[BaseCellIndex]))));

						// The raw kernel sum is unbounded, so three close seed trees would make acceptance
						// unconditional; the exponential maps it into [eps, 1] where it can still gate.
						double Dispersal = 1.0;
						const double SigmaCm = static_cast<double>(Setup.MeanDispersalDistanceCm);
						if (static_cast<double>(Entry.MaxHeightCm)
								< static_cast<double>(Setup.IndividualizationHeightCm) * IndividualizationBandFactor
							&& SigmaCm > 0.0)
						{
							const double CutoffCm = DispersalKernelCutoffSigmas * SigmaCm;
							PriorGrid.GatherInRadius(Position, CutoffCm, Neighbours);
							double KernelSum = 0.0;
							for (const int32 StemIndex : Neighbours)
							{
								const FPlacedStem& Stem = PlacedStems[StemIndex];
								if (Stem.TreeIndex != Entry.TreeIndex)
								{
									continue;
								}
								const double DistanceSquared = FVector2D::DistSquared(Stem.Position, Position);
								if (DistanceSquared <= CutoffCm * CutoffCm)
								{
									KernelSum += FMath::Exp(-DistanceSquared / (2.0 * SigmaCm * SigmaCm));
								}
							}
							Dispersal = DispersalFloor
								+ (1.0 - DispersalFloor) * (1.0 - FMath::Exp(-KernelSum));
						}

						const double SpeciesWeight =
							RecordWeights[RecordCellIndex * SetupCount + Species.SetupIndex];
						const double AcceptanceDraw = UE::ArcForest::KeyToUnitOpen(UE::ArcForest::Sampler::MakeSamplerKey(
							SimulationId, Input.Seed,
							UE::ArcForest::Sampler::SamplerAcceptance,
							Setup.SpeciesId, RecordCellIndex,
							MakeFieldOrdinal(Pass, Attempt, ESamplerKeySlot::Acceptance)));
						if (!(Light * Dispersal * SpeciesWeight > AcceptanceDraw))
						{
							++Rejections.AcceptanceRejected;
							continue;
						}

						FCandidate& Survivor = Survivors.AddDefaulted_GetRef();
						Survivor.Position = Position;
						Survivor.SurfaceZ = static_cast<double>(*SurfaceValue);
						Survivor.HeightCm = HeightCm;
						Survivor.CrownRadiusCm = Species.CrownRadiusPerHeight * HeightCm;
						Survivor.InhibitionRadiusCm =
							static_cast<double>(Input.Settings.InhibitionStrength) * Survivor.CrownRadiusCm;
						Survivor.Priority = UE::ArcForest::KeyToUnitOpen(UE::ArcForest::Sampler::MakeSamplerKey(
							SimulationId, Input.Seed,
							UE::ArcForest::Sampler::SamplerPriority,
							Setup.SpeciesId, RecordCellIndex,
							MakeFieldOrdinal(Pass, Attempt, ESamplerKeySlot::Priority)));
						Survivor.TreeIndex = Entry.TreeIndex;
						Survivor.EntryOrdinal = ChosenEntry;
						Survivor.RecordCellIndex = RecordCellIndex;
						Survivor.Attempt = Attempt;
					}

					if (Survivors.Num() > VolumeSurvivorBudget)
					{
						Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
						return Fail(
							OutResult,
							EArcForestRunStatus::Failed,
							FString::Printf(
								TEXT("Band pass %d has kept %d candidates across the volume, past the %lld the sampler can hold at once; raise the species' crown radius or lower MaximumCrownOccupancy."),
								Pass, Survivors.Num(), VolumeSurvivorBudget));
					}
				}
			}
		}

		if (Survivors.IsEmpty())
		{
			continue;
		}

		SurvivorGrid.Initialize(
			VolumeOriginCm,
			LookupCellSizeCm,
			BaseDimensions.X * BaseCellSizeCm,
			BaseDimensions.Y * BaseCellSizeCm);
		for (int32 SurvivorIndex = 0; SurvivorIndex < Survivors.Num(); ++SurvivorIndex)
		{
			SurvivorGrid.Add(Survivors[SurvivorIndex].Position, SurvivorIndex);
		}

		const int32 PassFirstStem = PlacedStems.Num();
		for (int32 SurvivorIndex = 0; SurvivorIndex < Survivors.Num(); ++SurvivorIndex)
		{
			const FCandidate& Survivor = Survivors[SurvivorIndex];
			SurvivorGrid.GatherInRadius(Survivor.Position, Survivor.InhibitionRadiusCm, Neighbours);
			bool bInhibited = false;
			for (const int32 OtherIndex : Neighbours)
			{
				if (OtherIndex == SurvivorIndex)
				{
					continue;
				}
				const FCandidate& Other = Survivors[OtherIndex];
				if (!HasHigherPriority(Other, Survivor))
				{
					continue;
				}
				if (FVector2D::DistSquared(Other.Position, Survivor.Position)
					<= Survivor.InhibitionRadiusCm * Survivor.InhibitionRadiusCm)
				{
					bInhibited = true;
					break;
				}
			}
			if (bInhibited)
			{
				++Rejections.Inhibited;
				continue;
			}

			FPlacedStem& Stem = PlacedStems.AddDefaulted_GetRef();
			Stem.Position = Survivor.Position;
			Stem.HeightCm = Survivor.HeightCm;
			Stem.CrownRadiusCm = Survivor.CrownRadiusCm;
			Stem.SurfaceZ = Survivor.SurfaceZ;
			Stem.TreeIndex = Survivor.TreeIndex;
			Stem.EntryOrdinal = Survivor.EntryOrdinal;
			Stem.PassIndex = Pass;
			Stem.RecordCellIndex = Survivor.RecordCellIndex;
		}

		for (int32 StemIndex = PassFirstStem; StemIndex < PlacedStems.Num(); ++StemIndex)
		{
			const FPlacedStem& Stem = PlacedStems[StemIndex];
			PriorGrid.Add(Stem.Position, StemIndex);

			const double CrownWeight = static_cast<double>(TreeSpecies[Stem.TreeIndex].Setup->CrownOccupancyWeight);
			if (CrownWeight <= 0.0 || Stem.CrownRadiusCm <= 0.0)
			{
				continue;
			}
			const int32 StemBaseX = FindAxisBaseCell(
				Stem.Position.X, Metadata.LocalBounds.Min.X, BaseCellSizeCm, BaseDimensions.X);
			const int32 StemBaseY = FindAxisBaseCell(
				Stem.Position.Y, Metadata.LocalBounds.Min.Y, BaseCellSizeCm, BaseDimensions.Y);
			if (2.0 * Stem.CrownRadiusCm <= BaseCellSizeCm)
			{
				Occupancy[StemBaseX + StemBaseY * BaseDimensions.X] += static_cast<float>(
					CrownWeight * UE_DOUBLE_PI * Stem.CrownRadiusCm * Stem.CrownRadiusCm / BaseCellAreaCm2);
				continue;
			}

			const int32 SpanCells = FMath::CeilToInt32(Stem.CrownRadiusCm / BaseCellSizeCm) + 1;
			const int32 SplatMinX = FMath::Max(0, StemBaseX - SpanCells);
			const int32 SplatMaxX = FMath::Min(BaseDimensions.X - 1, StemBaseX + SpanCells);
			const int32 SplatMinY = FMath::Max(0, StemBaseY - SpanCells);
			const int32 SplatMaxY = FMath::Min(BaseDimensions.Y - 1, StemBaseY + SpanCells);
			for (int32 SplatY = SplatMinY; SplatY <= SplatMaxY; ++SplatY)
			{
				const double CellMinY =
					Metadata.LocalBounds.Min.Y + SplatY * BaseCellSizeCm - Stem.Position.Y;
				for (int32 SplatX = SplatMinX; SplatX <= SplatMaxX; ++SplatX)
				{
					const double CellMinX =
						Metadata.LocalBounds.Min.X + SplatX * BaseCellSizeCm - Stem.Position.X;
					const double OverlapCm2 = DiskRectangleOverlapArea(
						Stem.CrownRadiusCm,
						CellMinX,
						CellMinY,
						CellMinX + BaseCellSizeCm,
						CellMinY + BaseCellSizeCm);
					if (OverlapCm2 <= 0.0)
					{
						continue;
					}
					Occupancy[SplatX + SplatY * BaseDimensions.X] +=
						static_cast<float>(CrownWeight * OverlapCm2 / BaseCellAreaCm2);
				}
			}
		}
	}

	TArray<UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsBand> DiagnosticsBands;
	DiagnosticsBands.Reserve(EntryCount);
	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		const FBandEntry& Entry = MergedOrder[EntryIndex];
		UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsBand& Band = DiagnosticsBands.AddDefaulted_GetRef();
		Band.SpeciesId = TreeSpecies[Entry.TreeIndex].Setup->SpeciesId;
		Band.BandIndex = Entry.BandIndex;
		Band.MinHeightCm = Entry.MinHeightCm;
		Band.MaxHeightCm = Entry.MaxHeightCm;
		Band.ExpectedStems = ExpectedStemsByEntry[EntryIndex];
	}

	TArray<UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsStem> DiagnosticsStems;
	DiagnosticsStems.Reserve(PlacedStems.Num());
	for (const FPlacedStem& Stem : PlacedStems)
	{
		UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsStem& DiagnosticsStem =
			DiagnosticsStems.AddDefaulted_GetRef();
		DiagnosticsStem.PositionCm = Stem.Position;
		DiagnosticsStem.CrownRadiusCm = Stem.CrownRadiusCm;
		DiagnosticsStem.EntryOrdinal = Stem.EntryOrdinal;
		DiagnosticsStem.PassIndex = Stem.PassIndex;
		DiagnosticsStem.RecordCellIndex = Stem.RecordCellIndex;
	}

	UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsContext DiagnosticsContext;
	DiagnosticsContext.SimulationId = SimulationId;
	DiagnosticsContext.Seed = Input.Seed;
	DiagnosticsContext.MaximumCrownOccupancy = Input.MaximumCrownOccupancy;
	DiagnosticsContext.UnevenAgedness = static_cast<double>(Input.Settings.UnevenAgedness);
	DiagnosticsContext.BaseOriginCm = VolumeOriginCm;
	DiagnosticsContext.BaseDimensions = BaseDimensions;
	DiagnosticsContext.BaseCellSizeCm = BaseCellSizeCm;
	DiagnosticsContext.Rejections = Rejections;
	DiagnosticsContext.DescendingBands = DiagnosticsBands;
	DiagnosticsContext.Stems = DiagnosticsStems;
	DiagnosticsContext.EmittingCells = EmittingCells;
	DiagnosticsContext.UsableBaseCells = UsableBaseCells;
	UE::ArcForest::Sampler::ReportSamplerDiagnostics(DiagnosticsContext);

	Records.Reserve(PlacedStems.Num());
	for (const FPlacedStem& Stem : PlacedStems)
	{
		const FArcForestFrozenSpeciesSetup& Setup = *TreeSpecies[Stem.TreeIndex].Setup;
		const double DbhCm = FMath::Pow(
			Stem.HeightCm / static_cast<double>(Setup.HeightDbhAllometryCoefficient),
			1.0 / static_cast<double>(Setup.HeightDbhAllometryExponent));
		const double BiomassKg = static_cast<double>(Setup.BiomassAllometryCoefficient)
			* FMath::Pow(DbhCm, static_cast<double>(Setup.BiomassAllometryExponent));
		if (!FMath::IsFinite(DbhCm) || DbhCm < 0.0
			|| !FMath::IsFinite(BiomassKg) || BiomassKg <= 0.0 || BiomassKg > MAX_flt)
		{
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			return Fail(
				OutResult,
				EArcForestRunStatus::Failed,
				FString::Printf(
					TEXT("Species %s produces non-positive allometry at %.1f cm, which the snapshot rejects."),
					*Setup.SpeciesId.ToString(), Stem.HeightCm));
		}

		FArcForestIndividualRecord& Record = Records.AddDefaulted_GetRef();
		Record.SpeciesId = Setup.SpeciesId;
		Record.LocalPosition = FVector(Stem.Position.X, Stem.Position.Y, Stem.SurfaceZ);
		Record.AgeYears = FMath::RoundToInt32(
			Setup.MaxAgeYears * FMath::Clamp(Stem.HeightCm / static_cast<double>(Setup.MaxHeightCm), 0.0, 1.0));
		Record.HeightCm = static_cast<float>(Stem.HeightCm);
		Record.DbhCm = static_cast<float>(DbhCm);
		Record.CrownRadiusCm = static_cast<float>(Stem.CrownRadiusCm);
		Record.BiomassKg = static_cast<float>(BiomassKg);
		Record.Vigor = 1.0f;
		Record.LifeStatus = EArcForestLifeStatus::Alive;
		Record.NextShedYear = MAX_int32;
	}

	for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
	{
		UE_CLOGF(
			SpeciesUnsolved[SetupIndex],
			LogArcEcotope,
			Warning,"ArcForest sampler: species %ls has no solvable size-class distribution - its crown geometry is non-positive or its authored bands cover under 5%% of the height distribution - so it was placed nowhere.",
			*Input.SpeciesSetups[SetupIndex].SpeciesId.ToString());
	}

	if (Records.IsEmpty())
	{
		int32 ViableSpeciesCount = 0;
		for (int32 SetupIndex = 0; SetupIndex < SetupCount; ++SetupIndex)
		{
			ViableSpeciesCount += SpeciesEverViable[SetupIndex] ? 1 : 0;
		}
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::Failed,
			FString::Printf(
				TEXT("The sampler placed no stem over %lld usable record cells; %d of %d species cleared their hard habitat thresholds anywhere on the site."),
				CoreRecordCellCount, ViableSpeciesCount, SetupCount));
	}

	Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Canonicalization));

	// Ids are stamped once every pass is closed: numbering restarted inside a pass would repeat across passes,
	// and Validate rejects duplicate identifiers that canonical sorting cannot remove.
	uint64 NextId = 1;
	for (FArcForestIndividualRecord& Record : Records)
	{
		Record.Id = NextId++;
	}

	FArcForestWorldSnapshot Snapshot;
	Snapshot.SchemaVersion = FArcForestWorldSnapshot::GetCurrentSchemaVersion();
	Snapshot.SimulationId = SimulationId;
	Snapshot.CurrentYear = 0;
	Snapshot.Seed = Input.Seed;
	Snapshot.SiteSnapshotHash = Metadata.SiteSnapshotHash;
	Snapshot.SettingsHash = Input.SettingsHash;
	Snapshot.RegionalRecruitmentConfig = FArcForestRegionalRecruitmentConfig();
	Snapshot.StateRevision = 1;
	Snapshot.Individuals = MoveTemp(Records);

	Snapshot.SpeciesEcologyHashes.Reserve(SetupCount);
	for (const FArcForestFrozenSpeciesSetup& Setup : Input.SpeciesSetups)
	{
		FArcForestSpeciesEcologyHash& Entry = Snapshot.SpeciesEcologyHashes.AddDefaulted_GetRef();
		Entry.SpeciesId = Setup.SpeciesId;
		Entry.EcologyHash = Setup.EcologyHash;
	}
	Snapshot.SpeciesEcologyHashes.Sort(
		[](const FArcForestSpeciesEcologyHash& Left, const FArcForestSpeciesEcologyHash& Right)
		{
			return GuidLess(Left.SpeciesId, Right.SpeciesId);
		});
	Snapshot.CanonicalizeAndRefreshChecksum();

	FText SnapshotError;
	if (Snapshot.Validate(SnapshotError) != EArcForestSimulationStateStatus::Valid)
	{
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		return Fail(
			OutResult,
			EArcForestRunStatus::Failed,
			FString::Printf(TEXT("The sampled snapshot is not valid: %s"), *SnapshotError.ToString()));
	}

	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest sampler | stems=%d recordCells=%lld species=%d bands=%d passes=%d chunks=%d",
		Snapshot.Individuals.Num(),
		CoreRecordCellCount,
		TreeCount,
		EntryCount,
		PassCount,
		Layout.GetChunkCount());

	OutResult.Status = EArcForestRunStatus::Succeeded;
	OutResult.Error.Reset();
	OutResult.Snapshot = MoveTemp(Snapshot);
	Progress.CompletedYears.Store(1);
	Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Complete));
	return true;
}
