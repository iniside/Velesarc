// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Persistence/ArcMassEntityPersistenceSubsystem.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "MassEntitySubsystem.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "ArcPersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Storage/ArcPersistenceBackend.h"

bool UArcMassEntityPersistenceSubsystem::ShouldCreateSubsystem(
	UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->WorldType == EWorldType::Game;
}

void UArcMassEntityPersistenceSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcMassEntityPersistenceSubsystem::Deinitialize()
{
	SaveAndUnloadAll();
	Super::Deinitialize();
}

FMassEntityManager& UArcMassEntityPersistenceSubsystem::GetEntityManager() const
{
	check(CachedEntityManager);
	return *CachedEntityManager;
}

void UArcMassEntityPersistenceSubsystem::Configure(
	float InCellSize, float InLoadRadius, const FGuid& InWorldId)
{
	CellSize = InCellSize;
	LoadRadius = InLoadRadius;
	WorldId = InWorldId;

	UMassEntitySubsystem* MassSubsystem =
		GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (MassSubsystem)
	{
		CachedEntityManager = &MassSubsystem->GetMutableEntityManager();
	}
}

FString UArcMassEntityPersistenceSubsystem::MakeCellStorageKey(
	const FIntVector& Cell) const
{
	return FString::Printf(TEXT("world/%s/cells/%s"),
		*WorldId.ToString(),
		*UE::ArcMass::Persistence::CellToKey(Cell));
}

// ── Cell Operations ──────────────────────────────────────────────────────

void UArcMassEntityPersistenceSubsystem::LoadCell(const FIntVector& Cell)
{
	if (LoadedCells.Contains(Cell))
	{
		return;
	}

	const UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI)
	{
		return;
	}
	UArcPersistenceSubsystem* PersistSub = GI->GetSubsystem<UArcPersistenceSubsystem>();
	IArcPersistenceBackend* Backend = PersistSub ? PersistSub->GetBackend() : nullptr;
	if (!Backend)
	{
		return;
	}

	TArray<uint8> Data;
	if (Backend->LoadEntry(MakeCellStorageKey(Cell), Data))
	{
		DeserializeAndSpawnCell(Cell, Data);
	}

	LoadedCells.Add(Cell);

	UE_LOG(LogTemp, Log,
		TEXT("ArcMassPersistence: Loaded cell [%d,%d] (%d entities)"),
		Cell.X, Cell.Y,
		CellEntityMap.Contains(Cell) ? CellEntityMap[Cell].Num() : 0);
}

void UArcMassEntityPersistenceSubsystem::SaveCell(const FIntVector& Cell)
{
	if (!LoadedCells.Contains(Cell))
	{
		return;
	}

	const UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI)
	{
		return;
	}
	UArcPersistenceSubsystem* PersistSub = GI->GetSubsystem<UArcPersistenceSubsystem>();
	IArcPersistenceBackend* Backend = PersistSub ? PersistSub->GetBackend() : nullptr;
	if (!Backend)
	{
		return;
	}

	TArray<uint8> Data = SerializeCell(Cell);
	Backend->SaveEntry(MakeCellStorageKey(Cell), Data);
}

void UArcMassEntityPersistenceSubsystem::UnloadCell(const FIntVector& Cell)
{
	if (!LoadedCells.Contains(Cell))
	{
		return;
	}

	// Step 1: Save the cell
	SaveCell(Cell);

	FMassEntityManager& EM = GetEntityManager();

	// Step 2: Handle entities loaded from this cell but now in other cells
	// — update their StorageCell so they save with their new cell
	for (auto& Pair : ActiveEntities)
	{
		if (!EM.IsEntityValid(Pair.Value))
		{
			continue;
		}

		FArcMassPersistenceFragment* PFrag =
			EM.GetFragmentDataPtr<FArcMassPersistenceFragment>(Pair.Value);
		if (!PFrag)
		{
			continue;
		}

		if (PFrag->StorageCell == Cell && PFrag->CurrentCell != Cell)
		{
			PFrag->StorageCell = PFrag->CurrentCell;
		}
	}

	// Step 3: Destroy entities currently in this cell
	TArray<FGuid> ToDestroy;
	if (const TSet<FGuid>* CellGuids = CellEntityMap.Find(Cell))
	{
		for (const FGuid& Guid : *CellGuids)
		{
			ToDestroy.Add(Guid);
		}
	}

	for (const FGuid& Guid : ToDestroy)
	{
		if (FMassEntityHandle* Handle = ActiveEntities.Find(Guid))
		{
			if (EM.IsEntityValid(*Handle))
			{
				EM.DestroyEntity(*Handle);
			}
			ActiveEntities.Remove(Guid);
		}
	}

	CellEntityMap.Remove(Cell);
	LoadedCells.Remove(Cell);

	UE_LOG(LogTemp, Log,
		TEXT("ArcMassPersistence: Unloaded cell [%d,%d] (%d entities destroyed)"),
		Cell.X, Cell.Y, ToDestroy.Num());
}

void UArcMassEntityPersistenceSubsystem::SaveAndUnloadAll()
{
	TArray<FIntVector> CellsToUnload = LoadedCells.Array();
	for (const FIntVector& Cell : CellsToUnload)
	{
		UnloadCell(Cell);
	}
}

bool UArcMassEntityPersistenceSubsystem::IsCellLoaded(
	const FIntVector& Cell) const
{
	return LoadedCells.Contains(Cell);
}

// ── Source Management ────────────────────────────────────────────────────

void UArcMassEntityPersistenceSubsystem::UpdateActiveCells(
	const TArray<FVector>& SourcePositions)
{
	// Compute desired cells from source positions
	TSet<FIntVector> DesiredCells;
	const int32 CellRadius = FMath::CeilToInt32(LoadRadius / CellSize);

	for (const FVector& Pos : SourcePositions)
	{
		const FIntVector Center =
			UE::ArcMass::Persistence::WorldToCell(Pos, CellSize);

		for (int32 X = -CellRadius; X <= CellRadius; ++X)
		{
			for (int32 Y = -CellRadius; Y <= CellRadius; ++Y)
			{
				DesiredCells.Add(FIntVector(
					Center.X + X, Center.Y + Y, 0));
			}
		}
	}

	// Load new cells
	for (const FIntVector& Cell : DesiredCells)
	{
		if (!LoadedCells.Contains(Cell))
		{
			LoadCell(Cell);
		}
	}

	// Unload cells no longer needed (save-before-unload enforced in UnloadCell)
	TArray<FIntVector> CellsToUnload;
	for (const FIntVector& Cell : LoadedCells)
	{
		if (!DesiredCells.Contains(Cell))
		{
			CellsToUnload.Add(Cell);
		}
	}

	for (const FIntVector& Cell : CellsToUnload)
	{
		UnloadCell(Cell);
	}
}

// ── Entity Cell Tracking ─────────────────────────────────────────────────

void UArcMassEntityPersistenceSubsystem::OnEntityCellChanged(
	const FGuid& Guid, const FIntVector& OldCell, const FIntVector& NewCell)
{
	if (TSet<FGuid>* OldSet = CellEntityMap.Find(OldCell))
	{
		OldSet->Remove(Guid);
	}

	CellEntityMap.FindOrAdd(NewCell).Add(Guid);
}

// ── Internal: Serialization ──────────────────────────────────────────────

TArray<uint8> UArcMassEntityPersistenceSubsystem::SerializeCell(
	const FIntVector& Cell)
{
	FMassEntityManager& EM = GetEntityManager();

	FArcJsonSaveArchive SaveAr;

	const TSet<FGuid>* CellGuids = CellEntityMap.Find(Cell);
	const int32 Count = CellGuids ? CellGuids->Num() : 0;

	SaveAr.BeginArray(FName("entities"), Count);

	if (CellGuids)
	{
		int32 Index = 0;
		for (const FGuid& Guid : *CellGuids)
		{
			const FMassEntityHandle* Handle = ActiveEntities.Find(Guid);
			if (!Handle || !EM.IsEntityValid(*Handle))
			{
				continue;
			}

			SaveAr.BeginArrayElement(Index);
			SaveAr.WriteProperty(FName("_guid"), Guid);

			// Get config fragment for serialization filtering
			const FArcMassPersistenceConfigFragment* Config =
				EM.GetConstSharedFragmentDataPtr<
					FArcMassPersistenceConfigFragment>(*Handle);

			FArcMassPersistenceConfigFragment DefaultConfig;
			FArcMassFragmentSerializer::SaveEntityFragments(
				EM, *Handle,
				Config ? *Config : DefaultConfig,
				SaveAr);

			SaveAr.EndArrayElement();
			++Index;
		}
	}

	SaveAr.EndArray();

	return SaveAr.Finalize();
}

void UArcMassEntityPersistenceSubsystem::DeserializeAndSpawnCell(
	const FIntVector& Cell, const TArray<uint8>& Data)
{
	FArcJsonLoadArchive LoadAr;
	if (!LoadAr.InitializeFromData(Data))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ArcMassPersistence: Failed to parse cell [%d,%d] data"),
			Cell.X, Cell.Y);
		return;
	}

	int32 EntityCount = 0;
	if (!LoadAr.BeginArray(FName("entities"), EntityCount))
	{
		return;
	}

	for (int32 i = 0; i < EntityCount; ++i)
	{
		if (!LoadAr.BeginArrayElement(i))
		{
			continue;
		}

		FGuid Guid;
		if (!LoadAr.ReadProperty(FName("_guid"), Guid))
		{
			LoadAr.EndArrayElement();
			continue;
		}

		// Skip if already active (migrated from another cell)
		if (ActiveEntities.Contains(Guid))
		{
			LoadAr.EndArrayElement();
			continue;
		}

		// TODO: Entity spawning from saved type
		// Full implementation needs:
		// 1. Read _meta.type -> find UMassEntityConfigAsset
		// 2. Spawn entity from template
		// 3. Apply fragment data via LoadEntityFragments
		// 4. Set PersistenceGuid and StorageCell on fragment
		//
		// For now, the save path fully works. The load path structure
		// is ready for when entity config registry is available.

		FString EntityType;
		if (LoadAr.BeginStruct(FName("_meta")))
		{
			LoadAr.ReadProperty(FName("type"), EntityType);
			LoadAr.EndStruct();
		}

		LoadAr.EndArrayElement();
	}

	LoadAr.EndArray();
}
