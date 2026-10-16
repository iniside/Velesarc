// Copyright Lukasz Baran. All Rights Reserved.

#include "Build/ArcTownTeardown.h"

#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "Build/ArcTownInstanceEmitter.h"
#include "Build/ArcTownPathSync.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Instances/ArcTownPartitionActor.h"
#include "Layers/LayersSubsystem.h"
#include "Stamps/ArcTerrainSplineModifier.h"
#include "Terrain/ArcTownPadStamp.h"
#include "Town/ArcTownVolume.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownTeardown, Log, All);

namespace ArcTownTeardownPrivate
{
	UWorldPartition* GetWorldPartition(UWorld& World)
	{
		return World.IsPartitionedWorld() ? World.GetWorldPartition() : nullptr;
	}

	/**
	 * Only a world open in the level editor may have its actors cascaded away: PIE, inactive and
	 * commandlet worlds destroy actors for reasons that are not a user deleting a settlement.
	 */
	bool IsTeardownAllowed(const UWorld& World)
	{
		return World.WorldType == EWorldType::Editor
			&& !World.bIsTearingDown
			&& !IsEngineExitRequested();
	}

	/**
	 * Resolves one partition actor, loading it through a pin when World Partition has it streamed out.
	 * A pin the caller keeps is what holds the actor loaded long enough to be destroyed.
	 */
	template <typename ActorType>
	ActorType* ResolveActor(
		UWorldPartition* WorldPartition,
		const FGuid& ActorGuid,
		ActorType* LoadedActor,
		TArray<FWorldPartitionReference>& InOutPins)
	{
		if (IsValid(LoadedActor))
		{
			return LoadedActor;
		}
		if (WorldPartition == nullptr || !ActorGuid.IsValid())
		{
			return nullptr;
		}

		FWorldPartitionReference Reference(WorldPartition, ActorGuid);
		if (!Reference.IsValid())
		{
			return nullptr;
		}
		ActorType* const Actor = Cast<ActorType>(Reference.GetActor());
		if (Actor != nullptr)
		{
			InOutPins.Add(Reference);
		}
		return Actor;
	}

	int32 ClearEntityHosts(
		UWorld& World,
		AArcTownVolume* Town,
		const FGuid& TownId,
		UWorldPartition* WorldPartition)
	{
		TArray<FWorldPartitionReference> Pins;
		TArray<AArcPlacedEntityPartitionActor*> Hosts;
		TSet<FGuid> ResolvedGuids;

		if (Town != nullptr)
		{
			for (const FArcTownEmittedActor& Record : Town->EmittedEntityHosts)
			{
				AArcPlacedEntityPartitionActor* const Host = ResolveActor(
					WorldPartition,
					Record.ActorGuid,
					Cast<AArcPlacedEntityPartitionActor>(Record.Actor.Get()),
					Pins);
				if (Host != nullptr)
				{
					ResolvedGuids.Add(Host->GetActorGuid());
					Hosts.AddUnique(Host);
				}
			}
		}

		TArray<FGuid> DescGuids;
		FArcTownInstanceEmitter::CollectEntityHostDescGuids(
			WorldPartition, FArcTownInstanceEmitter::MakeEntityHostGridGuid(TownId), DescGuids);
		for (const FGuid& DescGuid : DescGuids)
		{
			if (ResolvedGuids.Contains(DescGuid))
			{
				continue;
			}
			AArcPlacedEntityPartitionActor* const Host = ResolveActor<AArcPlacedEntityPartitionActor>(
				WorldPartition, DescGuid, nullptr, Pins);
			if (Host != nullptr)
			{
				ResolvedGuids.Add(DescGuid);
				Hosts.AddUnique(Host);
			}
		}

		if (Town != nullptr)
		{
			Town->Modify();
			Town->EmittedEntityHosts.Empty();
		}

		int32 DestroyedCount = 0;
		for (AArcPlacedEntityPartitionActor* Host : Hosts)
		{
			if (!IsValid(Host))
			{
				continue;
			}
			Host->Modify();
			if (Host->RemoveEntriesOwnedBy(TownId)
				&& World.EditorDestroyActor(Host, !Host->IsPackageExternal()))
			{
				++DestroyedCount;
			}
		}

		Pins.Reset();
		return DestroyedCount;
	}

	void CollectDescGuids(
		UWorldPartition* WorldPartition,
		const TSubclassOf<AActor>& ActorClass,
		TArray<FGuid>& OutGuids)
	{
		FWorldPartitionHelpers::ForEachActorDescInstance(
			WorldPartition,
			ActorClass,
			[&OutGuids](const FWorldPartitionActorDescInstance* DescInstance) -> bool
			{
				if (DescInstance != nullptr && DescInstance->GetGuid().IsValid())
				{
					OutGuids.Add(DescInstance->GetGuid());
				}
				return true;
			});
	}

	/**
	 * Every TownId a volume in this world still carries, whether that volume is loaded or not. Volume
	 * descs that fail to resolve land in OutUnresolvedGuids: their towns are missing from OutTownIds,
	 * which makes every actor those towns own look orphaned.
	 */
	void CollectLiveTownIds(
		UWorld& World,
		UWorldPartition* WorldPartition,
		TSet<FGuid>& OutTownIds,
		TArray<FGuid>& OutUnresolvedGuids)
	{
		for (TActorIterator<AArcTownVolume> It(&World); It; ++It)
		{
			const AArcTownVolume* const Town = *It;
			if (Town != nullptr && Town->TownId.IsValid())
			{
				OutTownIds.Add(Town->TownId);
			}
		}

		if (WorldPartition == nullptr)
		{
			return;
		}

		// An unloaded volume would otherwise make its whole settlement look orphaned, so every volume
		// desc is loaded just long enough to read its identity.
		TArray<FGuid> VolumeGuids;
		CollectDescGuids(WorldPartition, AArcTownVolume::StaticClass(), VolumeGuids);
		for (const FGuid& VolumeGuid : VolumeGuids)
		{
			const FWorldPartitionReference Reference(WorldPartition, VolumeGuid);
			const AArcTownVolume* const Town =
				Reference.IsValid() ? Cast<AArcTownVolume>(Reference.GetActor()) : nullptr;
			if (Town == nullptr)
			{
				OutUnresolvedGuids.Add(VolumeGuid);
				continue;
			}
			if (Town->TownId.IsValid())
			{
				OutTownIds.Add(Town->TownId);
			}
		}
	}

	/** Levels TActorIterator skips, which would hide the volumes they hold. */
	void CollectUniteratedLevelNames(UWorld& World, TArray<FString>& OutNames)
	{
		for (const ULevel* Level : World.GetLevels())
		{
			if (Level != nullptr && !Level->bIsVisible)
			{
				OutNames.Add(Level->GetOutermost()->GetName());
			}
		}
	}

	AArcTownVolume* FindLoadedVolume(UWorld& World, const FGuid& TownId)
	{
		for (TActorIterator<AArcTownVolume> It(&World); It; ++It)
		{
			if (*It != nullptr && (*It)->TownId == TownId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	constexpr ERenameFlags DiscardRenameFlags =
		REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional;

	bool MatchesGeneratedPadName(const FString& Name)
	{
		return Name.StartsWith(ArcTownPadComponentPrefix, ESearchCase::CaseSensitive);
	}

	bool MatchesGeneratedRoadName(const FString& Name)
	{
		return Name.Compare(ArcTownRoadComponentName, ESearchCase::CaseSensitive) == 0;
	}

	/**
	 * Destroys the town's terrain stamps of one kind, whose removal is the other half of undoing a
	 * build: a cleared town that keeps its stamps keeps their cuts carved into the terrain.
	 */
	template <typename StampType, typename NamePredicate>
	int32 DestroyTownStamps(AArcTownVolume& Town, NamePredicate NameMatches, const TCHAR* DiscardBaseName)
	{
		TArray<StampType*> Stamps;
		Town.GetComponents<StampType>(Stamps);

		int32 DestroyedCount = 0;
		// Destroying walks the snapshot gathered above, never the owned-component set itself.
		for (StampType* Stamp : Stamps)
		{
			if (!IsValid(Stamp) || !NameMatches(Stamp->GetName()))
			{
				continue;
			}
			// Frees the deterministic name a later rebuild reuses; DestroyComponent only marks garbage,
			// and a name still held would make StaticAllocateObject overwrite this object in place.
			const FName DiscardName =
				MakeUniqueObjectName(Stamp->GetOuter(), Stamp->GetClass(), FName(DiscardBaseName));
			Stamp->Rename(*DiscardName.ToString(), nullptr, DiscardRenameFlags);
			Stamp->DestroyComponent();
			++DestroyedCount;
		}
		return DestroyedCount;
	}

	int32 DestroyRoadMeshes(TConstArrayView<AArcTownPartitionActor*> Actors)
	{
		int32 DestroyedCount = 0;
		for (AArcTownPartitionActor* Actor : Actors)
		{
			if (IsValid(Actor))
			{
				Actor->Modify();
				DestroyedCount += Actor->DestroyGeneratedRoadSegments();
				DestroyedCount += Actor->DestroyGeneratedRoadNodes();
			}
		}
		return DestroyedCount;
	}

	int32 DestroyAll(UWorld& World, TConstArrayView<AArcTownPartitionActor*> Actors)
	{
		ULayersSubsystem* const Layers =
			GEditor != nullptr ? GEditor->GetEditorSubsystem<ULayersSubsystem>() : nullptr;

		int32 DestroyedCount = 0;
		for (AArcTownPartitionActor* Actor : Actors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}
			Actor->Modify();
			if (Layers != nullptr)
			{
				Layers->DisassociateActorFromLayers(Actor);
			}
			// EditorDestroyActor, not DestroyActor: the ISM components carry navigation-relevant
			// geometry, and only the editor wrapper unregisters them from the navigation system.
			if (World.EditorDestroyActor(Actor, !Actor->IsPackageExternal()))
			{
				++DestroyedCount;
			}
		}
		return DestroyedCount;
	}

	void ExecuteSweepOrphans(UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}
		ArcTownTeardown::SweepOrphans(*World);
	}

	FAutoConsoleCommand SweepOrphansCommand(
		TEXT("ArcTown.SweepOrphans"),
		TEXT("Destroys every ArcTown partition actor whose owning volume no longer exists."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ExecuteSweepOrphans));
}

int32 ArcTownTeardown::ClearTown(
	UWorld& World,
	AArcTownVolume* Town,
	const FGuid& TownId,
	const FBox& SearchBounds)
{
	using namespace ArcTownTeardownPrivate;
	check(IsInGameThread());

	if (!TownId.IsValid() || !IsTeardownAllowed(World))
	{
		return 0;
	}

	if (Town != nullptr && Town->TownId != TownId)
	{
		UE_LOGF(LogArcTownTeardown, Error,"ClearTown was given volume '%ls' carrying town %ls but asked to clear town %ls; nothing was destroyed.",
			*Town->GetName(),
			*Town->TownId.ToString(),
			*TownId.ToString());
		return 0;
	}

	if (Town == nullptr)
	{
		Town = FindLoadedVolume(World, TownId);
	}

	UWorldPartition* const WorldPartition = GetWorldPartition(World);

	TArray<FWorldPartitionReference> Pins;
	TArray<AArcTownPartitionActor*> Owned;
	TSet<FGuid> ResolvedGuids;

	if (Town != nullptr)
	{
		for (const FArcTownEmittedActor& Record : Town->EmittedActors)
		{
			AArcTownPartitionActor* const Actor = ResolveActor<AArcTownPartitionActor>(
				WorldPartition, Record.ActorGuid, Cast<AArcTownPartitionActor>(Record.Actor.Get()), Pins);
			if (Actor == nullptr)
			{
				continue;
			}
			ResolvedGuids.Add(Actor->GetActorGuid());
			// Someone else's actor, or one placed by hand: the ledger is not proof of ownership.
			if (Actor->OwnerGuid == TownId)
			{
				Owned.AddUnique(Actor);
			}
		}
	}

	if (WorldPartition != nullptr && SearchBounds.IsValid != 0)
	{
		// A building sits at terrain height, which the authored volume box need not span, so the desc
		// search grows by one partition grid size in Z.
		const uint32 GridSize = AArcTownPartitionActor::StaticClass()
			->GetDefaultObject<AArcTownPartitionActor>()->GetDefaultGridSize(&World);
		const FBox DescSearchBounds =
			SearchBounds.ExpandBy(FVector(0.0, 0.0, static_cast<double>(GridSize)));

		TArray<FGuid> DescGuids;
		FWorldPartitionHelpers::ForEachIntersectingActorDescInstance(
			WorldPartition,
			DescSearchBounds,
			AArcTownPartitionActor::StaticClass(),
			[&DescGuids, &ResolvedGuids](const FWorldPartitionActorDescInstance* DescInstance) -> bool
			{
				if (DescInstance != nullptr && !ResolvedGuids.Contains(DescInstance->GetGuid()))
				{
					DescGuids.Add(DescInstance->GetGuid());
				}
				return true;
			});

		// Collected before any is resolved: pinning loads an actor, which writes to the very container
		// the iteration walks.
		for (const FGuid& DescGuid : DescGuids)
		{
			AArcTownPartitionActor* const Actor =
				ResolveActor<AArcTownPartitionActor>(WorldPartition, DescGuid, nullptr, Pins);
			if (Actor != nullptr && Actor->OwnerGuid == TownId)
			{
				Owned.AddUnique(Actor);
			}
		}
	}

	int32 ClearedStampCount = 0;
	int32 ClearedPathCount = 0;
	if (Town != nullptr)
	{
		Town->Modify();
		Town->EmittedActors.Empty();
		ClearedStampCount =
			DestroyTownStamps<UArcTownPadStamp>(*Town, &MatchesGeneratedPadName, TEXT("ArcTownDiscardedPad"))
			+ DestroyTownStamps<UArcTerrainSplineModifier>(
				*Town, &MatchesGeneratedRoadName, TEXT("ArcTownDiscardedRoads"));
		ClearedPathCount = ArcTownPathSync::DestroyGeneratedPaths(*Town);
	}
	const int32 ClearedRoadMeshCount = DestroyRoadMeshes(Owned);
	const int32 ClearedEntityHostCount = ClearEntityHosts(World, Town, TownId, WorldPartition);
	const int32 ClearedCount = DestroyAll(World, Owned);

	// Released only here: dropping a pin unregisters the actor it names, and the destroys above need
	// every one of them still loaded.
	Pins.Reset();

	UE_LOGF(LogArcTownTeardown, Log,"ArcTown cleared %d partition actor(s), %d placed-entity host(s), %d road mesh component(s), %d terrain stamp(s) and %d generated path component(s) of town %ls in world '%ls'. Undo does not bring them back; rebuild the town.",
		ClearedCount,
		ClearedEntityHostCount,
		ClearedRoadMeshCount,
		ClearedStampCount,
		ClearedPathCount,
		*TownId.ToString(),
		*World.GetName());
	return ClearedCount;
}

int32 ArcTownTeardown::SweepOrphans(UWorld& World)
{
	using namespace ArcTownTeardownPrivate;
	check(IsInGameThread());

	if (!IsTeardownAllowed(World))
	{
		UE_LOGF(LogArcTownTeardown, Warning,"ArcTown.SweepOrphans runs on a world open in the level editor only; nothing was swept.");
		return 0;
	}

	UWorldPartition* const WorldPartition = GetWorldPartition(World);

	// A sweep that cannot enumerate every owner cannot tell an orphan from a live town's actor, so an
	// incomplete enumeration refuses the whole pass instead of destroying against a partial owner set.
	TArray<FString> UniteratedLevelNames;
	CollectUniteratedLevelNames(World, UniteratedLevelNames);
	if (!UniteratedLevelNames.IsEmpty())
	{
		for (const FString& LevelName : UniteratedLevelNames)
		{
			UE_LOGF(LogArcTownTeardown, Error,"ArcTown.SweepOrphans cannot read level '%ls': it is not visible, so the volumes it holds are invisible to the sweep.",
				*LevelName);
		}
		UE_LOGF(LogArcTownTeardown, Error,"ArcTown.SweepOrphans destroyed nothing: %d level(s) could not be read. Make them visible and sweep again.",
			UniteratedLevelNames.Num());
		return 0;
	}

	TSet<FGuid> LiveTownIds;
	TArray<FGuid> UnresolvedVolumeGuids;
	CollectLiveTownIds(World, WorldPartition, LiveTownIds, UnresolvedVolumeGuids);
	if (!UnresolvedVolumeGuids.IsEmpty())
	{
		for (const FGuid& VolumeGuid : UnresolvedVolumeGuids)
		{
			UE_LOGF(LogArcTownTeardown, Error,"ArcTown.SweepOrphans could not load town volume %ls; the town it names cannot be ruled live.",
				*VolumeGuid.ToString());
		}
		UE_LOGF(LogArcTownTeardown, Error,"ArcTown.SweepOrphans destroyed nothing: %d town volume(s) could not be resolved.",
			UnresolvedVolumeGuids.Num());
		return 0;
	}

	TArray<FWorldPartitionReference> Pins;
	TArray<AArcTownPartitionActor*> Orphans;
	TArray<AArcTownPartitionActor*> Unowned;

	const TFunction<void(AArcTownPartitionActor*)> ConsiderActor =
		[&LiveTownIds, &Orphans, &Unowned](AArcTownPartitionActor* Actor) -> void
	{
		if (!IsValid(Actor))
		{
			return;
		}
		// No owner id names no town, and the ledger is not proof of ownership either: hand-placed
		// actors are nobody's to destroy.
		if (!Actor->OwnerGuid.IsValid())
		{
			Unowned.AddUnique(Actor);
			return;
		}
		if (LiveTownIds.Contains(Actor->OwnerGuid))
		{
			return;
		}
		Orphans.AddUnique(Actor);
	};

	// An actor spawned this session has no desc yet, so the loaded set is walked whether the world is
	// partitioned or not.
	for (TActorIterator<AArcTownPartitionActor> It(&World); It; ++It)
	{
		ConsiderActor(*It);
	}

	if (WorldPartition != nullptr)
	{
		TArray<FGuid> ActorGuids;
		CollectDescGuids(WorldPartition, AArcTownPartitionActor::StaticClass(), ActorGuids);
		for (const FGuid& ActorGuid : ActorGuids)
		{
			ConsiderActor(ResolveActor<AArcTownPartitionActor>(WorldPartition, ActorGuid, nullptr, Pins));
		}
	}

	for (const AArcTownPartitionActor* UnownedActor : Unowned)
	{
		UE_LOGF(LogArcTownTeardown, Warning,"ArcTown partition actor '%ls' carries no owner id: unowned, left in place.",
			*UnownedActor->GetName());
	}

	for (const AArcTownPartitionActor* Orphan : Orphans)
	{
		UE_LOGF(LogArcTownTeardown, Warning,"Orphaned ArcTown partition actor '%ls' names owner %ls, which no volume carries.",
			*Orphan->GetName(),
			*Orphan->OwnerGuid.ToString());
	}

	const int32 DestroyedCount = Orphans.IsEmpty() ? 0 : DestroyAll(World, Orphans);

	// Same ordering as ClearTown: the pins outlive the destroys they enable.
	Pins.Reset();

	UE_LOGF(LogArcTownTeardown, Log,"ArcTown.SweepOrphans destroyed %d orphaned partition actor(s) and left %d unowned one(s) in world '%ls'. Undo does not bring them back.",
		DestroyedCount,
		Unowned.Num(),
		*World.GetName());
	return DestroyedCount;
}
