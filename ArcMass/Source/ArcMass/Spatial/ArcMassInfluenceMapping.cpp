// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassInfluenceMapping.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassWorldUtils.h"

TAutoConsoleVariable<bool> CVarArcDebugDrawInfluence(
	TEXT("arc.mass.DebugDrawInfluence"),
	false,
	TEXT("Toggles debug drawing for influence mapping grids (0 = off, 1 = on)"));

// ---------------------------------------------------------------------------
// FArcInfluenceCell
// ---------------------------------------------------------------------------

void FArcInfluenceCell::Init(int32 NumChannels)
{
	if (ChannelEntries.Num() != NumChannels)
	{
		ChannelEntries.SetNum(NumChannels);
	}
}

float FArcInfluenceCell::GetTotalInfluence(int32 Channel) const
{
	check(Channel >= 0 && Channel < ChannelEntries.Num());
	float Total = 0.f;
	for (const FArcInfluenceEntry& Entry : ChannelEntries[Channel])
	{
		Total += Entry.Strength;
	}
	return Total;
}

// ---------------------------------------------------------------------------
// FArcInfluenceGrid
// ---------------------------------------------------------------------------

FIntVector FArcInfluenceGrid::WorldToGrid(const FVector& WorldPos) const
{
	const float InvCellSize = 1.f / Settings.CellSize;
	return FIntVector(
		FMath::FloorToInt(WorldPos.X * InvCellSize),
		FMath::FloorToInt(WorldPos.Y * InvCellSize),
		Settings.bIs2D ? 0 : FMath::FloorToInt(WorldPos.Z * InvCellSize)
	);
}

FArcInfluenceCell& FArcInfluenceGrid::FindOrAddCell(const FIntVector& GridCoords)
{
	FArcInfluenceCell& Cell = Cells.FindOrAdd(GridCoords);
	Cell.Init(Settings.NumChannels);
	return Cell;
}

void FArcInfluenceGrid::AddInfluence(const FIntVector& GridCoords, int32 Channel, float Strength, FMassEntityHandle Source)
{
	check(Channel >= 0 && Channel < Settings.NumChannels);
	FArcInfluenceCell& Cell = FindOrAddCell(GridCoords);
	TArray<FArcInfluenceEntry>& Entries = Cell.ChannelEntries[Channel];

	for (FArcInfluenceEntry& Entry : Entries)
	{
		if (Entry.Source == Source)
		{
			Entry.Strength = Strength;
			return;
		}
	}

	Entries.Add({ Strength, Source });
}

void FArcInfluenceGrid::AddInfluenceBatch(TConstArrayView<FArcPendingInfluence> Batch)
{
	for (const FArcPendingInfluence& Pending : Batch)
	{
		check(Pending.Channel >= 0 && Pending.Channel < Settings.NumChannels);
		FArcInfluenceCell& Cell = FindOrAddCell(Pending.GridCoords);
		TArray<FArcInfluenceEntry>& Entries = Cell.ChannelEntries[Pending.Channel];

		bool bFound = false;
		for (FArcInfluenceEntry& Entry : Entries)
		{
			if (Entry.Source == Pending.Source)
			{
				Entry.Strength += Pending.Strength;
				Entry.Strength = FMath::Min(Entry.Strength, 1.0f);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Entries.Add({ Pending.Strength, Pending.Source });
		}
	}
}

float FArcInfluenceGrid::QueryInfluence(const FVector& WorldPos, int32 Channel) const
{
	check(Channel >= 0 && Channel < Settings.NumChannels);
	const FIntVector GridCoords = WorldToGrid(WorldPos);
	if (const FArcInfluenceCell* Cell = Cells.Find(GridCoords))
	{
		return Cell->GetTotalInfluence(Channel);
	}
	return 0.f;
}

float FArcInfluenceGrid::QueryInfluenceInRadius(const FVector& Center, float Radius, int32 Channel) const
{
	check(Channel >= 0 && Channel < Settings.NumChannels);
	const FVector Extent(Radius);
	const FIntVector MinGrid = WorldToGrid(Center - Extent);
	const FIntVector MaxGrid = WorldToGrid(Center + Extent);

	const float RadiusSq = Radius * Radius;
	const float HalfCell = Settings.CellSize * 0.5f;
	float Total = 0.f;

	for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
		{
			const int32 MinZ = Settings.bIs2D ? 0 : MinGrid.Z;
			const int32 MaxZ = Settings.bIs2D ? 0 : MaxGrid.Z;

			for (int32 Z = MinZ; Z <= MaxZ; Z++)
			{
				const FIntVector GridCoords(X, Y, Z);

				FVector CellCenter(
					X * Settings.CellSize + HalfCell,
					Y * Settings.CellSize + HalfCell,
					Settings.bIs2D ? Center.Z : (Z * Settings.CellSize + HalfCell)
				);

				if (FVector::DistSquared(Center, CellCenter) > RadiusSq)
				{
					continue;
				}

				if (const FArcInfluenceCell* Cell = Cells.Find(GridCoords))
				{
					Total += Cell->GetTotalInfluence(Channel);
				}
			}
		}
	}
	return Total;
}

void FArcInfluenceGrid::SetCellInfluence(const FIntVector& GridCoords, int32 Channel, float Strength)
{
	check(Channel >= 0 && Channel < Settings.NumChannels);
	FArcInfluenceCell& Cell = FindOrAddCell(GridCoords);
	TArray<FArcInfluenceEntry>& Entries = Cell.ChannelEntries[Channel];

	// Find existing entry with invalid source handle (volume-sourced)
	for (FArcInfluenceEntry& Entry : Entries)
	{
		if (!Entry.Source.IsValid())
		{
			Entry.Strength = Strength;
			return;
		}
	}

	Entries.Add({ Strength, FMassEntityHandle() });
}

void FArcInfluenceGrid::DecayStep(int32 MaxCells)
{
	if (Cells.IsEmpty() || Settings.DecayRate <= 0.f)
	{
		return;
	}

	TArray<FIntVector> Keys;
	Cells.GetKeys(Keys);

	const int32 NumCells = Keys.Num();
	if (NumCells == 0)
	{
		return;
	}

	if (DecayCursor >= NumCells)
	{
		DecayCursor = 0;
	}

	TArray<FIntVector> EmptyCells;

	const int32 EndIndex = FMath::Min(DecayCursor + MaxCells, NumCells);
	for (int32 Idx = DecayCursor; Idx < EndIndex; Idx++)
	{
		const FIntVector& CellCoords = Keys[Idx];
		FArcInfluenceCell* Cell = Cells.Find(CellCoords);
		if (!Cell)
		{
			continue;
		}

		bool bHasEntries = false;
		for (TArray<FArcInfluenceEntry>& Entries : Cell->ChannelEntries)
		{
			for (int32 i = Entries.Num() - 1; i >= 0; i--)
			{
				Entries[i].Strength -= Settings.DecayRate;
				if (Entries[i].Strength <= KINDA_SMALL_NUMBER)
				{
					Entries.RemoveAtSwap(i);
				}
			}
			if (!Entries.IsEmpty())
			{
				bHasEntries = true;
			}
		}

		if (!bHasEntries)
		{
			EmptyCells.Add(CellCoords);
		}
	}

	DecayCursor = (EndIndex >= NumCells) ? 0 : EndIndex;

	for (const FIntVector& Key : EmptyCells)
	{
		Cells.Remove(Key);
	}
}

void FArcInfluenceGrid::TickDecay(float DeltaTime)
{
	if (Settings.DecayRate <= 0.f)
	{
		return;
	}

	DecayAccumulator += DeltaTime;
	if (DecayAccumulator < Settings.DecayInterval)
	{
		return;
	}
	DecayAccumulator = 0;

	DecayStep(Settings.MaxCellsPerUpdate);
}

// ---------------------------------------------------------------------------
// Tiled storage
// ---------------------------------------------------------------------------

namespace ArcInfluenceTiledPrivate
{
	int32 FloorDiv(int32 Value, int32 Divisor)
	{
		const int32 Quotient = Value / Divisor;
		return (Value % Divisor != 0 && (Value < 0) != (Divisor < 0)) ? Quotient - 1 : Quotient;
	}
}

void FArcInfluenceBitplane::Init(int32 NumWords)
{
	Words.SetNumZeroed(NumWords);
}

void FArcInfluenceBitplane::ClearBits()
{
	FMemory::Memzero(Words.GetData(), Words.Num() * sizeof(uint64));
}

bool FArcInfluenceBitplane::IsEmpty() const
{
	for (const uint64 Word : Words)
	{
		if (Word != 0)
		{
			return false;
		}
	}
	return true;
}

void FArcInfluenceTile::Init(int32 InNumWords)
{
	NumWords = InNumWords;
	Occluders.Init(InNumWords);
}

const FArcInfluenceBitplane* FArcInfluenceTile::FindFrontPlane(int32 TeamId) const
{
	return FrontPlanes.Find(TeamId);
}

FArcInfluenceBitplane& FArcInfluenceTile::FindOrAddBackPlane(int32 TeamId)
{
	FArcInfluenceBitplane& Plane = BackPlanes.FindOrAdd(TeamId);
	if (Plane.Words.Num() != NumWords)
	{
		Plane.Init(NumWords);
	}
	return Plane;
}

bool FArcInfluenceTile::IsVisibleToTeam(int32 TeamId, int32 CellIndex) const
{
	const FArcInfluenceBitplane* Plane = FrontPlanes.Find(TeamId);
	return Plane && Plane->GetBit(CellIndex);
}

bool FArcInfluenceTile::IsVisibleToAnyTeam(int32 CellIndex) const
{
	for (const TPair<int32, FArcInfluenceBitplane>& Pair : FrontPlanes)
	{
		if (Pair.Value.GetBit(CellIndex))
		{
			return true;
		}
	}
	return false;
}

bool FArcInfluenceTile::IsVisibleToOtherTeam(int32 TeamId, int32 CellIndex) const
{
	for (const TPair<int32, FArcInfluenceBitplane>& Pair : FrontPlanes)
	{
		if (Pair.Key != TeamId && Pair.Value.GetBit(CellIndex))
		{
			return true;
		}
	}
	return false;
}

void FArcInfluenceTile::ClearBack()
{
	for (TPair<int32, FArcInfluenceBitplane>& Pair : BackPlanes)
	{
		Pair.Value.ClearBits();
	}
}

void FArcInfluenceTile::PublishBack()
{
	if (bJoinsNextCycle)
	{
		bJoinsNextCycle = false;
		return;
	}

	Swap(FrontPlanes, BackPlanes);

	TArray<int32> EmptyTeams;
	for (const TPair<int32, FArcInfluenceBitplane>& Pair : FrontPlanes)
	{
		if (Pair.Value.IsEmpty())
		{
			EmptyTeams.Add(Pair.Key);
		}
	}
	for (const int32 TeamId : EmptyTeams)
	{
		FrontPlanes.Remove(TeamId);
		BackPlanes.Remove(TeamId);
	}
}

int32 FArcInfluenceTiledGrid::GetNumWordsPerPlane() const
{
	return FMath::DivideAndRoundUp(Settings.TileDim * Settings.TileDim, 64);
}

float FArcInfluenceTiledGrid::GetTileWorldSize() const
{
	return Settings.CellSize * Settings.TileDim;
}

FIntPoint FArcInfluenceTiledGrid::WorldToCell(const FVector& WorldPos) const
{
	const double InvCellSize = 1.0 / Settings.CellSize;
	return FIntPoint(
		FMath::FloorToInt32(WorldPos.X * InvCellSize),
		FMath::FloorToInt32(WorldPos.Y * InvCellSize));
}

FIntPoint FArcInfluenceTiledGrid::WorldToTile(const FVector& WorldPos) const
{
	return CellToTile(WorldToCell(WorldPos));
}

FIntPoint FArcInfluenceTiledGrid::CellToTile(const FIntPoint& Cell) const
{
	return FIntPoint(
		ArcInfluenceTiledPrivate::FloorDiv(Cell.X, Settings.TileDim),
		ArcInfluenceTiledPrivate::FloorDiv(Cell.Y, Settings.TileDim));
}

int32 FArcInfluenceTiledGrid::CellIndexInTile(const FIntPoint& Cell) const
{
	const FIntPoint TileCoord = CellToTile(Cell);
	const int32 LocalX = Cell.X - TileCoord.X * Settings.TileDim;
	const int32 LocalY = Cell.Y - TileCoord.Y * Settings.TileDim;
	return LocalY * Settings.TileDim + LocalX;
}

FVector FArcInfluenceTiledGrid::GetCellCenter(const FIntPoint& Cell, double WorldZ) const
{
	const double HalfCell = Settings.CellSize * 0.5;
	return FVector(
		Cell.X * Settings.CellSize + HalfCell,
		Cell.Y * Settings.CellSize + HalfCell,
		WorldZ);
}

FArcInfluenceTile* FArcInfluenceTiledGrid::FindTile(const FIntPoint& TileCoord)
{
	return Tiles.Find(TileCoord);
}

const FArcInfluenceTile* FArcInfluenceTiledGrid::FindTile(const FIntPoint& TileCoord) const
{
	return Tiles.Find(TileCoord);
}

FArcInfluenceTile* FArcInfluenceTiledGrid::AllocateTile(const FIntPoint& TileCoord)
{
	if (FArcInfluenceTile* Existing = Tiles.Find(TileCoord))
	{
		return Existing;
	}

	if (Tiles.Num() >= Settings.MaxTiles)
	{
		RefusedTileAllocations++;
		return nullptr;
	}

	FArcInfluenceTile& Tile = Tiles.Add(TileCoord);
	Tile.Init(GetNumWordsPerPlane());
	OccluderDirtyTiles.Add(TileCoord);
	return &Tile;
}

void FArcInfluenceTiledGrid::ReleaseTile(const FIntPoint& TileCoord)
{
	Tiles.Remove(TileCoord);
	OccluderDirtyTiles.RemoveSingleSwap(TileCoord);
}

bool FArcInfluenceTiledGrid::IsCellVisibleToTeam(const FIntPoint& Cell, int32 TeamId) const
{
	const FArcInfluenceTile* Tile = Tiles.Find(CellToTile(Cell));
	return Tile && Tile->IsVisibleToTeam(TeamId, CellIndexInTile(Cell));
}

bool FArcInfluenceTiledGrid::IsCellVisibleToAnyTeam(const FIntPoint& Cell) const
{
	const FArcInfluenceTile* Tile = Tiles.Find(CellToTile(Cell));
	return Tile && Tile->IsVisibleToAnyTeam(CellIndexInTile(Cell));
}

bool FArcInfluenceTiledGrid::IsCellVisibleToOtherTeam(const FIntPoint& Cell, int32 TeamId) const
{
	const FArcInfluenceTile* Tile = Tiles.Find(CellToTile(Cell));
	return Tile && Tile->IsVisibleToOtherTeam(TeamId, CellIndexInTile(Cell));
}

float FArcInfluenceTiledGrid::QueryInfluence(const FVector& WorldPos) const
{
	return IsCellVisibleToAnyTeam(WorldToCell(WorldPos)) ? 1.f : 0.f;
}

float FArcInfluenceTiledGrid::QueryInfluenceInRadius(const FVector& Center, float Radius) const
{
	const FVector Extent(Radius);
	const FIntPoint MinCell = WorldToCell(Center - Extent);
	const FIntPoint MaxCell = WorldToCell(Center + Extent);
	const float RadiusSq = Radius * Radius;

	for (int32 X = MinCell.X; X <= MaxCell.X; X++)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; Y++)
		{
			const FIntPoint Cell(X, Y);
			if (FVector::DistSquared(Center, GetCellCenter(Cell, Center.Z)) > RadiusSq)
			{
				continue;
			}

			if (IsCellVisibleToAnyTeam(Cell))
			{
				return 1.f;
			}
		}
	}
	return 0.f;
}

// ---------------------------------------------------------------------------
// FArcInfluenceGridStorage
// ---------------------------------------------------------------------------

void FArcInfluenceGridStorage::Initialize(const FArcInfluenceGridConfig& Config)
{
	Reset();
	Kind = Config.Storage;

	if (Kind == EArcInfluenceStorage::Tiled)
	{
		// Editor ClampMin/Max do not apply to ini-authored +Grids= entries, and
		// TileDim*TileDim must not overflow int32.
		checkf(Config.TileDim >= 8 && Config.TileDim <= 256,
			TEXT("TileDim %d out of range [8, 256]"), Config.TileDim);
		Tiled.Settings = Config;
	}
	else
	{
		Sparse.Settings = Config;
	}
}

void FArcInfluenceGridStorage::Reset()
{
	Sparse.Cells.Empty();
	Sparse.DecayCursor = 0;
	Sparse.DecayAccumulator = 0.f;
	Tiled.Tiles.Empty();
	Tiled.OccluderDirtyTiles.Empty();
	Tiled.RefusedTileAllocations = 0;
	bLoggedAddInfluenceRejection = false;
}

const FArcInfluenceGridConfig& FArcInfluenceGridStorage::GetSettings() const
{
	return Kind == EArcInfluenceStorage::Tiled ? Tiled.Settings : Sparse.Settings;
}

// ---------------------------------------------------------------------------
// UArcInfluenceMappingSubsystem
// ---------------------------------------------------------------------------

void UArcInfluenceMappingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UArcInfluenceSettings* Settings = GetDefault<UArcInfluenceSettings>();
	check(Settings);

	Grids.Reset();
	Grids.SetNum(Settings->Grids.Num());
	for (int32 i = 0; i < Grids.Num(); i++)
	{
		const FArcInfluenceGridConfig& Config = Settings->Grids[i];
		check(Config.NumChannels >= 1);

		Grids[i].Initialize(Config);
	}
}

void UArcInfluenceMappingSubsystem::Deinitialize()
{
	Grids.Empty();
	Super::Deinitialize();
}

void UArcInfluenceMappingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if ENABLE_DRAW_DEBUG
	if (CVarArcDebugDrawInfluence.GetValueOnGameThread())
	{
		DebugDrawGrids();
	}
#endif
}

TStatId UArcInfluenceMappingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcInfluenceMappingSubsystem, STATGROUP_Tickables);
}

FArcInfluenceGrid* UArcInfluenceMappingSubsystem::GetGrid(int32 GridIndex)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	FArcInfluenceGridStorage& Storage = Grids[GridIndex];
	return Storage.Kind == EArcInfluenceStorage::Sparse ? &Storage.Sparse : nullptr;
}

FArcInfluenceTiledGrid* UArcInfluenceMappingSubsystem::GetTiledGrid(int32 GridIndex)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	FArcInfluenceGridStorage& Storage = Grids[GridIndex];
	return Storage.Kind == EArcInfluenceStorage::Tiled ? &Storage.Tiled : nullptr;
}

void UArcInfluenceMappingSubsystem::AddInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel, float Strength, FMassEntityHandle Source)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	FArcInfluenceGridStorage& Storage = Grids[GridIndex];

	if (Storage.Kind == EArcInfluenceStorage::Tiled)
	{
		UE_CLOGF(!Storage.bLoggedAddInfluenceRejection, LogMass, Warning,"AddInfluence rejected on tiled influence grid %d: tiled grids carry per-team visibility bits, not float channels.",
			GridIndex);
		Storage.bLoggedAddInfluenceRejection = true;
		return;
	}

	Storage.Sparse.AddInfluence(Storage.Sparse.WorldToGrid(WorldPos), Channel, Strength, Source);
}

float UArcInfluenceMappingSubsystem::QueryInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel) const
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	const FArcInfluenceGridStorage& Storage = Grids[GridIndex];

	return Storage.Kind == EArcInfluenceStorage::Tiled
		? Storage.Tiled.QueryInfluence(WorldPos)
		: Storage.Sparse.QueryInfluence(WorldPos, Channel);
}

float UArcInfluenceMappingSubsystem::QueryInfluenceInRadius(int32 GridIndex, const FVector& Center, float Radius, int32 Channel) const
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	const FArcInfluenceGridStorage& Storage = Grids[GridIndex];

	return Storage.Kind == EArcInfluenceStorage::Tiled
		? Storage.Tiled.QueryInfluenceInRadius(Center, Radius)
		: Storage.Sparse.QueryInfluenceInRadius(Center, Radius, Channel);
}

#if ENABLE_DRAW_DEBUG

void UArcInfluenceMappingSubsystem::DebugDrawGrids() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FArcInfluenceGridStorage& Storage : Grids)
	{
		const FArcInfluenceGridConfig& Config = Storage.GetSettings();
		const float CellSize = Config.CellSize;
		const float HalfCell = CellSize * 0.5f;

		if (Storage.Kind == EArcInfluenceStorage::Sparse)
		{
			for (const TPair<FIntVector, FArcInfluenceCell>& CellPair : Storage.Sparse.Cells)
			{
				const FIntVector& Coords = CellPair.Key;

				const FVector CellCenter(
					Coords.X * CellSize + HalfCell,
					Coords.Y * CellSize + HalfCell,
					Config.bIs2D ? 0.f : (Coords.Z * CellSize + HalfCell));

				float TotalStrength = 0.f;
				for (const TArray<FArcInfluenceEntry>& Entries : CellPair.Value.ChannelEntries)
				{
					for (const FArcInfluenceEntry& Entry : Entries)
					{
						TotalStrength += Entry.Strength;
					}
				}

				const FColor DrawColor = FColor::MakeRedToGreenColorFromScalar(FMath::Clamp(TotalStrength, 0.f, 1.f));
				DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f), DrawColor, false, -1.f, 0, 2.f);
			}
			continue;
		}

		const FArcInfluenceTiledGrid& Tiled = Storage.Tiled;
		const int32 TileDim = Config.TileDim;
		const float TileSize = Tiled.GetTileWorldSize();

		for (const TPair<FIntPoint, FArcInfluenceTile>& TilePair : Tiled.Tiles)
		{
			const FIntPoint& TileCoord = TilePair.Key;
			const FArcInfluenceTile& Tile = TilePair.Value;

			const FVector TileCenter(
				TileCoord.X * TileSize + TileSize * 0.5f,
				TileCoord.Y * TileSize + TileSize * 0.5f,
				0.f);
			DrawDebugBox(World, TileCenter, FVector(TileSize * 0.5f, TileSize * 0.5f, HalfCell),
				Tile.bJoinsNextCycle ? FColor::Yellow : FColor::Cyan, false, -1.f, 0, 4.f);

			for (int32 LocalY = 0; LocalY < TileDim; LocalY++)
			{
				for (int32 LocalX = 0; LocalX < TileDim; LocalX++)
				{
					const int32 CellIndex = LocalY * TileDim + LocalX;
					const bool bOccluded = Tile.Occluders.GetBit(CellIndex);
					const bool bVisible = Tile.IsVisibleToAnyTeam(CellIndex);
					if (!bOccluded && !bVisible)
					{
						continue;
					}

					const FVector CellCenter(
						(TileCoord.X * TileDim + LocalX) * CellSize + HalfCell,
						(TileCoord.Y * TileDim + LocalY) * CellSize + HalfCell,
						0.f);
					DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f),
						bOccluded ? FColor::Black : FColor::Red, false, -1.f, 0, 2.f);
				}
			}
		}
	}
}

#endif // ENABLE_DRAW_DEBUG

// ---------------------------------------------------------------------------
// UArcInfluenceMappingProcessor
// ---------------------------------------------------------------------------

void UArcInfluenceMappingTraitBase::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct StateTreeFragment = EntityManager.GetOrCreateConstSharedFragment(InfluenceConfig);
	BuildContext.AddConstSharedFragment(StateTreeFragment);
}

UArcInfluenceMappingProcessor::UArcInfluenceMappingProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	bAutoRegisterWithProcessingPhases = false;
	
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcInfluenceMappingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcInfluenceSourceFragment>(EMassFragmentPresence::All);
	AddGridTagRequirement(EntityQuery);
}

void UArcInfluenceMappingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (GridIndex == INDEX_NONE)
	{
		return;
	}
	
	UWorld* World = EntityManager.GetWorld();
	UArcInfluenceMappingSubsystem* Subsystem = World ? World->GetSubsystem<UArcInfluenceMappingSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcInfluenceMapping);

	const int32 GridIdx = GridIndex;
	check(GridIdx >= 0 && GridIdx < Subsystem->GetGridCount());
	FArcInfluenceGrid* GridPtr = Subsystem->GetGrid(GridIdx);
	if (!GridPtr)
	{
		return;
	}
	FArcInfluenceGrid& Grid = *GridPtr;

	// Phase 1: Collect all influence into a local batch
	TArray<FArcPendingInfluence> PendingBatch;

	EntityQuery.ForEachEntityChunk(Context,
		[&Grid, &PendingBatch](FMassExecutionContext& Ctx)
		{
			const FArcInfluenceSourceFragment& SourceConfig = Ctx.GetConstSharedFragment<FArcInfluenceSourceFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			const float CellSize = Grid.Settings.CellSize;
			const float HalfCell = CellSize * 0.5f;
			const bool bIs2D = Grid.Settings.bIs2D;
			const float Radius = SourceConfig.Radius;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FVector EntityPos = Transforms[EntityIt].GetTransform().GetLocation();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				for (const FArcInfluenceChannelStrength& ChStr : SourceConfig.Channels)
				{
					check(ChStr.Channel >= 0 && ChStr.Channel < Grid.Settings.NumChannels);

					if (Radius <= 0.f)
					{
						PendingBatch.Add({ Grid.WorldToGrid(EntityPos), ChStr.Channel, ChStr.Strength, Entity });
					}
					else
					{
						const int32 CellRadius = FMath::CeilToInt(Radius / CellSize);
						const FIntVector CenterCoord = Grid.WorldToGrid(EntityPos);
						const float RadiusSq = Radius * Radius;

						const int32 MinZ = bIs2D ? 0 : -CellRadius;
						const int32 MaxZ = bIs2D ? 0 : CellRadius;

						for (int32 DX = -CellRadius; DX <= CellRadius; DX++)
						{
							for (int32 DY = -CellRadius; DY <= CellRadius; DY++)
							{
								for (int32 DZ = MinZ; DZ <= MaxZ; DZ++)
								{
									const FIntVector CellCoord = CenterCoord + FIntVector(DX, DY, DZ);
									const FVector CellCenter(
										CellCoord.X * CellSize + HalfCell,
										CellCoord.Y * CellSize + HalfCell,
										bIs2D ? EntityPos.Z : (CellCoord.Z * CellSize + HalfCell)
									);

									if (FVector::DistSquared(EntityPos, CellCenter) <= RadiusSq)
									{
										const float Dist = FVector::Dist(EntityPos, CellCenter);
										const float Falloff = FMath::Max(0.f, 1.f - (Dist / Radius));
										PendingBatch.Add({ CellCoord, ChStr.Channel, ChStr.Strength * Falloff, Entity });
									}
								}
							}
						}
					}
				}
			}
		});

	// Phase 2: Batch insert all collected influence
	Grid.AddInfluenceBatch(PendingBatch);

	// Phase 3: Decay (skip semi-static grids)
	if (!Grid.Settings.bIsSemiStatic)
	{
		Grid.TickDecay(Context.GetDeltaTimeSeconds());
	}
}

// ---------------------------------------------------------------------------
// AArcInfluenceVolume
// ---------------------------------------------------------------------------

AArcInfluenceVolume::AArcInfluenceVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxShape"));
	BoxComponent->SetupAttachment(Root);
	BoxComponent->SetBoxExtent(FVector(500.f));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetGenerateOverlapEvents(false);

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereShape"));
	SphereComponent->SetupAttachment(Root);
	SphereComponent->SetSphereRadius(500.f);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(false);

	UpdateShapeVisibility();
}

void AArcInfluenceVolume::BeginPlay()
{
	Super::BeginPlay();

	if (bApplyOnBeginPlay)
	{
		ApplyToGrid();
	}
}

void AArcInfluenceVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveFromGrid();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AArcInfluenceVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AArcInfluenceVolume, Shape))
	{
		UpdateShapeVisibility();
	}
}
#endif

void AArcInfluenceVolume::UpdateShapeVisibility()
{
	if (BoxComponent)
	{
		BoxComponent->SetVisibility(Shape == EArcInfluenceVolumeShape::Box);
	}
	if (SphereComponent)
	{
		SphereComponent->SetVisibility(Shape == EArcInfluenceVolumeShape::Sphere);
	}
}

void AArcInfluenceVolume::ApplyToGrid()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcInfluenceMappingSubsystem* Subsystem = World->GetSubsystem<UArcInfluenceMappingSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	check(GridIndex >= 0 && GridIndex < Subsystem->GetGridCount());
	FArcInfluenceGrid* GridPtr = Subsystem->GetGrid(GridIndex);
	if (!GridPtr)
	{
		return;
	}
	FArcInfluenceGrid& Grid = *GridPtr;
	check(Channel >= 0 && Channel < Grid.Settings.NumChannels);

	if (bApplied)
	{
		RemoveFromGrid();
	}

	const float CellSize = Grid.Settings.CellSize;
	const float HalfCell = CellSize * 0.5f;
	const FVector ActorLocation = GetActorLocation();

	if (Shape == EArcInfluenceVolumeShape::Box)
	{
		const FVector BoxExtent = BoxComponent->GetScaledBoxExtent();
		const FVector MinWorld = ActorLocation - BoxExtent;
		const FVector MaxWorld = ActorLocation + BoxExtent;
		const FIntVector MinGrid = Grid.WorldToGrid(MinWorld);
		const FIntVector MaxGrid = Grid.WorldToGrid(MaxWorld);

		const int32 MinZ = Grid.Settings.bIs2D ? 0 : MinGrid.Z;
		const int32 MaxZ = Grid.Settings.bIs2D ? 0 : MaxGrid.Z;

		for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
		{
			for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
			{
				for (int32 Z = MinZ; Z <= MaxZ; Z++)
				{
					Grid.SetCellInfluence(FIntVector(X, Y, Z), Channel, Strength);
				}
			}
		}
	}
	else // Sphere
	{
		const float SphereRadius = SphereComponent->GetScaledSphereRadius();
		const float RadiusSq = SphereRadius * SphereRadius;
		const FVector Extent(SphereRadius);
		const FIntVector MinGrid = Grid.WorldToGrid(ActorLocation - Extent);
		const FIntVector MaxGrid = Grid.WorldToGrid(ActorLocation + Extent);

		const int32 MinZ = Grid.Settings.bIs2D ? 0 : MinGrid.Z;
		const int32 MaxZ = Grid.Settings.bIs2D ? 0 : MaxGrid.Z;

		for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
		{
			for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
			{
				for (int32 Z = MinZ; Z <= MaxZ; Z++)
				{
					const FVector CellCenter(
						X * CellSize + HalfCell,
						Y * CellSize + HalfCell,
						Grid.Settings.bIs2D ? ActorLocation.Z : (Z * CellSize + HalfCell)
					);

					if (FVector::DistSquared(ActorLocation, CellCenter) <= RadiusSq)
					{
						Grid.SetCellInfluence(FIntVector(X, Y, Z), Channel, Strength);
					}
				}
			}
		}
	}

	bApplied = true;
}

void AArcInfluenceVolume::RemoveFromGrid()
{
	if (!bApplied)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcInfluenceMappingSubsystem* Subsystem = World->GetSubsystem<UArcInfluenceMappingSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	check(GridIndex >= 0 && GridIndex < Subsystem->GetGridCount());
	FArcInfluenceGrid* GridPtr = Subsystem->GetGrid(GridIndex);
	if (!GridPtr)
	{
		return;
	}
	FArcInfluenceGrid& Grid = *GridPtr;
	check(Channel >= 0 && Channel < Grid.Settings.NumChannels);

	const float CellSize = Grid.Settings.CellSize;
	const float HalfCell = CellSize * 0.5f;
	const FVector ActorLocation = GetActorLocation();

	if (Shape == EArcInfluenceVolumeShape::Box)
	{
		const FVector BoxExtent = BoxComponent->GetScaledBoxExtent();
		const FVector MinWorld = ActorLocation - BoxExtent;
		const FVector MaxWorld = ActorLocation + BoxExtent;
		const FIntVector MinGrid = Grid.WorldToGrid(MinWorld);
		const FIntVector MaxGrid = Grid.WorldToGrid(MaxWorld);

		const int32 MinZ = Grid.Settings.bIs2D ? 0 : MinGrid.Z;
		const int32 MaxZ = Grid.Settings.bIs2D ? 0 : MaxGrid.Z;

		for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
		{
			for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
			{
				for (int32 Z = MinZ; Z <= MaxZ; Z++)
				{
					Grid.SetCellInfluence(FIntVector(X, Y, Z), Channel, 0.f);
				}
			}
		}
	}
	else // Sphere
	{
		const float SphereRadius = SphereComponent->GetScaledSphereRadius();
		const float RadiusSq = SphereRadius * SphereRadius;
		const FVector Extent(SphereRadius);
		const FIntVector MinGrid = Grid.WorldToGrid(ActorLocation - Extent);
		const FIntVector MaxGrid = Grid.WorldToGrid(ActorLocation + Extent);

		const int32 MinZ = Grid.Settings.bIs2D ? 0 : MinGrid.Z;
		const int32 MaxZ = Grid.Settings.bIs2D ? 0 : MaxGrid.Z;

		for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
		{
			for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
			{
				for (int32 Z = MinZ; Z <= MaxZ; Z++)
				{
					const FVector CellCenter(
						X * CellSize + HalfCell,
						Y * CellSize + HalfCell,
						Grid.Settings.bIs2D ? ActorLocation.Z : (Z * CellSize + HalfCell)
					);

					if (FVector::DistSquared(ActorLocation, CellCenter) <= RadiusSq)
					{
						Grid.SetCellInfluence(FIntVector(X, Y, Z), Channel, 0.f);
					}
				}
			}
		}
	}

	bApplied = false;
}
