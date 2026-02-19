// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassInfluenceMapping.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

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

const FArcInfluenceEntry* FArcInfluenceCell::GetStrongestSource(int32 Channel) const
{
	check(Channel >= 0 && Channel < ChannelEntries.Num());
	const FArcInfluenceEntry* Strongest = nullptr;
	for (const FArcInfluenceEntry& Entry : ChannelEntries[Channel])
	{
		if (!Strongest || Entry.Strength > Strongest->Strength)
		{
			Strongest = &Entry;
		}
	}
	return Strongest;
}

bool FArcInfluenceCell::IsEmpty() const
{
	for (const TArray<FArcInfluenceEntry>& Entries : ChannelEntries)
	{
		if (!Entries.IsEmpty())
		{
			return false;
		}
	}
	return true;
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

void FArcInfluenceGrid::RemoveInfluenceBySource(FMassEntityHandle Source)
{
	TArray<FIntVector> EmptyCells;

	for (auto& Pair : Cells)
	{
		bool bHasEntries = false;
		for (TArray<FArcInfluenceEntry>& Entries : Pair.Value.ChannelEntries)
		{
			Entries.RemoveAll([Source](const FArcInfluenceEntry& Entry)
			{
				return Entry.Source == Source;
			});
			if (!Entries.IsEmpty())
			{
				bHasEntries = true;
			}
		}

		if (!bHasEntries)
		{
			EmptyCells.Add(Pair.Key);
		}
	}

	for (const FIntVector& Key : EmptyCells)
	{
		Cells.Remove(Key);
	}
}

FArcInfluenceCell* FArcInfluenceGrid::QueryCell(const FIntVector& GridCoords)
{
	return Cells.Find(GridCoords);
}

const FArcInfluenceCell* FArcInfluenceGrid::QueryCell(const FIntVector& GridCoords) const
{
	return Cells.Find(GridCoords);
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

void FArcInfluenceGrid::PropagateStep(int32 MaxCells)
{
	if (Cells.IsEmpty() || Settings.PropagationRate <= 0.f)
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

	if (PropagationCursor >= NumCells)
	{
		PropagationCursor = 0;
	}

	static const FIntVector Neighbors2D[] = {
		FIntVector( 1,  0, 0),
		FIntVector(-1,  0, 0),
		FIntVector( 0,  1, 0),
		FIntVector( 0, -1, 0)
	};
	static const FIntVector Neighbors3D[] = {
		FIntVector( 1,  0,  0),
		FIntVector(-1,  0,  0),
		FIntVector( 0,  1,  0),
		FIntVector( 0, -1,  0),
		FIntVector( 0,  0,  1),
		FIntVector( 0,  0, -1)
	};

	const FIntVector* NeighborOffsets = Settings.bIs2D ? Neighbors2D : Neighbors3D;
	const int32 NumNeighbors = Settings.bIs2D ? 4 : 6;
	const int32 NumChannels = Settings.NumChannels;

	struct FPropagated
	{
		int32 Channel;
		float Strength;
		FMassEntityHandle Source;
	};
	TMap<FIntVector, TArray<FPropagated>> PropagatedEntries;

	const int32 EndIndex = FMath::Min(PropagationCursor + MaxCells, NumCells);
	for (int32 Idx = PropagationCursor; Idx < EndIndex; Idx++)
	{
		const FIntVector& CellCoords = Keys[Idx];
		const FArcInfluenceCell* Cell = Cells.Find(CellCoords);
		if (!Cell)
		{
			continue;
		}

		for (int32 Ch = 0; Ch < NumChannels; Ch++)
		{
			for (const FArcInfluenceEntry& Entry : Cell->ChannelEntries[Ch])
			{
				const float SpreadStrength = Entry.Strength * Settings.PropagationRate;
				if (SpreadStrength < KINDA_SMALL_NUMBER)
				{
					continue;
				}

				for (int32 N = 0; N < NumNeighbors; N++)
				{
					const FIntVector NeighborCoords = CellCoords + NeighborOffsets[N];
					PropagatedEntries.FindOrAdd(NeighborCoords).Add({ Ch, SpreadStrength, Entry.Source });
				}
			}
		}
	}

	PropagationCursor = (EndIndex >= NumCells) ? 0 : EndIndex;

	for (auto& Pair : PropagatedEntries)
	{
		FArcInfluenceCell& TargetCell = FindOrAddCell(Pair.Key);

		for (const FPropagated& Propagated : Pair.Value)
		{
			TArray<FArcInfluenceEntry>& Entries = TargetCell.ChannelEntries[Propagated.Channel];

			bool bMerged = false;
			for (FArcInfluenceEntry& Existing : Entries)
			{
				if (Existing.Source == Propagated.Source)
				{
					Existing.Strength += Propagated.Strength;
					bMerged = true;
					break;
				}
			}
			if (!bMerged)
			{
				Entries.Add({ Propagated.Strength, Propagated.Source });
			}
		}
	}
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
// UArcInfluenceMappingSubsystem
// ---------------------------------------------------------------------------

void UArcInfluenceMappingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UArcInfluenceSettings* Settings = GetDefault<UArcInfluenceSettings>();
	check(Settings);

	Grids.SetNum(Settings->Grids.Num());
	for (int32 i = 0; i < Grids.Num(); i++)
	{
		const FArcInfluenceGridConfig& Config = Settings->Grids[i];
		check(Config.NumChannels >= 1);

		Grids[i].Settings = Config;
		Grids[i].Cells.Empty();
		Grids[i].PropagationCursor = 0;
		Grids[i].DecayCursor = 0;
		Grids[i].DecayAccumulator = 0.f;
	}
}

void UArcInfluenceMappingSubsystem::Deinitialize()
{
	for (FArcInfluenceGrid& Grid : Grids)
	{
		Grid.Cells.Empty();
		Grid.PropagationCursor = 0;
		Grid.DecayCursor = 0;
		Grid.DecayAccumulator = 0.f;
	}
	Grids.Empty();
	Super::Deinitialize();
}

FArcInfluenceGrid& UArcInfluenceMappingSubsystem::GetGrid(int32 GridIndex)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	return Grids[GridIndex];
}

void UArcInfluenceMappingSubsystem::AddInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel, float Strength, FMassEntityHandle Source)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	FArcInfluenceGrid& Grid = Grids[GridIndex];
	Grid.AddInfluence(Grid.WorldToGrid(WorldPos), Channel, Strength, Source);
}

void UArcInfluenceMappingSubsystem::RemoveInfluenceBySource(int32 GridIndex, FMassEntityHandle Source)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	Grids[GridIndex].RemoveInfluenceBySource(Source);
}

float UArcInfluenceMappingSubsystem::QueryInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel) const
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	return Grids[GridIndex].QueryInfluence(WorldPos, Channel);
}

float UArcInfluenceMappingSubsystem::QueryInfluenceInRadius(int32 GridIndex, const FVector& Center, float Radius, int32 Channel) const
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	return Grids[GridIndex].QueryInfluenceInRadius(Center, Radius, Channel);
}

void UArcInfluenceMappingSubsystem::UpdateSemiStaticGrid(int32 GridIndex)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	FArcInfluenceGrid& Grid = Grids[GridIndex];
	if (!Grid.Settings.bIsSemiStatic)
	{
		return;
	}

	const int32 NumCells = Grid.Cells.Num();
	if (NumCells > 0)
	{
		Grid.PropagateStep(NumCells);
		Grid.DecayStep(NumCells);
	}
}

void UArcInfluenceMappingSubsystem::SetCellInfluence(int32 GridIndex, const FIntVector& GridCoords, int32 Channel, float Strength)
{
	check(GridIndex >= 0 && GridIndex < Grids.Num());
	Grids[GridIndex].SetCellInfluence(GridCoords, Channel, Strength);
}

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
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
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

	const int32 GridIdx = GridIndex;
	check(GridIdx >= 0 && GridIdx < Subsystem->GetGridCount());
	FArcInfluenceGrid& Grid = Subsystem->GetGrid(GridIdx);

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

#if WITH_GAMEPLAY_DEBUGGER
	if (CVarArcDebugDrawInfluence.GetValueOnAnyThread() && World)
	{
		const float CellSize = Grid.Settings.CellSize;
		const float HalfCell = CellSize * 0.5f;

		for (const auto& CellPair : Grid.Cells)
		{
			const FIntVector& Coords = CellPair.Key;
			const FArcInfluenceCell& Cell = CellPair.Value;

			FVector CellCenter(
				Coords.X * CellSize + HalfCell,
				Coords.Y * CellSize + HalfCell,
				Grid.Settings.bIs2D ? 0.f : (Coords.Z * CellSize + HalfCell)
			);

			float TotalStrength = 0.f;
			for (const TArray<FArcInfluenceEntry>& Entries : Cell.ChannelEntries)
			{
				for (const FArcInfluenceEntry& Entry : Entries)
				{
					TotalStrength += Entry.Strength;
				}
			}

			const float Alpha = FMath::Clamp(TotalStrength, 0.f, 1.f);
			const FColor DrawColor = FColor::MakeRedToGreenColorFromScalar(Alpha);
			//const FColor DrawColor = Alpha > 0.1f ? FColor::Green : FColor::Red;
			
			DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f), DrawColor, false, -1.f, 0, 2.f);
		}
	}
#endif
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
	FArcInfluenceGrid& Grid = Subsystem->GetGrid(GridIndex);
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
	FArcInfluenceGrid& Grid = Subsystem->GetGrid(GridIndex);
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
