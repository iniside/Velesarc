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
#include "Storage/ArcPersistenceResult.h"
#include "Storage/ArcPersistenceKeyConvention.h"
#include "ArcMass/Persistence/ArcMassPersistenceSettings.h"
#include "Async/Async.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassCommandBuffer.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "AssetRegistry/IAssetRegistry.h"

namespace
{
	struct FCellEntityRecord
	{
		FGuid Guid;
		FGuid ConfigGuid;
		TArray<FInstancedStruct> Fragments;
	};

	// Parses a cell blob into per-entity records with fully deserialized
	// fragment data.  Safe to call on any thread (no shared state access).
	TArray<FCellEntityRecord> ParseCellBlob(const TArray<uint8>& Data)
	{
		TArray<FCellEntityRecord> Records;

		FArcJsonLoadArchive LoadAr;
		if (!LoadAr.InitializeFromData(Data))
		{
			return Records;
		}

		int32 EntityCount = 0;
		if (!LoadAr.BeginArray(FName("entities"), EntityCount))
		{
			return Records;
		}

		Records.Reserve(EntityCount);

		for (int32 i = 0; i < EntityCount; ++i)
		{
			if (!LoadAr.BeginArrayElement(i))
			{
				continue;
			}

			FCellEntityRecord Record;
			if (!LoadAr.ReadProperty(FName("_guid"), Record.Guid))
			{
				LoadAr.EndArrayElement();
				continue;
			}

			// Optional — old saves won't have this field
			LoadAr.ReadProperty(FName("_configGuid"), Record.ConfigGuid);

			// Parse fragment data (mirrors LoadEntityFragments but into
			// standalone FInstancedStruct instead of live entity memory)
			int32 FragCount = 0;
			if (LoadAr.BeginArray(FName("fragments"), FragCount))
			{
				Record.Fragments.Reserve(FragCount);

				for (int32 f = 0; f < FragCount; ++f)
				{
					if (!LoadAr.BeginArrayElement(f))
					{
						continue;
					}

					FString TypePath;
					if (!LoadAr.ReadProperty(FName("_type"), TypePath))
					{
						LoadAr.EndArrayElement();
						continue;
					}

					const UScriptStruct* FragType =
						FindObject<UScriptStruct>(nullptr, *TypePath);
					if (!FragType)
					{
						LoadAr.EndArrayElement();
						continue;
					}

					FInstancedStruct Instance;
					Instance.InitializeAs(FragType);

					// LoadFragment reads _version, checks it, and loads data
					FArcMassFragmentSerializer::LoadFragment(
						FragType, Instance.GetMutableMemory(), LoadAr);

					Record.Fragments.Add(MoveTemp(Instance));
					LoadAr.EndArrayElement();
				}

				LoadAr.EndArray();
			}

			Records.Add(MoveTemp(Record));
			LoadAr.EndArrayElement();
		}

		LoadAr.EndArray();
		return Records;
	}

	UMassEntityConfigAsset* ResolveConfigAsset(const FGuid& ConfigGuid)
	{
		if (!ConfigGuid.IsValid())
		{
			return nullptr;
		}

		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		TMultiMap<FName, FString> TagValues;
		TagValues.Add(FName("MassEntityConfigGuid"), ConfigGuid.ToString());

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByTagValues(TagValues, Assets);

		if (Assets.IsEmpty())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ArcMassPersistence: No UMassEntityConfigAsset found for ConfigGuid %s"),
				*ConfigGuid.ToString());
			return nullptr;
		}

		return Cast<UMassEntityConfigAsset>(Assets[0].GetAsset());
	}

	// Overlay saved fragment data and set persistence tracking on a batch of
	// entity handles.  Shared between template and legacy paths.
	void ApplyRecordsToEntities(
		UArcMassEntityPersistenceSubsystem* Sub,
		FMassEntityManager& EM,
		const FIntVector& Cell,
		TArray<FCellEntityRecord>& Records,
		TConstArrayView<int32> RecordIndices,
		TConstArrayView<FMassEntityHandle> Handles)
	{
		for (int32 j = 0; j < RecordIndices.Num(); ++j)
		{
			FCellEntityRecord& Record = Records[RecordIndices[j]];
			const FMassEntityHandle Handle = Handles[j];

			// Copy parsed fragment data onto the entity
			FMassEntityView View(EM, Handle);
			for (const FInstancedStruct& Frag : Record.Fragments)
			{
				const UScriptStruct* Type = Frag.GetScriptStruct();
				FStructView Target = View.GetFragmentDataStruct(Type);
				if (Target.IsValid())
				{
					Type->CopyScriptStruct(
						Target.GetMemory(), Frag.GetMemory());
				}
			}

			// Set persistence tracking (StorageCell/CurrentCell
			// are not UPROPERTY so they're not in saved data)
			FArcMassPersistenceFragment* PFrag =
				EM.GetFragmentDataPtr<FArcMassPersistenceFragment>(Handle);
			if (PFrag)
			{
				PFrag->PersistenceGuid = Record.Guid;
				PFrag->StorageCell = Cell;
				PFrag->CurrentCell = Cell;
			}

			Sub->ActiveEntities.Add(Record.Guid, Handle);
			Sub->CellEntityMap.FindOrAdd(Cell).Add(Record.Guid);
		}
	}

	// Batch-create entities from parsed records.  Runs during EM command flush.
	void SpawnParsedEntities(
		UArcMassEntityPersistenceSubsystem* Sub,
		const FIntVector& Cell,
		TArray<FCellEntityRecord>& Records)
	{
		FMassEntityManager& EM = Sub->GetEntityManager();

		// Separate records into template-based (valid ConfigGuid) and legacy groups
		struct FTemplateGroup
		{
			FGuid ConfigGuid;
			TArray<int32> RecordIndices;
		};

		struct FLegacyGroup
		{
			TArray<const UScriptStruct*> FragmentTypes;
			TArray<int32> RecordIndices;
		};

		TArray<FTemplateGroup> TemplateGroups;
		TArray<FLegacyGroup> LegacyGroups;

		for (int32 i = 0; i < Records.Num(); ++i)
		{
			if (Sub->ActiveEntities.Contains(Records[i].Guid))
			{
				continue;
			}

			if (Records[i].ConfigGuid.IsValid())
			{
				// Group by ConfigGuid
				int32 GroupIdx = TemplateGroups.IndexOfByPredicate(
					[&Records, i](const FTemplateGroup& G)
					{
						return G.ConfigGuid == Records[i].ConfigGuid;
					});

				if (GroupIdx == INDEX_NONE)
				{
					GroupIdx = TemplateGroups.Num();
					TemplateGroups.Add({Records[i].ConfigGuid, {}});
				}
				TemplateGroups[GroupIdx].RecordIndices.Add(i);
			}
			else
			{
				// Legacy path: group by fragment types
				TArray<const UScriptStruct*> Types;
				for (const FInstancedStruct& Frag : Records[i].Fragments)
				{
					Types.AddUnique(Frag.GetScriptStruct());
				}
				Types.AddUnique(FArcMassPersistenceFragment::StaticStruct());
				Types.Sort();

				int32 GroupIdx = LegacyGroups.IndexOfByPredicate(
					[&Types](const FLegacyGroup& G)
					{
						return G.FragmentTypes == Types;
					});

				if (GroupIdx == INDEX_NONE)
				{
					GroupIdx = LegacyGroups.Num();
					LegacyGroups.Add({Types, {}});
				}
				LegacyGroups[GroupIdx].RecordIndices.Add(i);
			}
		}

		// ── Template path: resolve config asset → spawn via template ────
		UWorld* World = Sub->GetWorld();
		UMassSpawnerSubsystem* SpawnerSub = World
			? World->GetSubsystem<UMassSpawnerSubsystem>()
			: nullptr;

		for (const FTemplateGroup& Group : TemplateGroups)
		{
			UMassEntityConfigAsset* ConfigAsset =
				ResolveConfigAsset(Group.ConfigGuid);
			if (!ConfigAsset || !SpawnerSub)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("ArcMassPersistence: Falling back to legacy spawn for ConfigGuid %s"),
					*Group.ConfigGuid.ToString());

				// Fall back to legacy for these records
				FLegacyGroup Fallback;
				for (int32 Idx : Group.RecordIndices)
				{
					for (const FInstancedStruct& Frag : Records[Idx].Fragments)
					{
						Fallback.FragmentTypes.AddUnique(Frag.GetScriptStruct());
					}
				}
				Fallback.FragmentTypes.AddUnique(FArcMassPersistenceFragment::StaticStruct());
				Fallback.FragmentTypes.Sort();
				Fallback.RecordIndices = Group.RecordIndices;
				LegacyGroups.Add(MoveTemp(Fallback));
				continue;
			}

			const FMassEntityTemplate& Template =
				ConfigAsset->GetOrCreateEntityTemplate(*World);

			TArray<FMassEntityHandle> Handles;
			{
				TSharedPtr<FMassEntityManager::FEntityCreationContext> Context =
					SpawnerSub->SpawnEntities(
						Template,
						static_cast<uint32>(Group.RecordIndices.Num()),
						Handles);

				ApplyRecordsToEntities(
					Sub, EM, Cell, Records, Group.RecordIndices, Handles);
			} // Context released → observer notification
		}

		// ── Legacy path: archetype from fragment types, BatchCreateEntities ──
		for (const FLegacyGroup& Group : LegacyGroups)
		{
			FMassElementBitSet Elements;
			for (const UScriptStruct* Type : Group.FragmentTypes)
			{
				Elements.Add(TNotNull<const UScriptStruct*>(Type));
			}
			Elements.Add<FArcMassPersistenceTag>();

			const FMassArchetypeHandle Archetype =
				EM.CreateArchetype(Elements);

			TArray<FMassEntityHandle> Handles;
			{
				TSharedRef<FMassEntityManager::FEntityCreationContext> Context =
					EM.BatchCreateEntities(
						Archetype, Group.RecordIndices.Num(), Handles);

				ApplyRecordsToEntities(
					Sub, EM, Cell, Records, Group.RecordIndices, Handles);
			} // Context released → observer notification
		}
	}
}

bool UArcMassEntityPersistenceSubsystem::ShouldCreateSubsystem(
	UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UArcMassEntityPersistenceSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Super::Initialize(Collection);

	// Auto-configure from developer settings so the subsystem works without
	// an explicit Configure() call (which only tests were doing).
	const UArcMassPersistenceSettings* Settings = GetDefault<UArcMassPersistenceSettings>();
	const FGuid AutoWorldId = FGuid::NewDeterministicGuid(
		GetWorld()->GetFName().ToString());
	Configure(Settings->CellSize, Settings->LoadRadius, AutoWorldId);
}

void UArcMassEntityPersistenceSubsystem::Deinitialize()
{
	// Save all loaded cells while the EntityManager is still alive.
	// Skip entity destruction — the world teardown handles that.
	if (CachedEntityManager)
	{
		for (const FIntVector& Cell : LoadedCells)
		{
			SaveCell(Cell);
		}
	}

	ActiveEntities.Empty();
	CellEntityMap.Empty();
	LoadedCells.Empty();
	PendingCellLoads.Empty();
	PendingSaveCells.Empty();
	CachedEntityManager = nullptr;
	FlushBackend();
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
	const FString SubKey = FString::Printf(TEXT("cells/%s"),
		*UE::ArcMass::Persistence::CellToKey(Cell));
	return UE::ArcPersistence::MakeWorldKey(WorldId.ToString(), SubKey);
}

// ── Cell Operations ──────────────────────────────────────────────────────

void UArcMassEntityPersistenceSubsystem::LoadCell(const FIntVector& Cell)
{
	if (LoadedCells.Contains(Cell) || PendingCellLoads.Contains(Cell))
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

	PendingCellLoads.Add(Cell);

	TFuture<FArcPersistenceLoadResult> Future = Backend->LoadEntry(MakeCellStorageKey(Cell));
	TWeakObjectPtr<UArcMassEntityPersistenceSubsystem> WeakThis(this);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, Cell, Future = MoveTemp(Future)]() mutable
	{
		FArcPersistenceLoadResult LoadResult = Future.Get();

		// Deserialize on background thread — pure data parsing, no shared state
		TArray<FCellEntityRecord> Records;
		if (LoadResult.bSuccess)
		{
			Records = ParseCellBlob(LoadResult.Data);
		}

		// Hop to game thread only to dispatch a deferred command.
		// Actual entity creation runs when the EntityManager flushes.
		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, Cell, Records = MoveTemp(Records)]() mutable
		{
			UArcMassEntityPersistenceSubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}

			This->PendingCellLoads.Remove(Cell);

			// Cell was unloaded while we were loading — discard
			if (This->LoadedCells.Contains(Cell))
			{
				return;
			}

			// Dispatch entity creation through Mass command buffer.
			// SpawnParsedEntities runs during EM flush, not here.
			TWeakObjectPtr<UArcMassEntityPersistenceSubsystem> WeakSub(This);
			FMassEntityManager& EM = This->GetEntityManager();
			EM.Defer().PushCommand<FMassDeferredCreateCommand>(
				[WeakSub, Cell, Records = MoveTemp(Records)]
				(FMassEntityManager&) mutable
			{
				UArcMassEntityPersistenceSubsystem* Sub = WeakSub.Get();
				if (!Sub || Sub->LoadedCells.Contains(Cell))
				{
					return;
				}

				SpawnParsedEntities(Sub, Cell, Records);
				Sub->LoadedCells.Add(Cell);

				// Flush any queued saves now that the cell is loaded
				if (Sub->PendingSaveCells.Contains(Cell))
				{
					Sub->PendingSaveCells.Remove(Cell);
					Sub->SaveCell(Cell);
				}

				UE_LOG(LogTemp, Log,
					TEXT("ArcMassPersistence: Loaded cell [%d,%d] (%d entities)"),
					Cell.X, Cell.Y, Records.Num());
			});
		});
	});
}

void UArcMassEntityPersistenceSubsystem::SaveCell(const FIntVector& Cell)
{
	if (!CachedEntityManager)
	{
		return;
	}

	// Cell is loaded — serialize and save immediately
	if (LoadedCells.Contains(Cell))
	{
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
		Backend->SaveEntry(MakeCellStorageKey(Cell), MoveTemp(Data));
		return;
	}

	// Cell has entities but isn't loaded — queue save and trigger load
	if (CellEntityMap.Contains(Cell))
	{
		PendingSaveCells.Add(Cell);
		if (!IsCellLoadingOrLoaded(Cell))
		{
			LoadCell(Cell);
		}
	}
}

void UArcMassEntityPersistenceSubsystem::UnloadCell(const FIntVector& Cell)
{
	// Cancel pending async load if still in-flight — the game thread
	// callback checks LoadedCells and will discard the result.
	PendingCellLoads.Remove(Cell);

	if (!LoadedCells.Contains(Cell))
	{
		return;
	}

	// Step 1: Save the cell (SaveCell guards EntityManager internally)
	SaveCell(Cell);

	int32 DestroyedCount = 0;

	// Steps 2-3 require a live EntityManager. During world teardown
	// UMassEntitySubsystem may already be gone — skip entity ops.
	if (CachedEntityManager)
	{
		FMassEntityManager& EM = *CachedEntityManager;

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

		// Step 3: Destroy entities currently in this cell via deferred commands
		TArray<FGuid> ToDestroy;
		if (const TSet<FGuid>* CellGuids = CellEntityMap.Find(Cell))
		{
			for (const FGuid& Guid : *CellGuids)
			{
				ToDestroy.Add(Guid);
			}
		}

		TArray<FMassEntityHandle> HandlesToDestroy;
		HandlesToDestroy.Reserve(ToDestroy.Num());

		for (const FGuid& Guid : ToDestroy)
		{
			if (FMassEntityHandle* Handle = ActiveEntities.Find(Guid))
			{
				if (EM.IsEntityValid(*Handle))
				{
					HandlesToDestroy.Add(*Handle);
				}
				ActiveEntities.Remove(Guid);
			}
		}

		DestroyedCount = HandlesToDestroy.Num();
		if (HandlesToDestroy.Num() > 0)
		{
			EM.Defer().DestroyEntities(MoveTemp(HandlesToDestroy));
		}
	}

	CellEntityMap.Remove(Cell);
	LoadedCells.Remove(Cell);

	UE_LOG(LogTemp, Log,
		TEXT("ArcMassPersistence: Unloaded cell [%d,%d] (%d entities destroyed)"),
		Cell.X, Cell.Y, DestroyedCount);
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

bool UArcMassEntityPersistenceSubsystem::IsCellLoadingOrLoaded(
	const FIntVector& Cell) const
{
	return LoadedCells.Contains(Cell) || PendingCellLoads.Contains(Cell);
}

void UArcMassEntityPersistenceSubsystem::FlushBackend()
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}
	UArcPersistenceSubsystem* PersistSub = GI->GetSubsystem<UArcPersistenceSubsystem>();
	IArcPersistenceBackend* Backend = PersistSub ? PersistSub->GetBackend() : nullptr;
	if (Backend)
	{
		Backend->Flush();
	}
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
		if (!IsCellLoadingOrLoaded(Cell))
		{
			LoadCell(Cell);
		}
	}

	// Cancel pending loads no longer needed
	TArray<FIntVector> PendingToCancel;
	for (const FIntVector& Cell : PendingCellLoads)
	{
		if (!DesiredCells.Contains(Cell))
		{
			PendingToCancel.Add(Cell);
		}
	}
	for (const FIntVector& Cell : PendingToCancel)
	{
		PendingCellLoads.Remove(Cell);
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
		if (OldSet->IsEmpty())
		{
			CellEntityMap.Remove(OldCell);
		}
	}

	CellEntityMap.FindOrAdd(NewCell).Add(Guid);
}

// ── Synchronous Load (for testing / editor use) ─────────────────────────

void UArcMassEntityPersistenceSubsystem::LoadCellFromData(
	const FIntVector& Cell, const TArray<uint8>& Data)
{
	TArray<FCellEntityRecord> Records = ParseCellBlob(Data);
	SpawnParsedEntities(this, Cell, Records);
	LoadedCells.Add(Cell);
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

			if (Config && Config->ConfigGuid.IsValid())
			{
				SaveAr.WriteProperty(FName("_configGuid"), Config->ConfigGuid);
			}

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

