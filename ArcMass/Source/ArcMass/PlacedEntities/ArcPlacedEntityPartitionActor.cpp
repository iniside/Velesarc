// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMassRenderLogs.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/EntityFragments.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ActorComponent.h" // FRegisterComponentContext (skinned proxy rebuild in UpdateInstanceRender)
#include "Mass/EntityBuilder.h"

#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/PlacedEntities/ArcLinkableGuid.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrait.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrackerTypes.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcPersistenceSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Storage/ArcPersistenceBackend.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Mesh/ArcRenderMeshFragments.h"

#if WITH_EDITOR
#include "ArcMass/NiagaraVisualization/ArcNiagaraVisTrait.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "NiagaraComponent.h"
#include "Editor.h"
#include "Editor/EditorEngineDelegates.h"
#include "Selection.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
// Componentless render path
#include "Mesh/ArcRenderMeshUtils.h"
#include "Mesh/ArcRenderStateHelper.h"
#include "Mesh/ArcISMRenderStateHelper.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "Misc/App.h"
#include "ShaderCompiler.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
// Skinned componentless render path
#include "ArcMass/SkinnedMeshVisualization/ArcSkinnedMeshHolderRegistry.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
// TEMP DIAGNOSTIC: package-dirty callstack capture
#include "HAL/PlatformStackWalk.h"
#include "UObject/Package.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityPartitionActor)

#if WITH_EDITOR
// TEMP DIAGNOSTIC: dumps the C++ callstack whenever this actor's package is marked dirty, so we can
// pin the engine-side trigger for the perpetual "Save modified actors?" prompt on level close.
// Remove once the dirtying source is identified.
namespace ArcPlacedEntityPartitionActor_DirtyDiag
{

static void DumpDirtyCallstack(const UPackage* Package, bool bWasDirty, const AActor* Actor)
{
	const SIZE_T BufferSize = 65535;
	ANSICHAR* Buffer = static_cast<ANSICHAR*>(FMemory::SystemMalloc(BufferSize));
	if (Buffer == nullptr)
	{
		return;
	}
	Buffer[0] = '\0';
	FPlatformStackWalk::StackWalkAndDump(Buffer, BufferSize, /*IgnoreCount*/0);
	UE_LOGF(LogTemp, Warning,"[PlacedDirty] MarkPackageDirty fired: Package='%ls' bWasDirty=%d Actor='%ls'\nCallstack:\n%ls",
		Package != nullptr ? *Package->GetName() : TEXT("<null>"),
		bWasDirty ? 1 : 0,
		Actor != nullptr ? *Actor->GetName() : TEXT("<null>"),
		ANSI_TO_TCHAR(Buffer));
	FMemory::SystemFree(Buffer);
}

} // namespace ArcPlacedEntityPartitionActor_DirtyDiag
#endif // WITH_EDITOR

AArcPlacedEntityPartitionActor::AArcPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// -----------------------------------------------------------------------------
// Runtime Entity Spawning
// -----------------------------------------------------------------------------

void AArcPlacedEntityPartitionActor::BeginPlay()
{
	Super::BeginPlay();

	// Grow removal overlay to match Entries before loading so LoadRemovals can index into it safely.
	if (ClassRemovals.Num() < Entries.Num())
	{
		ClassRemovals.SetNum(Entries.Num());
	}

	// Load persisted removals and purge stale respawns (elapsed while off) before spawning so
	// removed instances never spawn and expired timers are cleared first.
	LoadRemovals();
	PurgeExpiredRemovals(FDateTime::UtcNow().ToUnixTimestamp());

	// Dedicated servers never load meshes (BuildTemplate skips vis fragments) — spawn immediately.
	UWorld* World = GetWorld();
	if (World != nullptr && World->GetNetMode() == NM_DedicatedServer)
	{
		SpawnEntities();
		SetupTrackerEntity();
		return;
	}

	// Gate spawning on the Client bundle of every referenced config: the bundle load completes
	// only when the soft-referenced meshes/materials are resident, so BuildTemplate never sync-loads.
	TArray<FPrimaryAssetId> PrimaryAssetIds;
	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig == nullptr)
		{
			continue;
		}
		FPrimaryAssetId AssetId = Entry.EntityConfig->GetPrimaryAssetId();
		if (AssetId.IsValid())
		{
			PrimaryAssetIds.AddUnique(AssetId);
		}
		else
		{
			UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::BeginPlay: config '%ls' has no valid PrimaryAssetId — "
					"AssetManager hasn't registered it; bundle gate bypassed for this config.",
				*GetPathNameSafe(Entry.EntityConfig));
		}
	}

	if (PrimaryAssetIds.Num() == 0)
	{
		SpawnEntities();
		SetupTrackerEntity();
		return;
	}

	bPendingBundleSpawn = true;

	TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakSelf(this);
	FStreamableDelegate OnComplete = FStreamableDelegate::CreateLambda(
		[WeakSelf]()
		{
			AArcPlacedEntityPartitionActor* Self = WeakSelf.Get();
			if (Self == nullptr || !Self->bPendingBundleSpawn)
			{
				return;
			}
			UWorld* SelfWorld = Self->GetWorld();
			if (SelfWorld == nullptr || SelfWorld->bIsTearingDown)
			{
				return;
			}
			Self->bPendingBundleSpawn = false;
			Self->SpawnEntities();
			Self->SetupTrackerEntity();
		});

	UAssetManager& AssetManager = UAssetManager::Get();
	ClientBundleHandle = AssetManager.ChangeBundleStateForPrimaryAssets(
		PrimaryAssetIds,
		{ FName(TEXT("Client")) },
		TArray<FName>(),
		/*bRemoveAllBundles*/ false,
		FStreamableDelegate(),
		FStreamableManager::AsyncLoadHighPriority);

	if (!ClientBundleHandle.IsValid() || ClientBundleHandle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnComplete);
	}
	else
	{
		ClientBundleHandle->BindCompleteDelegate(OnComplete);
		// On cancel, spawn anyway — a cancelled load must not permanently suppress placed
		// entities; genuinely missing meshes surface via the trait's Error log.
		ClientBundleHandle->BindCancelDelegate(FStreamableDelegate::CreateLambda(
			[OnComplete]() { OnComplete.ExecuteIfBound(); }));
	}
}

void AArcPlacedEntityPartitionActor::SetupTrackerEntity()
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!ensureMsgf(MassSub != nullptr,
		TEXT("AArcPlacedEntityPartitionActor::SetupTrackerEntity: UMassEntitySubsystem not available when partition actor "
			"%s started — respawn processor will not fire for this cell."), *GetName()))
	{
		return;
	}

	FMassEntityManager& EM = MassSub->GetMutableEntityManager();

	UE::Mass::FEntityBuilder Builder(EM.AsShared());
	Builder.Add_GetRef<FArcPlacedEntityRespawnTrackerFragment>().OwnerPartitionActor = this;
	Builder.Add<FArcPlacedEntityRespawnTrackerTag>();
	TrackerEntity = Builder.Commit();
}

void AArcPlacedEntityPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PartitionActor EndPlay: '%ls' reason=%d spawnedEntities=%d",
		*GetName(), static_cast<int32>(EndPlayReason), SpawnedEntities.Num());

	// Pending bundle completion must not spawn into a world we're leaving.
	bPendingBundleSpawn = false;

	if (TrackerEntity.IsValid())
	{
		UWorld* World = GetWorld();
		UMassEntitySubsystem* MassSub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		if (MassSub != nullptr)
		{
			FMassEntityManager& EM = MassSub->GetMutableEntityManager();
			// Deferred destroy: safe regardless of whether we're inside a Mass phase or not.
			// (EndPlay can fire mid-frame in response to Destroy() called from gameplay code.)
			EM.Defer().DestroyEntity(TrackerEntity);
		}
		TrackerEntity = FMassEntityHandle{};
	}

	SaveRemovals();
	{
		TGuardValue<bool> DespawnGuard(bIsDespawningOnEndPlay, true);
		DespawnEntities();
	}
	Super::EndPlay(EndPlayReason);
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

namespace ArcPlacedEntityPartitionActorPersistence
{

static bool HasAnyRemovals(const TArray<FArcPlacedEntityRemovals>& ClassRemovals, const TArray<FArcPlacedEntityPendingRespawn>& PendingRespawns)
{
	if (PendingRespawns.Num() > 0)
	{
		return true;
	}
	for (const FArcPlacedEntityRemovals& Entry : ClassRemovals)
	{
		if (Entry.RemovedInstances.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

} // namespace ArcPlacedEntityPartitionActorPersistence

IArcPersistenceBackend* AArcPlacedEntityPartitionActor::GetPersistenceBackend() const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return nullptr;
	}
	UArcPersistenceSubsystem* PersistenceSubsystem = GameInstance->GetSubsystem<UArcPersistenceSubsystem>();
	if (PersistenceSubsystem == nullptr)
	{
		return nullptr;
	}
	return PersistenceSubsystem->GetBackend();
}

void AArcPlacedEntityPartitionActor::SaveRemovals()
{
	if (!ArcPlacedEntityPartitionActorPersistence::HasAnyRemovals(ClassRemovals, PendingRespawns))
	{
		return;
	}

	IArcPersistenceBackend* Backend = GetPersistenceBackend();
	if (Backend == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();

	FArcJsonSaveArchive Archive;

	Archive.WriteProperty(FName(TEXT("schema_version")), static_cast<int32>(1));
	Archive.WriteProperty(FName(TEXT("entry_count")), static_cast<int32>(Entries.Num()));

	// Serialize ClassRemovals
	Archive.BeginArray(FName(TEXT("ClassRemovals")), ClassRemovals.Num());
	for (int32 Idx = 0; Idx < ClassRemovals.Num(); ++Idx)
	{
		Archive.BeginArrayElement(Idx);

		const TSet<int32>& RemovedSet = ClassRemovals[Idx].RemovedInstances;
		Archive.BeginArray(FName(TEXT("RemovedInstances")), RemovedSet.Num());
		int32 ElemIdx = 0;
		for (int32 RemovedIdx : RemovedSet)
		{
			Archive.BeginArrayElement(ElemIdx);
			Archive.WriteProperty(FName(TEXT("Index")), RemovedIdx);
			Archive.EndArrayElement();
			++ElemIdx;
		}
		Archive.EndArray();

		Archive.EndArrayElement();
	}
	Archive.EndArray();

	// Serialize PendingRespawns
	Archive.BeginArray(FName(TEXT("PendingRespawns")), PendingRespawns.Num());
	for (int32 Idx = 0; Idx < PendingRespawns.Num(); ++Idx)
	{
		Archive.BeginArrayElement(Idx);
		Archive.WriteProperty(FName(TEXT("RespawnAtUtc")), PendingRespawns[Idx].RespawnAtUtc);
		Archive.WriteProperty(FName(TEXT("ClassIndex")), PendingRespawns[Idx].ClassIndex);
		Archive.WriteProperty(FName(TEXT("TransformIndex")), PendingRespawns[Idx].TransformIndex);
		Archive.EndArrayElement();
	}
	Archive.EndArray();

	TArray<uint8> Data = Archive.Finalize();

	FString StorageKey = FString::Printf(TEXT("worlds/%s/partitions/%s/removals"),
		*World->GetName(), *GetName());
	Backend->SaveEntry(StorageKey, MoveTemp(Data));
}

void AArcPlacedEntityPartitionActor::LoadRemovals()
{
	IArcPersistenceBackend* Backend = GetPersistenceBackend();
	if (Backend == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();

	FString StorageKey = FString::Printf(TEXT("worlds/%s/partitions/%s/removals"),
		*World->GetName(), *GetName());

	FArcPersistenceLoadResult Result = Backend->LoadEntry(StorageKey).Get();

	if (!Result.bSuccess || Result.Data.Num() == 0)
	{
		return;
	}

	FArcJsonLoadArchive Archive;
	if (!Archive.InitializeFromData(Result.Data))
	{
		return;
	}

	int32 SchemaVersion = 0;
	Archive.ReadProperty(FName(TEXT("schema_version")), SchemaVersion);

	int32 EntryCount = INDEX_NONE;
	Archive.ReadProperty(FName(TEXT("entry_count")), EntryCount);

	if (SchemaVersion > 1)
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::LoadRemovals: Unrecognised schema_version %d for '%ls'. Skipping load.",
			SchemaVersion, *GetName());
		return;
	}

	if (EntryCount != INDEX_NONE && EntryCount != Entries.Num())
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::LoadRemovals: entry_count mismatch for '%ls' (saved %d, current %d). Skipping load.",
			*GetName(), EntryCount, Entries.Num());
		return;
	}

	// Read ClassRemovals
	int32 ClassCount = 0;
	if (Archive.BeginArray(FName(TEXT("ClassRemovals")), ClassCount))
	{
		TArray<FArcPlacedEntityRemovals> LoadedRemovals;
		LoadedRemovals.SetNum(ClassCount);

		for (int32 Idx = 0; Idx < ClassCount; ++Idx)
		{
			Archive.BeginArrayElement(Idx);

			int32 RemovedCount = 0;
			if (Archive.BeginArray(FName(TEXT("RemovedInstances")), RemovedCount))
			{
				for (int32 R = 0; R < RemovedCount; ++R)
				{
					Archive.BeginArrayElement(R);
					int32 RemovedIdx = INDEX_NONE;
					Archive.ReadProperty(FName(TEXT("Index")), RemovedIdx);
					if (RemovedIdx != INDEX_NONE)
					{
						LoadedRemovals[Idx].RemovedInstances.Add(RemovedIdx);
					}
					Archive.EndArrayElement();
				}
				Archive.EndArray();
			}

			Archive.EndArrayElement();
		}
		Archive.EndArray();

		ClassRemovals = MoveTemp(LoadedRemovals);

		// Grow to match Entries (parallel-array invariant)
		if (ClassRemovals.Num() < Entries.Num())
		{
			ClassRemovals.SetNum(Entries.Num());
		}
	}

	// Read PendingRespawns
	int32 PendingCount = 0;
	if (Archive.BeginArray(FName(TEXT("PendingRespawns")), PendingCount))
	{
		PendingRespawns.Reserve(PendingCount);
		for (int32 Idx = 0; Idx < PendingCount; ++Idx)
		{
			Archive.BeginArrayElement(Idx);
			FArcPlacedEntityPendingRespawn Entry;
			Archive.ReadProperty(FName(TEXT("RespawnAtUtc")), Entry.RespawnAtUtc);
			Archive.ReadProperty(FName(TEXT("ClassIndex")), Entry.ClassIndex);
			Archive.ReadProperty(FName(TEXT("TransformIndex")), Entry.TransformIndex);
			PendingRespawns.Add(Entry);
			Archive.EndArrayElement();
		}
		Archive.EndArray();
	}
}

void AArcPlacedEntityPartitionActor::SpawnEntities()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	UArcMassSpawnerSubsystem* Spawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (Spawner == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Count total entities to reserve
	int32 TotalCount = 0;
	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig != nullptr)
		{
			TotalCount += Entry.InstanceTransforms.Num();
		}
	}

	if (TotalCount == 0)
	{
		return;
	}

	SpawnedEntities.Reserve(TotalCount);

	// Grow ClassRemovals to be parallel to Entries (populated by LoadRemovals before SpawnEntities is called)
	if (ClassRemovals.Num() < Entries.Num())
	{
		ClassRemovals.SetNum(Entries.Num());
	}

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		if (Entry.EntityConfig == nullptr || Entry.InstanceTransforms.Num() == 0)
		{
			continue;
		}

		UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(Entry.EntityConfig);
		if (ArcCfg == nullptr)
		{
			UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::SpawnEntities: EntityConfig '%ls' is not a UArcMassEntityConfigAsset — skipping",
				*GetNameSafe(Entry.EntityConfig));
			for (int32 TransformIndex = 0; TransformIndex < Entry.InstanceTransforms.Num(); ++TransformIndex)
			{
				SpawnedEntities.Add(FMassEntityHandle{});
			}
			continue;
		}

		const int32 TransformCount = Entry.InstanceTransforms.Num();

		// Count live (non-removed) transforms for this entry
		int32 LiveCount = 0;
		for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
		{
			if (!ClassRemovals[EntryIndex].RemovedInstances.Contains(TransformIndex))
			{
				++LiveCount;
			}
		}

		if (LiveCount == 0)
		{
			// All instances removed — push placeholder handles to preserve flat-index shape
			for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
			{
				SpawnedEntities.Add(FMassEntityHandle{});
			}
			continue;
		}

		for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
		{
			if (ClassRemovals[EntryIndex].RemovedInstances.Contains(TransformIndex))
			{
				// This instance was removed — preserve flat index with a null handle
				SpawnedEntities.Add(FMassEntityHandle{});
				continue;
			}

			const FArcPlacedEntityFragmentOverrides* Overrides = Entry.PerInstanceOverrides.Find(TransformIndex);
			TConstArrayView<FInstancedStruct> OverrideView = (Overrides != nullptr)
				? MakeArrayView(Overrides->FragmentValues)
				: TConstArrayView<FInstancedStruct>{};

			const FTransform Xform = Entry.InstanceTransforms[TransformIndex] * GetActorTransform();
			FMassEntityHandle H = Spawner->SpawnEntityWithOverrides(ArcCfg, Xform, OverrideView);
			ApplyLinkGuidFixup(H, EntityManager);
			if (FArcPlacedEntityOwnerFragment* OwnerFrag =
				EntityManager.GetFragmentDataPtr<FArcPlacedEntityOwnerFragment>(H))
			{
				OwnerFrag->OwnerPartitionActor = this;
				OwnerFrag->EntryIndex          = EntryIndex;
				OwnerFrag->TransformIndex      = TransformIndex;
			}
			SpawnedEntities.Add(H);
			EntityToLocation.Add(H, TPair<int32, int32>(EntryIndex, TransformIndex));
		}
	}
}

void AArcPlacedEntityPartitionActor::ApplyLinkGuidFixup(FMassEntityHandle Entity, FMassEntityManager& EntityManager)
{
	// If entity has both LinkGuid and PersistenceGuid fragments, unify them
	FStructView LinkGuidView = EntityManager.GetFragmentDataStruct(Entity, FArcLinkableGuidFragment::StaticStruct());
	if (LinkGuidView.IsValid())
	{
		const FArcLinkableGuidFragment& LinkGuidFrag = LinkGuidView.Get<const FArcLinkableGuidFragment>();
		if (LinkGuidFrag.LinkGuid.IsValid())
		{
			FStructView PersistView = EntityManager.GetFragmentDataStruct(Entity, FArcMassPersistenceFragment::StaticStruct());
			if (PersistView.IsValid())
			{
				FArcMassPersistenceFragment& PersistFrag = PersistView.Get<FArcMassPersistenceFragment>();
				PersistFrag.PersistenceGuid = LinkGuidFrag.LinkGuid;
			}
		}
	}
}

void AArcPlacedEntityPartitionActor::DespawnEntities()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	TArray<FMassEntityHandle> ValidEntities;
	ValidEntities.Reserve(SpawnedEntities.Num());
	for (const FMassEntityHandle& EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			ValidEntities.Add(EntityHandle);
		}
	}

	if (ValidEntities.Num() > 0)
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PartitionActor DespawnEntities: '%ls' BatchDestroyEntities of %d entities (of %d tracked)",
			*GetName(), ValidEntities.Num(), SpawnedEntities.Num());
		EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	}

	SpawnedEntities.Reset();
	EntityToLocation.Empty();
}

void AArcPlacedEntityPartitionActor::RecordRemovalForRespawn(int32 EntryIndex, int32 TransformIndex)
{
	// EndPlay guard: cell unloads must not record removals — those entities are just despawned, not "removed by gameplay".
	if (bIsDespawningOnEndPlay)
	{
		return;
	}

	// Validate indices
	if (!Entries.IsValidIndex(EntryIndex))
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::MarkInstanceRemoved: EntryIndex %d is out of range (Entries.Num()=%d)",
			EntryIndex, Entries.Num());
		return;
	}
	if (!Entries[EntryIndex].InstanceTransforms.IsValidIndex(TransformIndex))
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::MarkInstanceRemoved: TransformIndex %d is out of range for EntryIndex %d (InstanceTransforms.Num()=%d)",
			TransformIndex, EntryIndex, Entries[EntryIndex].InstanceTransforms.Num());
		return;
	}

	// Grow ClassRemovals to be parallel to Entries if needed (defensive: SpawnEntities does this too,
	// but MarkInstanceRemoved can be called before or after SpawnEntities).
	if (ClassRemovals.Num() < Entries.Num())
	{
		ClassRemovals.SetNum(Entries.Num());
	}

	// Mark the instance as removed (TSet::Add is a no-op if already present)
	ClassRemovals[EntryIndex].RemovedInstances.Add(TransformIndex);

	// Read respawn duration from the entity's const-shared fragment (set by UArcPlacedEntityRespawnTrait).
	// Read BEFORE the deferred-destroy below — once the entity is destroyed, the fragment is gone.
	int32 RespawnTimeSeconds = 0;
	{
		const int32 FlatIndexLookup = ComputeFlatIndex(EntryIndex, TransformIndex);
		if (SpawnedEntities.IsValidIndex(FlatIndexLookup) && SpawnedEntities[FlatIndexLookup].IsValid())
		{
			UWorld* World = GetWorld();
			UMassEntitySubsystem* Sub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
			if (Sub != nullptr)
			{
				const FMassEntityManager& EM = Sub->GetEntityManager();
				const FArcPlacedEntityRespawnConfigFragment* Cfg =
					EM.GetConstSharedFragmentDataPtr<FArcPlacedEntityRespawnConfigFragment>(
						SpawnedEntities[FlatIndexLookup]);
				if (Cfg != nullptr)
				{
					RespawnTimeSeconds = Cfg->RespawnTimeSeconds;
				}
				else
				{
					// Trait not on this EntityConfig → permanent removal. Log once so it's not silent.
					UE_LOGF(LogMass, Warning,
						"AArcPlacedEntityPartitionActor::MarkInstanceRemoved: EntityConfig for slot (EntryIndex=%d, TransformIndex=%d) "
						"has no UArcPlacedEntityRespawnTrait — slot will be permanently removed. "
						"Add the trait to the UMassEntityConfigAsset to enable respawn.",
						EntryIndex, TransformIndex);
				}
			}
		}
	}

	if (RespawnTimeSeconds > 0)
	{
		const int64 NowUtc = FDateTime::UtcNow().ToUnixTimestamp();
		FArcPlacedEntityPendingRespawn Pending;
		Pending.RespawnAtUtc   = NowUtc + static_cast<int64>(RespawnTimeSeconds);
		Pending.ClassIndex     = EntryIndex;
		Pending.TransformIndex = TransformIndex;
		PendingRespawns.Add(Pending);
	}

	// Null the slot in SpawnedEntities and remove from the location map.
	// The handle is captured and the destroy is issued by the caller (MarkInstanceRemoved or the observer).
	const int32 FlatIndex = ComputeFlatIndex(EntryIndex, TransformIndex);
	if (SpawnedEntities.IsValidIndex(FlatIndex) && SpawnedEntities[FlatIndex].IsValid())
	{
		EntityToLocation.Remove(SpawnedEntities[FlatIndex]);
		SpawnedEntities[FlatIndex] = FMassEntityHandle{};
	}
}

void AArcPlacedEntityPartitionActor::MarkInstanceRemoved(int32 EntryIndex, int32 TransformIndex)
{
	// Capture the handle BEFORE RecordRemovalForRespawn nulls the slot.
	FMassEntityHandle Handle;
	if (Entries.IsValidIndex(EntryIndex))
	{
		const int32 FlatIndex = ComputeFlatIndex(EntryIndex, TransformIndex);
		if (SpawnedEntities.IsValidIndex(FlatIndex))
		{
			Handle = SpawnedEntities[FlatIndex];
		}
	}

	RecordRemovalForRespawn(EntryIndex, TransformIndex);

	if (Handle.IsValid())
	{
		UWorld* World = GetWorld();
		UMassEntitySubsystem* Sub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		if (Sub != nullptr)
		{
			Sub->GetMutableEntityManager().Defer().DestroyEntity(Handle);
		}
	}
}

void AArcPlacedEntityPartitionActor::MarkInstanceRemoved(FMassEntityHandle Entity)
{
	const TPair<int32, int32>* Location = EntityToLocation.Find(Entity);
	if (Location == nullptr)
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::MarkInstanceRemoved: Entity (Index=%d SerialNumber=%d) not found in EntityToLocation map",
			Entity.Index, Entity.SerialNumber);
		return;
	}

	const int32 EntryIndex     = Location->Key;
	const int32 TransformIndex = Location->Value;
	MarkInstanceRemoved(EntryIndex, TransformIndex);
}

int32 AArcPlacedEntityPartitionActor::ComputeFlatIndex(int32 EntryIndex, int32 TransformIndex) const
{
	int32 FlatIndex = 0;
	for (int32 i = 0; i < EntryIndex; ++i)
	{
		FlatIndex += Entries[i].InstanceTransforms.Num();
	}
	FlatIndex += TransformIndex;
	return FlatIndex;
}

TArray<FArcPlacedEntityPendingRespawn> AArcPlacedEntityPartitionActor::CollectAndClearExpiredRespawns(int64 NowUtc)
{
	// Pass 1: scan the FULL array (not break-on-first-non-expired — per-class RespawnTimeSeconds
	// can interleave, so expired entries may hide behind non-expired ones).
	TArray<int32> ExpiredIndices;
	TArray<FArcPlacedEntityPendingRespawn> ExpiredCopies;

	for (int32 i = 0; i < PendingRespawns.Num(); ++i)
	{
		if (PendingRespawns[i].RespawnAtUtc <= NowUtc)
		{
			ExpiredIndices.Add(i);
			ExpiredCopies.Add(PendingRespawns[i]);
		}
	}

	// Pass 2: remove in reverse so earlier indices remain valid after each RemoveAt.
	for (int32 i = ExpiredIndices.Num() - 1; i >= 0; --i)
	{
		PendingRespawns.RemoveAt(ExpiredIndices[i]);
	}

	// Clear each expired entry from ClassRemovals so SpawnEntities / RestoreInstance will re-spawn it.
	for (const FArcPlacedEntityPendingRespawn& Entry : ExpiredCopies)
	{
		if (ClassRemovals.IsValidIndex(Entry.ClassIndex))
		{
			ClassRemovals[Entry.ClassIndex].RemovedInstances.Remove(Entry.TransformIndex);
		}
	}

	return ExpiredCopies;
}

void AArcPlacedEntityPartitionActor::RestoreExpiredRemovals(int64 NowUtc)
{
	if (bIsDespawningOnEndPlay)
	{
		return;
	}
	TArray<FArcPlacedEntityPendingRespawn> Expired = CollectAndClearExpiredRespawns(NowUtc);
	for (const FArcPlacedEntityPendingRespawn& Entry : Expired)
	{
		RestoreInstance(Entry.ClassIndex, Entry.TransformIndex);
	}
}

void AArcPlacedEntityPartitionActor::PurgeExpiredRemovals(int64 NowUtc)
{
	// Discard the result — the subsequent SpawnEntities call will spawn unblocked instances naturally.
	CollectAndClearExpiredRespawns(NowUtc);
}

void AArcPlacedEntityPartitionActor::RestoreInstance(int32 EntryIndex, int32 TransformIndex)
{
	// Validate entry and transform indices
	if (!Entries.IsValidIndex(EntryIndex))
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: EntryIndex %d is out of range (Entries.Num()=%d)",
			EntryIndex, Entries.Num());
		return;
	}

	const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
	if (Entry.EntityConfig == nullptr)
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: EntryIndex %d has null EntityConfig",
			EntryIndex);
		return;
	}

	if (!Entry.InstanceTransforms.IsValidIndex(TransformIndex))
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: TransformIndex %d is out of range for EntryIndex %d (InstanceTransforms.Num()=%d)",
			TransformIndex, EntryIndex, Entry.InstanceTransforms.Num());
		return;
	}

	UWorld* World = GetWorld();
	UArcMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UArcMassSpawnerSubsystem>() : nullptr;
	if (Spawner == nullptr)
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: No UArcMassSpawnerSubsystem available");
		return;
	}

	UMassEntitySubsystem* Sub = World->GetSubsystem<UMassEntitySubsystem>();
	if (Sub == nullptr)
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: No UMassEntitySubsystem available");
		return;
	}

	FMassEntityManager& EntityManager = Sub->GetMutableEntityManager();

	UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(Entry.EntityConfig);
	if (ArcCfg == nullptr)
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: EntityConfig '%ls' is not a UArcMassEntityConfigAsset at EntryIndex %d",
			*GetNameSafe(Entry.EntityConfig), EntryIndex);
		return;
	}

	const FArcPlacedEntityFragmentOverrides* Overrides = Entry.PerInstanceOverrides.Find(TransformIndex);
	TConstArrayView<FInstancedStruct> OverrideView = (Overrides != nullptr)
		? MakeArrayView(Overrides->FragmentValues)
		: TConstArrayView<FInstancedStruct>{};

	const FTransform Xform = Entry.InstanceTransforms[TransformIndex] * GetActorTransform();
	FMassEntityHandle Handle = Spawner->SpawnEntityWithOverrides(ArcCfg, Xform, OverrideView);
	if (!Handle.IsValid())
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: SpawnEntityWithOverrides returned invalid handle for EntryIndex %d TransformIndex %d",
			EntryIndex, TransformIndex);
		return;
	}

	ApplyLinkGuidFixup(Handle, EntityManager);
	if (FArcPlacedEntityOwnerFragment* OwnerFrag =
		EntityManager.GetFragmentDataPtr<FArcPlacedEntityOwnerFragment>(Handle))
	{
		OwnerFrag->OwnerPartitionActor = this;
		OwnerFrag->EntryIndex          = EntryIndex;
		OwnerFrag->TransformIndex      = TransformIndex;
	}

	// Write handle into the flat SpawnedEntities slot (MarkInstanceRemoved nulled it earlier)
	const int32 FlatIndex = ComputeFlatIndex(EntryIndex, TransformIndex);
	if (SpawnedEntities.IsValidIndex(FlatIndex))
	{
		SpawnedEntities[FlatIndex] = Handle;
	}
	else
	{
		UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor::RestoreInstance: FlatIndex %d is out of range (SpawnedEntities.Num()=%d) for EntryIndex %d TransformIndex %d",
			FlatIndex, SpawnedEntities.Num(), EntryIndex, TransformIndex);
	}

	EntityToLocation.Add(Handle, TPair<int32, int32>(EntryIndex, TransformIndex));
}

// -----------------------------------------------------------------------------
// File-scoped helpers (componentless path)
// -----------------------------------------------------------------------------

#if WITH_EDITOR
namespace ArcPlacedEntityPartitionActor_Componentless
{

static uint32 ComputeConfigHash(const FArcVisualizationMeshFragment& Vis, const FArcOverrideMaterialsFragment* OverrideMats)
{
	uint32 Hash = 0;
	Hash = HashCombine(Hash, GetTypeHash(static_cast<int64>(Vis.CustomDepthStencilValue)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<int32>(Vis.TranslucencySortPriority)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.HiddenInGame)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.CastShadow)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.CastShadowAsTwoSided)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.CastHiddenShadow)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.AffectDynamicIndirectLighting)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.AffectIndirectLightingWhileHidden)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.ReceivesDecals)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.NeverDistanceCull)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.CastFarShadow)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.CastInsetShadow)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.CastContactShadow)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.RenderCustomDepth)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.VisibleInReflections)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.VisibleInRayTracing)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.VisibleInReflectionCaptures)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.LightingChannels.bChannel0)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.LightingChannels.bChannel1)));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(Vis.LightingChannels.bChannel2)));
	Hash = HashCombine(Hash, GetTypeHash(Vis.MinDrawDistance));
	Hash = HashCombine(Hash, GetTypeHash(Vis.LDMaxDrawDistance));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Vis.CustomDepthStencilWriteMask)));

	if (OverrideMats)
	{
		for (const TObjectPtr<UMaterialInterface>& MatPtr : OverrideMats->OverrideMaterials)
		{
			Hash = HashCombine(Hash, GetTypeHash(MatPtr.Get()));
		}
	}
	else
	{
		Hash = HashCombine(Hash, 0u);
	}

	return Hash;
}

static const UArcUnifiedMeshVisualizationTrait* FindUnifiedTrait(const FArcPlacedEntityEntry& Entry)
{
	if (Entry.EntityConfig == nullptr)
	{
		return nullptr;
	}
	return Cast<UArcUnifiedMeshVisualizationTrait>(
		Entry.EntityConfig->GetConfig().FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
}

static void BuildVisFragmentFromProps(const FArcMassVisRenderingProps& Props, FArcMassVisualizationMeshConfigFragment& Out)
{
	Out.CastShadow                       = Props.bCastShadow;
	Out.CastShadowAsTwoSided             = Props.bCastShadowAsTwoSided;
	Out.CastHiddenShadow                 = Props.bCastHiddenShadow;
	Out.CastFarShadow                    = Props.bCastFarShadow;
	Out.CastInsetShadow                  = Props.bCastInsetShadow;
	Out.CastContactShadow                = Props.bCastContactShadow;
	Out.HiddenInGame                     = Props.bHiddenInGame;
	Out.ReceivesDecals                   = Props.bReceivesDecals;
	Out.NeverDistanceCull                = Props.bNeverDistanceCull;
	Out.RenderCustomDepth                = Props.bRenderCustomDepth;
	Out.CustomDepthStencilValue          = Props.CustomDepthStencilValue;
	Out.TranslucencySortPriority         = Props.TranslucencySortPriority;
	Out.AffectDynamicIndirectLighting    = Props.bAffectDynamicIndirectLighting;
	Out.AffectIndirectLightingWhileHidden = Props.bAffectIndirectLightingWhileHidden;
	Out.LightingChannels                 = Props.LightingChannels;
	Out.VisibleInReflections             = Props.bVisibleInReflections;
	Out.VisibleInRayTracing              = Props.bVisibleInRayTracing;
	Out.VisibleInReflectionCaptures      = Props.bVisibleInReflectionCaptures;
	Out.MinDrawDistance                  = Props.MinDrawDistance;
	Out.LDMaxDrawDistance                = Props.LDMaxDrawDistance;
	Out.CustomDepthStencilWriteMask      = Props.CustomDepthStencilWriteMask;
}

} // namespace ArcPlacedEntityPartitionActor_Componentless
#endif // WITH_EDITOR

// -----------------------------------------------------------------------------
// IArcMassEntityManagerProvider
// -----------------------------------------------------------------------------

IArcMassEntityManager* AArcPlacedEntityPartitionActor::GetMassEntityManager(const FArcMassEntityElementId& InId)
{
	return (InId.PartitionActor.Get() == this) ? this : nullptr;
}

// -----------------------------------------------------------------------------
// IArcMassEntityManager — non-editor stubs
// -----------------------------------------------------------------------------

bool AArcPlacedEntityPartitionActor::CanEditMassEntityInstance(const FArcMassEntityElementId& InId) const
{
#if WITH_EDITOR
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	return ResolveElementId(InId, E, T);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::CanMoveMassEntityInstance(const FArcMassEntityElementId& InId, ETypedElementWorldType WorldType) const
{
#if WITH_EDITOR
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	return ResolveElementId(InId, E, T);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::GetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, FTransform& OutTransform, bool bWorldSpace) const
{
#if WITH_EDITOR
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	if (!ResolveElementId(InId, E, T))
	{
		return false;
	}
	const FTransform Local = Entries[E].InstanceTransforms[T];
	const FTransform Comp = GetEntryComponentTransform(Entries[E]);
	OutTransform = bWorldSpace ? Comp * (Local * GetActorTransform()) : Comp * Local;
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::SetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, const FTransform& InTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
#if WITH_EDITOR
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	if (!ResolveElementId(InId, E, T))
	{
		return false;
	}
	const FTransform Comp = GetEntryComponentTransform(Entries[E]);
	const FTransform Stripped = Comp.GetRelativeTransformReverse(InTransform);
	Entries[E].InstanceTransforms[T] = bWorldSpace ? Stripped.GetRelativeTransform(GetActorTransform()) : Stripped;
	const FTransform WorldXform = Comp * (Entries[E].InstanceTransforms[T] * GetActorTransform());
	UpdateInstanceRender(E, T, WorldXform);
	FArcPlacedEntityEditorEvents::OnInstanceTransformChanged.Broadcast();
	return true;
#else
	return false;
#endif
}

void AArcPlacedEntityPartitionActor::NotifyMassEntityInstanceMovementStarted(const FArcMassEntityElementId& InId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifyMassEntityInstanceMovementOngoing(const FArcMassEntityElementId& InId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifyMassEntityInstanceMovementEnded(const FArcMassEntityElementId& InId)
{
#if WITH_EDITOR
	Modify();
#endif
}

void AArcPlacedEntityPartitionActor::NotifyMassEntityInstanceSelectionChanged(const FArcMassEntityElementId& InId, bool bIsSelected)
{
	// No-op — selection highlight is not yet implemented for the componentless path
}

bool AArcPlacedEntityPartitionActor::DeleteMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds)
{
#if WITH_EDITOR
	// Pass 1: resolve and group by entry
	TMap<int32, TArray<int32>> ByEntry;
	for (const FArcMassEntityElementId& Id : InIds)
	{
		int32 E = INDEX_NONE;
		int32 T = INDEX_NONE;
		if (ResolveElementId(Id, E, T))
		{
			ByEntry.FindOrAdd(E).Add(T);
		}
	}

	if (ByEntry.IsEmpty())
	{
		return false;
	}

	Modify();

	// Ordering contract: deselect+destroy ALL handles while Entries still holds pre-delete indices.
	TearDownEditorRenderState();
	DestroyEditorPreviewISMCs();

	// Pass 2: remove transforms back-to-front per entry, then re-index overrides
	for (TPair<int32, TArray<int32>>& Pair : ByEntry)
	{
		const int32 EntryIdx = Pair.Key;
		TArray<int32>& Indices = Pair.Value;
		Indices.Sort(TGreater<int32>());

		FArcPlacedEntityEntry& Entry = Entries[EntryIdx];

		for (const int32 T : Indices)
		{
			Entry.PerInstanceOverrides.Remove(T);
			Entry.InstanceTransforms.RemoveAt(T);
		}

		// Re-index surviving override keys
		TArray<TTuple<int32, FArcPlacedEntityFragmentOverrides>> ReindexedPairs;
		ReindexedPairs.Reserve(Entry.PerInstanceOverrides.Num());
		for (TPair<int32, FArcPlacedEntityFragmentOverrides>& OverridePair : Entry.PerInstanceOverrides)
		{
			const int32 OldKey = OverridePair.Key;
			int32 Shift = 0;
			for (const int32 Removed : Indices)
			{
				if (Removed < OldKey)
				{
					++Shift;
				}
			}
			ReindexedPairs.Emplace(OldKey - Shift, MoveTemp(OverridePair.Value));
		}
		Entry.PerInstanceOverrides.Reset();
		for (TTuple<int32, FArcPlacedEntityFragmentOverrides>& ReindexedPair : ReindexedPairs)
		{
			Entry.PerInstanceOverrides.Emplace(ReindexedPair.Get<0>(), MoveTemp(ReindexedPair.Get<1>()));
		}
	}

	// Prune empty entries (back to front)
	for (int32 EntryIdx = Entries.Num() - 1; EntryIdx >= 0; --EntryIdx)
	{
		if (Entries[EntryIdx].InstanceTransforms.Num() == 0)
		{
			Entries.RemoveAt(EntryIdx);
		}
	}

	if (Entries.Num() == 0)
	{
		UEditorActorSubsystem* EditorActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
		if (EditorActorSubsystem != nullptr)
		{
			EditorActorSubsystem->DestroyActor(this);
		}
		return true;
	}

	RebuildGuidIndex();
	RebuildEditorRenderState();
	RebuildEditorPreviewISMCs();
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::DuplicateMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds, TArray<FArcMassEntityElementId>& OutNewIds)
{
#if WITH_EDITOR
	// Pass 1: resolve and group by entry
	TMap<int32, TArray<int32>> ByEntry;
	for (const FArcMassEntityElementId& Id : InIds)
	{
		int32 E = INDEX_NONE;
		int32 T = INDEX_NONE;
		if (ResolveElementId(Id, E, T))
		{
			ByEntry.FindOrAdd(E).Add(T);
		}
	}

	if (ByEntry.IsEmpty())
	{
		return false;
	}

	Modify();

	// Teardown first: indices are about to change, all handles must be deselected and destroyed
	TearDownEditorRenderState();
	DestroyEditorPreviewISMCs();

	// Pass 2: copy transforms per entry, regenerate LinkGuid where present
	for (TPair<int32, TArray<int32>>& Pair : ByEntry)
	{
		const int32 E = Pair.Key;
		const TArray<int32>& SrcIndices = Pair.Value;
		FArcPlacedEntityEntry& Entry = Entries[E];

		for (const int32 SrcT : SrcIndices)
		{
			const FTransform Copy = Entry.InstanceTransforms[SrcT];
			const int32 NewT = Entry.InstanceTransforms.Add(Copy);

			// Copy overrides if present
			const FArcPlacedEntityFragmentOverrides* SrcOverrides = Entry.PerInstanceOverrides.Find(SrcT);
			if (SrcOverrides != nullptr)
			{
				Entry.PerInstanceOverrides.Add(NewT, *SrcOverrides);

				FArcPlacedEntityFragmentOverrides* NewOverrides = Entry.PerInstanceOverrides.Find(NewT);
				if (NewOverrides != nullptr)
				{
					for (FInstancedStruct& FragmentValue : NewOverrides->FragmentValues)
					{
						if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
						{
							FArcLinkableGuidFragment& GuidFrag = FragmentValue.GetMutable<FArcLinkableGuidFragment>();
							GuidFrag.LinkGuid = FGuid::NewGuid();
							break;
						}
					}
				}
			}

			AutoAssignLinkGuidIfNeeded(Entry, NewT);

			FArcMassEntityElementId NewId;
			NewId.PartitionActor = this;
			NewId.EntryIndex = E;
			NewId.TransformIndex = NewT;
			OutNewIds.Add(NewId);
		}
	}

	RebuildGuidIndex();
	RebuildEditorRenderState();
	RebuildEditorPreviewISMCs();
	return OutNewIds.Num() > 0;
#else
	return false;
#endif
}

void AArcPlacedEntityPartitionActor::BeginDestroy()
{
	// EndPlay is not guaranteed on every destruction path (editor undo + GC, world teardown);
	// the weak ptr in the bundle-completion lambda resolves non-null until RF_BeginDestroyed.
	bPendingBundleSpawn = false;

#if WITH_EDITOR
	if (PendingRenderRebuildTickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PendingRenderRebuildTickHandle);
		PendingRenderRebuildTickHandle.Reset();
	}
	if (GEditor && !GExitPurge)
	{
		TearDownEditorRenderState();
	}
	UE::Editor::PIE::OnPreBegin.Remove(PreBeginPIEHandle);
	UE::Editor::PIE::OnEnd.Remove(EndPIEHandle);
	UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
	MaterialCompilationFinishedHandle.Reset();

	// TEMP DIAGNOSTIC: unsubscribe package-dirty callstack capture.
	if (PackageDirtyDiagHandle.IsValid())
	{
		UPackage::PackageMarkedDirtyEvent.Remove(PackageDirtyDiagHandle);
		PackageDirtyDiagHandle.Reset();
	}

	OnEditorStateChanged.Broadcast(this, EArcPlacedEntityOutlinerEvent::Destroyed);
#endif
	Super::BeginDestroy();
}

// -----------------------------------------------------------------------------
// Editor
// -----------------------------------------------------------------------------

#if WITH_EDITOR

uint32 AArcPlacedEntityPartitionActor::GetDefaultGridSize(UWorld* InWorld) const
{
	return 25600;
}

AActor* AArcPlacedEntityPartitionActor::GetSceneOutlinerParent() const
{
	if (AActor* Parent = OutlinerParent.Get())
	{
		return Parent;
	}
	return Super::GetSceneOutlinerParent();
}

void AArcPlacedEntityPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		// TEMP DIAGNOSTIC: catch dirtying that originates after load completes (this runs post-registration).
		if (!PackageDirtyDiagHandle.IsValid())
		{
			PackageDirtyDiagHandle = UPackage::PackageMarkedDirtyEvent.AddWeakLambda(this,
				[this](UPackage* Package, bool bWasDirty)
				{
					if (Package != nullptr && Package == GetPackage())
					{
						ArcPlacedEntityPartitionActor_DirtyDiag::DumpDirtyCallstack(Package, bWasDirty, this);
					}
				});
		}
		UE_LOGF(LogTemp, Warning,"[PlacedDirty] PostRegisterAllComponents ENTER Actor='%ls' IsDirty=%d",
			*GetName(), GetPackage() && GetPackage()->IsDirty() ? 1 : 0);

		RebuildEditorPreviewISMCs();
		UE_LOGF(LogTemp, Warning,"[PlacedDirty]   after RebuildEditorPreviewISMCs   IsDirty=%d",
			GetPackage() && GetPackage()->IsDirty() ? 1 : 0);

		RebuildEditorRenderState();
		UE_LOGF(LogTemp, Warning,"[PlacedDirty]   after RebuildEditorRenderState    IsDirty=%d",
			GetPackage() && GetPackage()->IsDirty() ? 1 : 0);

		UE::Editor::PIE::OnPreBegin.Remove(PreBeginPIEHandle);
		UE::Editor::PIE::OnEnd.Remove(EndPIEHandle);
		PreBeginPIEHandle = UE::Editor::PIE::OnPreBegin.AddUObject(this, &AArcPlacedEntityPartitionActor::OnPreBeginPIE);
		EndPIEHandle = UE::Editor::PIE::OnEnd.AddUObject(this, &AArcPlacedEntityPartitionActor::OnEndPIE);

		UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
		MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddWeakLambda(
			this, [this](UMaterialInterface* Material) { OnMaterialCompilationFinished(Material); });
	}
}

void AArcPlacedEntityPartitionActor::OnPreBeginPIE(const bool bIsSimulating)
{
	TearDownEditorRenderState();
	DestroyEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::OnEndPIE(const bool bIsSimulating)
{
	RebuildEditorPreviewISMCs();
	RebuildEditorRenderState();
}

void AArcPlacedEntityPartitionActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();
	RebuildEditorRenderState();
}

void AArcPlacedEntityPartitionActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	FArcPlacedEntityEditorEvents::OnInstanceTransformChanged.Broadcast();
}

void AArcPlacedEntityPartitionActor::PostLoad()
{
	Super::PostLoad();

	for (FArcPlacedEntityEntry& Entry : Entries)
	{
		for (int32 i = 0; i < Entry.InstanceTransforms.Num(); ++i)
		{
			AutoAssignLinkGuidIfNeeded(Entry, i);
		}
	}
	RebuildGuidIndex();

#if WITH_EDITOR
	// TEMP DIAGNOSTIC: subscribe as early as possible so we catch any later MarkPackageDirty on this actor.
	if (!PackageDirtyDiagHandle.IsValid())
	{
		PackageDirtyDiagHandle = UPackage::PackageMarkedDirtyEvent.AddWeakLambda(this,
			[this](UPackage* Package, bool bWasDirty)
			{
				if (Package != nullptr && Package == GetPackage())
				{
					ArcPlacedEntityPartitionActor_DirtyDiag::DumpDirtyCallstack(Package, bWasDirty, this);
				}
			});
	}
	UE_LOGF(LogTemp, Warning,"[PlacedDirty] PostLoad ENTER  Actor='%ls' Package='%ls' IsDirty=%d",
		*GetName(), GetPackage() ? *GetPackage()->GetName() : TEXT("<null>"), GetPackage() && GetPackage()->IsDirty() ? 1 : 0);

	// PIE worlds report IsGameWorld()==true and don't need previews — SpawnEntities() creates real entities at BeginPlay.
	if (UWorld* W = GetWorld(); W != nullptr && !W->IsGameWorld())
	{
		RebuildEditorPreviewISMCs();
		RebuildEditorRenderState();
	}

	UE_LOGF(LogTemp, Warning,"[PlacedDirty] PostLoad EXIT   Actor='%ls' IsDirty=%d",
		*GetName(), GetPackage() && GetPackage()->IsDirty() ? 1 : 0);
#endif
}

void AArcPlacedEntityPartitionActor::AutoAssignLinkGuidIfNeeded(FArcPlacedEntityEntry& Entry, int32 InstanceIndex)
{
	if (Entry.EntityConfig == nullptr)
	{
		return;
	}

	const UMassEntityTraitBase* LinkableTrait = Entry.EntityConfig->GetConfig().FindTrait(UArcLinkableGuidTrait::StaticClass());
	if (LinkableTrait == nullptr)
	{
		return;
	}

	FArcPlacedEntityFragmentOverrides& Overrides = Entry.PerInstanceOverrides.FindOrAdd(InstanceIndex);

	// Check if a LinkGuid override already exists
	for (FInstancedStruct& FragmentValue : Overrides.FragmentValues)
	{
		if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
		{
			return;
		}
	}

	FArcLinkableGuidFragment NewGuidFrag;
	NewGuidFrag.LinkGuid = FGuid::NewGuid();
	Overrides.FragmentValues.Add(FInstancedStruct::Make(NewGuidFrag));
}

void AArcPlacedEntityPartitionActor::AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex)
{
	OutEntryIndex = INDEX_NONE;
	OutInstanceIndex = INDEX_NONE;

	if (ConfigAsset == nullptr)
	{
		return;
	}

	Modify();

	// Find existing entry for this config
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		if (Entry.EntityConfig == ConfigAsset)
		{
			OutInstanceIndex = Entry.InstanceTransforms.Add(Transform);
			AutoAssignLinkGuidIfNeeded(Entry, OutInstanceIndex);
			OutEntryIndex = EntryIndex;
			RebuildGuidIndex();
			RebuildAllEditorState();
			return;
		}
	}

	// Create new entry
	OutEntryIndex = Entries.Num();
	FArcPlacedEntityEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EntityConfig = ConfigAsset;
	OutInstanceIndex = NewEntry.InstanceTransforms.Add(Transform);
	AutoAssignLinkGuidIfNeeded(NewEntry, OutInstanceIndex);
	RebuildGuidIndex();
	RebuildAllEditorState();
}

int32 AArcPlacedEntityPartitionActor::AddInstances(UMassEntityConfigAsset* ConfigAsset, const FGuid& OwnerGuid, TConstArrayView<FTransform> Transforms)
{
	if (ConfigAsset == nullptr || Transforms.IsEmpty())
	{
		return INDEX_NONE;
	}

	Modify();

	int32 EntryIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].EntityConfig == ConfigAsset && Entries[Index].OwnerGuid == OwnerGuid)
		{
			EntryIndex = Index;
			break;
		}
	}

	if (EntryIndex == INDEX_NONE)
	{
		EntryIndex = Entries.Num();
		FArcPlacedEntityEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.EntityConfig = ConfigAsset;
		NewEntry.OwnerGuid = OwnerGuid;
	}

	FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
	Entry.InstanceTransforms.Reserve(Entry.InstanceTransforms.Num() + Transforms.Num());
	for (const FTransform& Transform : Transforms)
	{
		const int32 InstanceIndex = Entry.InstanceTransforms.Add(Transform);
		AutoAssignLinkGuidIfNeeded(Entry, InstanceIndex);
	}

	// Both rebuilds are deferred to one pass: AutoAssignLinkGuidIfNeeded never reads GuidToInstanceMap,
	// and RebuildGuidIndex is a pure re-derivation of it.
	RebuildGuidIndex();
	RebuildAllEditorState();

	return EntryIndex;
}

void AArcPlacedEntityPartitionActor::RebuildEditorPreviewISMCs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcPlacedRebuildEditorPreviewISMCs);

	if (bIsRebuildingPreviewISMCs)
	{
		return;
	}
	TGuardValue<bool> RebuildGuard(bIsRebuildingPreviewISMCs, true);

	DestroyEditorPreviewISMCs();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups.AddDefaulted_GetRef();

		if (Entry.EntityConfig == nullptr)
		{
			continue;
		}

		// Static-mesh-only entries use the componentless MassEntity path — skip ISMC creation for them.
		if (IsComponentlessEntry(Entry))
		{
			// No ISM component built for componentless entries; RebuildEditorRenderState handles them.
			continue;
		}

		if (IsCompositeComponentlessEntry(Entry))
		{
			// Composite is rendered by the componentless holder path (RebuildEditorRenderState) now.
			continue;
		}

		// Try Niagara visualization trait
		const UArcNiagaraVisTrait* NiagaraTrait = Cast<UArcNiagaraVisTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcNiagaraVisTrait::StaticClass()));
		if (NiagaraTrait != nullptr && NiagaraTrait->IsValid())
		{
			const FArcNiagaraVisConfigFragment& NiagaraConfig = NiagaraTrait->GetNiagaraConfig();
			UNiagaraSystem* NiagaraSystem = NiagaraConfig.NiagaraSystem.LoadSynchronous();
			if (NiagaraSystem != nullptr)
			{
				const FTransform& ComponentTransform = NiagaraTrait->GetComponentTransform();
				Group.NiagaraComponentTransform = ComponentTransform;

				for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
				{
					UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(this);
					NiagaraComp->bIsEditorOnly = true;
					NiagaraComp->SetIsVisualizationComponent(true);
					NiagaraComp->SetAsset(NiagaraSystem);
					NiagaraComp->SetupAttachment(GetRootComponent());
					FTransform FinalTransform = ComponentTransform * InstanceTransform;
					NiagaraComp->SetRelativeTransform(FinalTransform);
					NiagaraComp->RegisterComponent();
					NiagaraComp->Activate(true);

					Group.NiagaraComponents.Add(NiagaraComp);
				}
			}
		}

	}

}

void AArcPlacedEntityPartitionActor::DestroyEditorPreviewISMCs()
{
	for (FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
	{
		for (UNiagaraComponent* NiagaraComp : Group.NiagaraComponents)
		{
			if (NiagaraComp != nullptr)
			{
				NiagaraComp->Deactivate();
				NiagaraComp->DestroyComponent();
			}
		}
		Group.NiagaraComponents.Empty();
	}

	TArray<UInstancedStaticMeshComponent*> ISMComponents;
	GetComponents(ISMComponents);
	for (UInstancedStaticMeshComponent* ISMComponent : ISMComponents)
	{
		ISMComponent->DestroyComponent();
	}

	EditorPreviewGroups.Empty();
}

void AArcPlacedEntityPartitionActor::RebuildGuidIndex()
{
	GuidToInstanceMap.Reset();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		for (const TPair<int32, FArcPlacedEntityFragmentOverrides>& OverridePair : Entry.PerInstanceOverrides)
		{
			for (const FInstancedStruct& FragmentValue : OverridePair.Value.FragmentValues)
			{
				if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
				{
					const FArcLinkableGuidFragment& GuidFrag = FragmentValue.Get<const FArcLinkableGuidFragment>();
					if (GuidFrag.LinkGuid.IsValid())
					{
						GuidToInstanceMap.Add(GuidFrag.LinkGuid, TPair<int32, int32>(EntryIndex, OverridePair.Key));
					}
					break;
				}
			}
		}
	}
}

bool AArcPlacedEntityPartitionActor::FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	const TPair<int32, int32>* Found = GuidToInstanceMap.Find(Guid);
	if (Found == nullptr)
	{
		return false;
	}
	OutEntryIndex = Found->Key;
	OutTransformIndex = Found->Value;
	return true;
}

bool AArcPlacedEntityPartitionActor::RemoveEntriesOwnedBy(const FGuid& OwnerGuid)
{
	if (!OwnerGuid.IsValid())
	{
		return Entries.Num() == 0;
	}

	// Pass 1: collect indices of entries owned by OwnerGuid.
	TArray<int32> IndicesToRemove;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].OwnerGuid == OwnerGuid)
		{
			IndicesToRemove.Add(i);
		}
	}

	// Pass 2: remove in reverse order so earlier indices stay valid.
	for (int32 ReverseIdx = IndicesToRemove.Num() - 1; ReverseIdx >= 0; --ReverseIdx)
	{
		Entries.RemoveAt(IndicesToRemove[ReverseIdx]);
	}

	Modify();
	MarkPackageDirty();
	if (Entries.Num() > 0)
	{
		// RebuildEditorRenderState() tears down (componentless source entities + holders in the
		// shared Mass manager) as its first step, then rebuilds from the surviving entries.
		// RebuildEditorPreviewISMCs() likewise destroys old ISMCs internally before rebuilding.
		RebuildEditorRenderState();
		RebuildEditorPreviewISMCs();
	}
	else
	{
		// Nothing to rebuild — the caller will DestroyActor. Tear down eagerly so the actor leaves
		// no orphaned componentless source entities / holders in the shared Mass manager
		// (GC-deferred BeginDestroy is too late: a re-populated actor in the same cell may rebuild first).
		TearDownEditorRenderState();
		DestroyEditorPreviewISMCs();
	}
	// GuidToInstanceMap was left stale by the RemoveAt above; rebuild it last (matching the
	// DeleteMassEntityInstances / DuplicateMassEntityInstances ordering).
	RebuildGuidIndex();

	return Entries.Num() == 0;
}

bool AArcPlacedEntityPartitionActor::HasHandPlacedEntries() const
{
	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (!Entry.OwnerGuid.IsValid())
		{
			return true;
		}
	}
	return false;
}

// -----------------------------------------------------------------------------
// Componentless editor render state (Steps 2–5)
// -----------------------------------------------------------------------------

FMassEntityManager* AArcPlacedEntityPartitionActor::GetEditorEntityManager() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	UMassEntitySubsystem* S = W->GetSubsystem<UMassEntitySubsystem>();
	return S ? &S->GetMutableEntityManager() : nullptr;
}

bool AArcPlacedEntityPartitionActor::IsComponentlessEntry(const FArcPlacedEntityEntry& Entry) const
{
	if (Entry.EntityConfig == nullptr)
	{
		return false;
	}

	const FMassEntityConfig& Cfg = Entry.EntityConfig->GetConfig();

	const UArcUnifiedMeshVisualizationTrait* Unified = Cast<UArcUnifiedMeshVisualizationTrait>(
		Cfg.FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	if (Unified == nullptr)
	{
		return false;
	}
	if (Cfg.FindTrait(UArcNiagaraVisTrait::StaticClass()) != nullptr)
	{
		return false;
	}
	return Unified->GetInferredMode() == EArcUnifiedMeshVisMode::Static;
}

bool AArcPlacedEntityPartitionActor::IsSkinnedComponentlessEntry(const FArcPlacedEntityEntry& Entry) const
{
	if (Entry.EntityConfig == nullptr)
	{
		return false;
	}

	const FMassEntityConfig& Cfg = Entry.EntityConfig->GetConfig();

	const UArcUnifiedMeshVisualizationTrait* Unified = Cast<UArcUnifiedMeshVisualizationTrait>(
		Cfg.FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	if (Unified == nullptr)
	{
		return false;
	}
	if (Cfg.FindTrait(UArcNiagaraVisTrait::StaticClass()) != nullptr)
	{
		return false;
	}
	return Unified->GetInferredMode() == EArcUnifiedMeshVisMode::Skinned;
}

bool AArcPlacedEntityPartitionActor::IsCompositeComponentlessEntry(const FArcPlacedEntityEntry& Entry) const
{
	if (Entry.EntityConfig == nullptr)
	{
		return false;
	}

	const FMassEntityConfig& Cfg = Entry.EntityConfig->GetConfig();

	const UArcUnifiedMeshVisualizationTrait* Unified = Cast<UArcUnifiedMeshVisualizationTrait>(
		Cfg.FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	if (Unified == nullptr)
	{
		return false;
	}
	// Unlike the static and skinned classifiers, a co-authored Niagara trait does not disqualify composite.
	return Unified->GetInferredMode() == EArcUnifiedMeshVisMode::Composite;
}

#if WITH_EDITOR
FTransform AArcPlacedEntityPartitionActor::GetEntryComponentTransform(const FArcPlacedEntityEntry& Entry) const
{
	// Returns the per-entry pivot offset that RebuildEditorRenderState pre-multiplies into the
	// instance transform, so the gizmo pivot matches the visible mesh.
	// Degenerate (single-entry) modes return that entry's RelativeTransform;
	// composite mode returns identity (per-entry transforms are baked per sub-mesh, the gizmo
	// moves the whole entity).
	const UArcUnifiedMeshVisualizationTrait* Unified =
		ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
	if (Unified == nullptr)
	{
		return FTransform::Identity;
	}

	const EArcUnifiedMeshVisMode Mode = Unified->GetInferredMode();
	if (Mode == EArcUnifiedMeshVisMode::Static || Mode == EArcUnifiedMeshVisMode::Skinned)
	{
		return Unified->GetSingleRelativeTransform();
	}
	return FTransform::Identity;
}
#endif // WITH_EDITOR

bool AArcPlacedEntityPartitionActor::ResolveElementId(const FArcMassEntityElementId& InId, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	if (InId.PartitionActor.Get() != this)
	{
		return false;
	}
	if (!Entries.IsValidIndex(InId.EntryIndex))
	{
		return false;
	}
	if (!Entries[InId.EntryIndex].InstanceTransforms.IsValidIndex(InId.TransformIndex))
	{
		return false;
	}
	OutEntryIndex = InId.EntryIndex;
	OutTransformIndex = InId.TransformIndex;
	return true;
}

bool AArcPlacedEntityPartitionActor::UpdateInstanceRender(int32 EntryIndex, int32 TransformIndex, const FTransform& WorldTransform)
{
	FArcComponentlessInstanceInfo* Info = InstanceToSourceEntity.Find(TPair<int32, int32>(EntryIndex, TransformIndex));
	if (!Info)
	{
		return false;
	}

	FMassEntityManager* Mgr = GetEditorEntityManager();
	if (!Mgr)
	{
		return false;
	}

	// Composite path — Info->Holder is invalid; each sub-mesh has its own holder in CompositeHolders.
	// Mirrors the static branch's rebuild sequence (UpdateInstanceTransformIncremental / MarkRenderStateDirty)
	// applied per sub-holder.
	if (Info->CompositeHolders.Num() > 0)
	{
		for (const FArcCompositeSubHolderRef& Ref : Info->CompositeHolders)
		{
			// Skinned sub-holders live in SkinnedHolderRegistry. Mirror the single-skinned branch below:
			if (Ref.bIsSkinned)
			{
				FArcMassRenderISMSkinnedFragment* SubSkinnedFrag = Mgr->GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Ref.Holder);
				if (SubSkinnedFrag == nullptr)
				{
					continue;
				}
				TMap<FArcHolderInstanceKey, int32>* SkinnedDenseMap = SkinnedHolderRegistry.HolderEntityToDenseIndex.Find(Ref.Holder);
				if (SkinnedDenseMap == nullptr)
				{
					continue;
				}
				const int32* SlotPtr = SkinnedDenseMap->Find(FArcHolderInstanceKey{Info->Source, Ref.SubIndex});
				if (SlotPtr == nullptr)
				{
					continue;
				}
				const int32 Slot = *SlotPtr;

				const FTransform SubWorld = Ref.RelativeTransform * WorldTransform;
				if (SubSkinnedFrag->PerInstanceData.IsValidIndex(Slot))
				{
					SubSkinnedFrag->PerInstanceData[Slot].Transform = FTransform3f(SubWorld);
				}

				// Mirror the single-skinned branch: incremental transform push, synchronous rebuild fallback.
				FArcRenderStateHelper& SkinnedHelper =
					Mgr->GetFragmentDataChecked<FArcRenderStateFragment>(Ref.Holder).GetRenderStateHelper();
				FArcMassInstancedSkinnedMeshRenderStateHelper& SubSkinnedHelper =
					Mgr->GetFragmentDataChecked<FArcRenderStateFragment>(Ref.Holder)
						.GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
				if (!SubSkinnedHelper.UpdateInstanceTransformIncremental(Slot))
				{
					SkinnedHelper.DestroyRenderState(nullptr);
					if (SkinnedHelper.ShouldCreateRenderState())
					{
						FRegisterComponentContext Ctx(nullptr);
						Ctx.Initialize(SkinnedHelper.GetWorld());
						SkinnedHelper.CreateRenderState(&Ctx);
						Ctx.Process();
					}
				}
				else
				{
					// Incremental patch succeeded => proxy exists => recompute culling bounds.
					SkinnedHolderRegistry.RefreshHolderBounds(Ref.Holder, *Mgr);
				}
				continue;
			}

			FArcRenderISMFragment* ISMFrag = Mgr->GetFragmentDataPtr<FArcRenderISMFragment>(Ref.Holder);
			if (ISMFrag == nullptr)
			{
				continue;
			}

			FArcRenderStateFragment* RenderState = Mgr->GetFragmentDataPtr<FArcRenderStateFragment>(Ref.Holder);
			if (RenderState == nullptr)
			{
				continue;
			}

			TMap<FArcHolderInstanceKey, int32>* DenseMap = HolderRegistry.HolderEntityToDenseIndex.Find(Ref.Holder);
			if (DenseMap == nullptr)
			{
				continue;
			}
			const int32* SlotPtr = DenseMap->Find(FArcHolderInstanceKey{Info->Source, Ref.SubIndex});
			if (SlotPtr == nullptr)
			{
				continue;
			}
			const int32 Slot = *SlotPtr;

			const FTransform SubWorld = Ref.RelativeTransform * WorldTransform;
			if (ISMFrag->PerInstanceSMData.IsValidIndex(Slot))
			{
				ISMFrag->PerInstanceSMData[Slot].Transform = SubWorld.ToMatrixWithScale();
			}

			// Mirror the static branch's incremental-or-dirty rebuild sequence exactly.
			FArcISMRenderStateHelper& Helper = RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>();
			if (!Helper.UpdateInstanceTransformIncremental(Slot))
			{
				Helper.MarkRenderStateDirty();
			}
			else
			{
				// Incremental patch succeeded => proxy exists => recompute culling bounds.
				HolderRegistry.RefreshHolderBounds(Ref.Holder, *Mgr);
			}
		}

		// Update the source entity's transform fragment once (gizmo pivot = the instance base transform).
		if (FTransformFragment* TF = Mgr->GetFragmentDataPtr<FTransformFragment>(Info->Source))
		{
			TF->SetTransform(WorldTransform);
		}

		return true;
	}

	// Dispatch by holder fragment type — skinned holders carry FArcMassRenderISMSkinnedFragment,
	// static holders carry FArcRenderISMFragment. Mutually exclusive by archetype construction.
	FArcMassRenderISMSkinnedFragment* SkinnedFrag = Mgr->GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Info->Holder);
	if (SkinnedFrag)
	{
		// Skinned path — resolve slot from the skinned registry reverse map
		TMap<FArcHolderInstanceKey, int32>* SkinnedDenseMap = SkinnedHolderRegistry.HolderEntityToDenseIndex.Find(Info->Holder);
		if (!SkinnedDenseMap)
		{
			return false;
		}
		const int32* SlotPtr = SkinnedDenseMap->Find(FArcHolderInstanceKey{Info->Source, 0});
		if (!SlotPtr)
		{
			return false;
		}
		const int32 Slot = *SlotPtr;

		// Write the new transform into the dense per-instance array before any incremental push, so the
		// manager delta and RefreshHolderBounds both read the updated transform. Keep this order.
		if (SkinnedFrag->PerInstanceData.IsValidIndex(Slot))
		{
			SkinnedFrag->PerInstanceData[Slot].Transform = FTransform3f(WorldTransform);
		}

		// Update the source entity's transform fragment
		if (FTransformFragment* TF = Mgr->GetFragmentDataPtr<FTransformFragment>(Info->Source))
		{
			TF->SetTransform(WorldTransform);
		}

		// Incrementally patch the scene proxy (mirrors the static path). UpdateInstanceTransformIncremental
		// flushes the manager transform delta + UpdatePrimitiveInstances, keeping the FPrimitiveSceneInfo
		// (and its Nanite RayTracingId) alive — no proxy churn. On false (no live proxy) fall back to the
		// synchronous DestroyRenderState+CreateRenderState rebuild: MarkRenderStateDirty alone can't be used
		// here because its sole consumer (UArcRenderStateDirtyUpdateProcessor) is disabled.
		FArcRenderStateHelper& Helper =
			Mgr->GetFragmentDataChecked<FArcRenderStateFragment>(Info->Holder).GetRenderStateHelper();
		FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
			Mgr->GetFragmentDataChecked<FArcRenderStateFragment>(Info->Holder)
				.GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
		if (!SkinnedHelper.UpdateInstanceTransformIncremental(Slot))
		{
			Helper.DestroyRenderState(nullptr);
			if (Helper.ShouldCreateRenderState())
			{
				FRegisterComponentContext Ctx(nullptr);
				Ctx.Initialize(Helper.GetWorld());
				Helper.CreateRenderState(&Ctx);
				Ctx.Process();
			}
		}
		else
		{
			// Incremental patch succeeded => proxy exists => recompute + re-push culling bounds so the
			// holder's WorldBounds/LocalBounds don't go stale after a per-instance move (mirrors static path).
			SkinnedHolderRegistry.RefreshHolderBounds(Info->Holder, *Mgr);
		}
		return true;
	}

	// Static path
	FArcRenderISMFragment* ISMFrag = Mgr->GetFragmentDataPtr<FArcRenderISMFragment>(Info->Holder);
	FArcRenderStateFragment* RenderState = Mgr->GetFragmentDataPtr<FArcRenderStateFragment>(Info->Holder);
	if (!ISMFrag || !RenderState)
	{
		return false;
	}

	// Resolve dense index live from the registry reverse map — never cached
	TMap<FArcHolderInstanceKey, int32>* DenseMap = HolderRegistry.HolderEntityToDenseIndex.Find(Info->Holder);
	if (!DenseMap)
	{
		return false;
	}
	const int32* DensePtr = DenseMap->Find(FArcHolderInstanceKey{Info->Source, 0});
	if (!DensePtr)
	{
		return false;
	}
	const int32 DenseIndex = *DensePtr;

	// Update the per-instance transform in the ISM fragment
	if (ISMFrag->PerInstanceSMData.IsValidIndex(DenseIndex))
	{
		ISMFrag->PerInstanceSMData[DenseIndex].Transform = WorldTransform.ToMatrixWithScale();
	}

	// Update the source entity's transform fragment
	if (FTransformFragment* TF = Mgr->GetFragmentDataPtr<FTransformFragment>(Info->Source))
	{
		TF->SetTransform(WorldTransform);
	}

	// Incrementally patch the scene proxy or fall back to full dirty.
	// NOTE: PerInstanceSMData[DenseIndex].Transform was written above, before this block, so
	// RefreshHolderBounds -> CalculateInstancedStaticMeshBounds sees the new transform. Keep that order.
	FArcISMRenderStateHelper& Helper = RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>();
	if (!Helper.UpdateInstanceTransformIncremental(DenseIndex))
	{
		Helper.MarkRenderStateDirty();
	}
	else
	{
		// Incremental patch succeeded => proxy exists => recompute + re-push culling bounds so the
		// holder's WorldBounds/LocalBounds don't go stale after a per-instance move.
		HolderRegistry.RefreshHolderBounds(Info->Holder, *Mgr);
	}
	return true;
}

void AArcPlacedEntityPartitionActor::RebuildEditorRenderState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcPlacedRebuildEditorRenderState);

	if (bIsRebuildingEditorRenderState)
	{
		return;
	}
	TGuardValue<bool> Guard(bIsRebuildingEditorRenderState, true);

	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	TearDownEditorRenderState();

	if (World->Scene == nullptr || !FApp::CanEverRender())
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': ABORT no scene / cannot render (Scene=%d CanEverRender=%d) — %d entries unrendered",
			*GetName(), World->Scene != nullptr, FApp::CanEverRender(), Entries.Num());
		return;
	}

	// Kick async loads for any not-yet-resident trait meshes/materials. Entries whose mesh
	// isn't resident defer below (bAnyDeferred) and the readiness poll rebuilds once loaded.
	RequestEditorMeshLoad();

	// Defer while global shaders are still compiling: building the proxy now binds GPUScene/
	// instance-culling compute against not-yet-final shader permutations, tripping the RHI
	// ValidateBoundUniformBuffer layout-hash check. The poll lands the build on a settled tick.
	if (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling())
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': DEFER ALL — shaders still compiling (%d jobs), %d entries unrendered; readiness poll armed",
			*GetName(), GShaderCompilingManager->GetNumRemainingJobs(), Entries.Num());
		EnsureRenderReadinessPoll(true);
		return;
	}

	FMassEntityManager* Mgr = GetEditorEntityManager();
	if (!Mgr)
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': ABORT no editor entity manager — %d entries unrendered",
			*GetName(), Entries.Num());
		return;
	}

	HolderRegistry.InitializeArchetypes(*Mgr);
	SkinnedHolderRegistry.InitializeArchetypes(*Mgr);

	// Build source archetype once: {FTransformFragment, FArcTypeElementFragment}
	// Shared between static and skinned loops — (EntryIndex, TransformIndex) key is unique per
	// instance regardless of mesh type, so entries from both loops never collide in InstanceToSourceEntity.
	if (!SourceEntityArchetype.IsValid())
	{
		TArray<const UScriptStruct*> FragStructs;
		FragStructs.Add(FTransformFragment::StaticStruct());
		FragStructs.Add(FArcTypeElementFragment::StaticStruct());
		SourceEntityArchetype = Mgr->CreateArchetype(TConstArrayView<const UScriptStruct*>(FragStructs));
	}

	TSet<FMassEntityHandle> TouchedHolders;

	bool bAnyDeferred = false;

	int32 DeferredStaticEntries = 0;
	int32 DeferredSkinnedEntries = 0;
	int32 DeferredCompositeEntries = 0;

	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];

		if (!IsComponentlessEntry(Entry))
		{
			continue;
		}

		const UArcUnifiedMeshVisualizationTrait* Unified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		if (Unified == nullptr)
		{
			continue;
		}

		UStaticMesh* Mesh = Unified->GetSingleStaticMeshDirect();
		const FTransform EntryPivot = GetEntryComponentTransform(Entry);
		FArcMassVisRenderingProps RenderProps = Unified->GetSingleRenderingProps();
		TArray<TObjectPtr<UMaterialInterface>> ResolvedMats = Unified->GetSingleMaterialOverridesDirect();

		// Non-loading: an unloaded mesh defers the entry; RequestEditorMeshLoad has already
		// queued the async load and the readiness poll rebuilds when it lands.
		if (Mesh == nullptr || Mesh->IsCompiling()
			|| Mesh->GetRenderData() == nullptr || !Mesh->GetRenderData()->IsInitialized())
		{
			++DeferredStaticEntries;
			UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': DEFER static entry %d (%ls) x%d instances — Mesh=%ls Compiling=%d RenderData=%d Initialized=%d",
				*GetName(), E, *GetNameSafe(Entry.EntityConfig), Entry.InstanceTransforms.Num(),
				*GetNameSafe(Mesh),
				Mesh != nullptr && Mesh->IsCompiling(),
				Mesh != nullptr && Mesh->GetRenderData() != nullptr,
				Mesh != nullptr && Mesh->GetRenderData() != nullptr && Mesh->GetRenderData()->IsInitialized());
			bAnyDeferred = true;
			continue;
		}

		// Build config fragments from the trait
		FArcMassStaticMeshConfigFragment StaticCfg(Mesh);

		FArcMassVisualizationMeshConfigFragment VisCfg;
		ArcPlacedEntityPartitionActor_Componentless::BuildVisFragmentFromProps(RenderProps, VisCfg);

		FArcOverrideMaterialsFragment OverrideMats;
		OverrideMats.OverrideMaterials = MoveTemp(ResolvedMats);
		const bool bHasMats = OverrideMats.OverrideMaterials.Num() > 0;
		const FArcOverrideMaterialsFragment* OverrideMatsPtr = bHasMats ? &OverrideMats : nullptr;

		FArcStaticMeshFragment SMFrag = ArcMass::Visualization::ToEngineFragment(StaticCfg);
		FArcVisualizationMeshFragment VisFrag = ArcMass::Visualization::ToEngineFragment(VisCfg);

		FArcISMHolderKey Key;
		Key.Mesh = StaticCfg.Mesh;
		Key.ConfigHash = ArcPlacedEntityPartitionActor_Componentless::ComputeConfigHash(VisFrag, OverrideMatsPtr);

		FMassEntityHandle Holder = HolderRegistry.FindOrCreateHolder(HolderBucket, Key, SMFrag, VisFrag, OverrideMatsPtr, *Mgr);
		if (!Holder.IsValid())
		{
			continue;
		}

		for (int32 T = 0; T < Entry.InstanceTransforms.Num(); ++T)
		{
			const FTransform WorldXform = EntryPivot * (Entry.InstanceTransforms[T] * GetActorTransform());

			TArray<FMassEntityHandle> Created;
			Mgr->BatchCreateEntities(SourceEntityArchetype, FMassArchetypeSharedFragmentValues(), 1, Created);
			if (Created.Num() == 0)
			{
				continue;
			}

			const FMassEntityHandle Source = Created[0];

			Mgr->GetFragmentDataChecked<FTransformFragment>(Source).SetTransform(WorldXform);

			// Register the element (discard handle — lightweight POD id resolves on demand at pick time)
			FArcMassEntityElementId Id;
			Id.PartitionActor = this;
			Id.EntryIndex = E;
			Id.TransformIndex = T;
			ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(Id, /*bAllowCreate*/true);

			FArcTypeElementInstanceId InstanceId;
			InstanceId.Owner = this;
			InstanceId.EntryIndex = E;
			InstanceId.TransformIndex = T;

			Mgr->GetFragmentDataChecked<FArcTypeElementFragment>(Source).InstanceId = InstanceId;

			HolderRegistry.AddInstance(Holder, Source, 0, WorldXform, *Mgr, /*bDeferBoundsAndCullingRefresh=*/true, &InstanceId);

			InstanceToSourceEntity.Add(
				TPair<int32, int32>(E, T),
				FArcComponentlessInstanceInfo{Source, Holder});

			TouchedHolders.Add(Holder);
		}
	}

	// Refresh bounds once per holder after the full batch
	for (const FMassEntityHandle& H : TouchedHolders)
	{
		HolderRegistry.RefreshHolderBounds(H, *Mgr);
	}

	// -------------------------------------------------------------------------
	// Skinned componentless loop — parallel to the static loop above.
	// Entries where IsSkinnedComponentlessEntry is true are rendered via the
	// FArcSkinnedMeshHolderRegistry path.
	// -------------------------------------------------------------------------

	TSet<FMassEntityHandle> SkinnedTouchedHolders;

	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];

		if (!IsSkinnedComponentlessEntry(Entry))
		{
			continue;
		}

		const UArcUnifiedMeshVisualizationTrait* SkinnedUnified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		if (SkinnedUnified == nullptr)
		{
			continue;
		}

		USkinnedAsset* Asset = nullptr;
		UTransformProviderData* TransformProvider = nullptr;
		FTransform SkinnedPivot = FTransform::Identity;
		FArcMassVisRenderingProps SkinnedRenderProps;
		TArray<TObjectPtr<UMaterialInterface>> SkinnedResolvedMats;
		// Single valid entry — its fields drive the skinned holder, RelativeTransform is the pivot.
		for (const FArcUnifiedMeshEntry& UnifiedEntry : SkinnedUnified->GetMeshEntries())
		{
			if (!UnifiedEntry.IsValidEntry())
			{
				continue;
			}
			Asset = UnifiedEntry.SkinnedAsset.Get();
			TransformProvider = UnifiedEntry.TransformProvider;
			SkinnedPivot = UnifiedEntry.RelativeTransform;
			SkinnedRenderProps = UnifiedEntry.RenderingProps;
			for (const TSoftObjectPtr<UMaterialInterface>& SoftMat : UnifiedEntry.MaterialOverrides)
			{
				if (UMaterialInterface* Mat = SoftMat.Get())
				{
					SkinnedResolvedMats.Add(Mat);
				}
			}
			break;
		}

		if (Asset == nullptr || Asset->GetResourceForRendering() == nullptr)
		{
			++DeferredSkinnedEntries;
			UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': DEFER skinned entry %d (%ls) x%d instances — Asset=%ls RenderResource=%d",
				*GetName(), E, *GetNameSafe(Entry.EntityConfig), Entry.InstanceTransforms.Num(),
				*GetNameSafe(Asset), Asset != nullptr && Asset->GetResourceForRendering() != nullptr);
			bAnyDeferred = true;
			continue;
		}

		// Build shared fragments from the trait
		FArcMassSkinnedMeshFragment SkinnedMeshFrag;
		SkinnedMeshFrag.SkinnedAsset = Asset;
		SkinnedMeshFrag.TransformProvider = TransformProvider;

		FArcMassVisualizationMeshConfigFragment SkinnedVisCfg;
		ArcPlacedEntityPartitionActor_Componentless::BuildVisFragmentFromProps(SkinnedRenderProps, SkinnedVisCfg);

		FArcOverrideMaterialsFragment SkinnedOverrideMats;
		SkinnedOverrideMats.OverrideMaterials = MoveTemp(SkinnedResolvedMats);
		const bool bSkinnedHasMats = SkinnedOverrideMats.OverrideMaterials.Num() > 0;
		const FArcOverrideMaterialsFragment* SkinnedOverrideMatsPtr = bSkinnedHasMats ? &SkinnedOverrideMats : nullptr;

		FArcVisualizationMeshFragment SkinnedVisFrag = ArcMass::Visualization::ToEngineFragment(SkinnedVisCfg);

		FArcSkinnedISMHolderKey SkinnedKey;
		SkinnedKey.Asset = Asset;
		SkinnedKey.ConfigHash = ArcPlacedEntityPartitionActor_Componentless::ComputeConfigHash(SkinnedVisFrag, SkinnedOverrideMatsPtr);

		FMassEntityHandle SkinnedHolder = SkinnedHolderRegistry.FindOrCreateHolder(
			SkinnedHolderBucket, SkinnedKey, SkinnedMeshFrag, SkinnedVisFrag, SkinnedOverrideMatsPtr, *Mgr);
		if (!SkinnedHolder.IsValid())
		{
			continue;
		}

		const FTransform& CompTransform = SkinnedPivot;

		for (int32 T = 0; T < Entry.InstanceTransforms.Num(); ++T)
		{
			const FTransform WorldXform = CompTransform * (Entry.InstanceTransforms[T] * GetActorTransform());

			TArray<FMassEntityHandle> SkinnedCreated;
			Mgr->BatchCreateEntities(SourceEntityArchetype, FMassArchetypeSharedFragmentValues(), 1, SkinnedCreated);
			if (SkinnedCreated.Num() == 0)
			{
				continue;
			}

			const FMassEntityHandle Source = SkinnedCreated[0];

			Mgr->GetFragmentDataChecked<FTransformFragment>(Source).SetTransform(WorldXform);

			FArcMassEntityElementId Id;
			Id.PartitionActor = this;
			Id.EntryIndex = E;
			Id.TransformIndex = T;
			ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(Id, /*bAllowCreate*/true);

			FArcTypeElementInstanceId InstanceId;
			InstanceId.Owner = this;
			InstanceId.EntryIndex = E;
			InstanceId.TransformIndex = T;

			Mgr->GetFragmentDataChecked<FArcTypeElementFragment>(Source).InstanceId = InstanceId;

			SkinnedHolderRegistry.AddInstance(SkinnedHolder, Source, /*SubIndex*/0, WorldXform, /*AnimationIndex*/0,
				*Mgr, /*bDeferBoundsAndCullingRefresh*/true, &InstanceId);

			InstanceToSourceEntity.Add(
				TPair<int32, int32>(E, T),
				FArcComponentlessInstanceInfo{Source, SkinnedHolder});

			SkinnedTouchedHolders.Add(SkinnedHolder);
		}
	}

	// Refresh skinned holder bounds once per holder after the full batch
	for (const FMassEntityHandle& H : SkinnedTouchedHolders)
	{
		SkinnedHolderRegistry.RefreshHolderBounds(H, *Mgr);
	}

	// -------------------------------------------------------------------------
	// Composite componentless loop — each entry spawns one source entity and N
	// holder-ISM slots (one per sub-mesh).  Mirrors the static loop above but
	// iterates MeshEntries and fills FArcComponentlessInstanceInfo::CompositeHolders.
	// -------------------------------------------------------------------------

	TSet<FMassEntityHandle> CompositeTouchedHolders;
	TSet<FMassEntityHandle> CompositeSkinnedTouchedHolders;

	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];

		if (!IsCompositeComponentlessEntry(Entry))
		{
			continue;
		}

		struct FCompositeSubEntrySpec
		{
			UStaticMesh* Mesh = nullptr;
			USkinnedAsset* SkinnedAsset = nullptr;
			UTransformProviderData* TransformProvider = nullptr;
			FTransform RelativeTransform = FTransform::Identity;
			TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
			FArcMassVisRenderingProps RenderingProps;
		};
		TArray<FCompositeSubEntrySpec> SubEntries;
		bool bAllReady = true;

		const UArcUnifiedMeshVisualizationTrait* CompositeUnified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		if (CompositeUnified == nullptr)
		{
			continue;
		}

		for (const FArcUnifiedMeshEntry& UnifiedEntry : CompositeUnified->GetMeshEntries())
		{
			if (!UnifiedEntry.IsValidEntry())
			{
				continue;
			}
			FCompositeSubEntrySpec& Spec = SubEntries.AddDefaulted_GetRef();
			Spec.RelativeTransform = UnifiedEntry.RelativeTransform;
			Spec.RenderingProps = UnifiedEntry.RenderingProps;
			for (const TSoftObjectPtr<UMaterialInterface>& SoftMat : UnifiedEntry.MaterialOverrides)
			{
				if (UMaterialInterface* Mat = SoftMat.Get())
				{
					Spec.MaterialOverrides.Add(Mat);
				}
			}
			if (UnifiedEntry.IsSkinnedEntry())
			{
				Spec.SkinnedAsset = UnifiedEntry.SkinnedAsset.Get();
				Spec.TransformProvider = UnifiedEntry.TransformProvider;
				// Authored but unresolved (still streaming) or no render resource yet — defer.
				if (Spec.SkinnedAsset == nullptr || Spec.SkinnedAsset->GetResourceForRendering() == nullptr)
				{
					bAllReady = false;
				}
			}
			else
			{
				Spec.Mesh = UnifiedEntry.StaticMesh.Get();
				if (Spec.Mesh == nullptr || Spec.Mesh->IsCompiling()
					|| Spec.Mesh->GetRenderData() == nullptr || !Spec.Mesh->GetRenderData()->IsInitialized())
				{
					bAllReady = false;
				}
			}
		}

		// Defer the whole entry if ANY sub-mesh is not yet render-ready.
		if (!bAllReady)
		{
			++DeferredCompositeEntries;
			UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': DEFER composite entry %d (%ls) x%d instances — at least one of %d sub-meshes not render-ready",
				*GetName(), E, *GetNameSafe(Entry.EntityConfig), Entry.InstanceTransforms.Num(), SubEntries.Num());
			bAnyDeferred = true;
			continue;
		}

		for (int32 T = 0; T < Entry.InstanceTransforms.Num(); ++T)
		{
			const FTransform Base = Entry.InstanceTransforms[T] * GetActorTransform();

			TArray<FMassEntityHandle> Created;
			Mgr->BatchCreateEntities(SourceEntityArchetype, FMassArchetypeSharedFragmentValues(), 1, Created);
			if (Created.Num() == 0)
			{
				continue;
			}

			const FMassEntityHandle Source = Created[0];

			Mgr->GetFragmentDataChecked<FTransformFragment>(Source).SetTransform(Base);

			FArcMassEntityElementId Id;
			Id.PartitionActor = this;
			Id.EntryIndex = E;
			Id.TransformIndex = T;
			ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(Id, /*bAllowCreate*/true);

			FArcTypeElementInstanceId InstanceId;
			InstanceId.Owner = this;
			InstanceId.EntryIndex = E;
			InstanceId.TransformIndex = T;

			Mgr->GetFragmentDataChecked<FArcTypeElementFragment>(Source).InstanceId = InstanceId;

			FArcComponentlessInstanceInfo Info;
			Info.Source = Source;
			// Info.Holder left default-invalid — composite path fills CompositeHolders instead.

			for (int32 M = 0; M < SubEntries.Num(); ++M)
			{
				const FCompositeSubEntrySpec& Sub = SubEntries[M];

				FArcMassVisualizationMeshConfigFragment VisCfg;
				ArcPlacedEntityPartitionActor_Componentless::BuildVisFragmentFromProps(Sub.RenderingProps, VisCfg);

				FArcOverrideMaterialsFragment OverrideMats;
				OverrideMats.OverrideMaterials = Sub.MaterialOverrides;
				const bool bHasMats = OverrideMats.OverrideMaterials.Num() > 0;
				const FArcOverrideMaterialsFragment* OverrideMatsPtr = bHasMats ? &OverrideMats : nullptr;

				FArcVisualizationMeshFragment VisFrag = ArcMass::Visualization::ToEngineFragment(VisCfg);

				const FTransform WorldXform = Sub.RelativeTransform * Base;

				FMassEntityHandle Holder;
				if (Sub.SkinnedAsset != nullptr)
				{
					// Skinned sub-mesh — same registry path as the single-skinned loop above,
					// but keyed by the sub-entry ordinal so multiple skinned subs on one holder
					// stay distinct.
					FArcMassSkinnedMeshFragment SkinnedMeshFrag;
					SkinnedMeshFrag.SkinnedAsset = Sub.SkinnedAsset;
					SkinnedMeshFrag.TransformProvider = Sub.TransformProvider;

					FArcSkinnedISMHolderKey SkinnedKey;
					SkinnedKey.Asset = Sub.SkinnedAsset;
					SkinnedKey.ConfigHash = ArcPlacedEntityPartitionActor_Componentless::ComputeConfigHash(VisFrag, OverrideMatsPtr);

					Holder = SkinnedHolderRegistry.FindOrCreateHolder(
						SkinnedHolderBucket, SkinnedKey, SkinnedMeshFrag, VisFrag, OverrideMatsPtr, *Mgr);
					if (!Holder.IsValid())
					{
						continue;
					}

					SkinnedHolderRegistry.AddInstance(Holder, Source, /*SubIndex*/M, WorldXform, /*AnimationIndex*/0,
						*Mgr, /*bDeferBoundsAndCullingRefresh*/true, &InstanceId);
					CompositeSkinnedTouchedHolders.Add(Holder);
				}
				else
				{
					FArcMassStaticMeshConfigFragment StaticCfg(Sub.Mesh);
					FArcStaticMeshFragment SMFrag = ArcMass::Visualization::ToEngineFragment(StaticCfg);

					FArcISMHolderKey Key;
					Key.Mesh = StaticCfg.Mesh;
					Key.ConfigHash = ArcPlacedEntityPartitionActor_Componentless::ComputeConfigHash(VisFrag, OverrideMatsPtr);

					Holder = HolderRegistry.FindOrCreateHolder(HolderBucket, Key, SMFrag, VisFrag, OverrideMatsPtr, *Mgr);
					if (!Holder.IsValid())
					{
						continue;
					}

					HolderRegistry.AddInstance(Holder, Source, /*SubIndex=*/M, WorldXform, *Mgr, /*bDeferBoundsAndCullingRefresh=*/true, &InstanceId);
					CompositeTouchedHolders.Add(Holder);
				}

				FArcCompositeSubHolderRef SubRef;
				SubRef.Holder = Holder;
				SubRef.SubIndex = M;
				SubRef.RelativeTransform = Sub.RelativeTransform;
				SubRef.bIsSkinned = (Sub.SkinnedAsset != nullptr);
				Info.CompositeHolders.Add(SubRef);
			}

			InstanceToSourceEntity.Add(TPair<int32, int32>(E, T), Info);
		}
	}

	// Refresh composite holder bounds once per holder after the full batch
	for (const FMassEntityHandle& H : CompositeTouchedHolders)
	{
		HolderRegistry.RefreshHolderBounds(H, *Mgr);
	}
	for (const FMassEntityHandle& H : CompositeSkinnedTouchedHolders)
	{
		SkinnedHolderRegistry.RefreshHolderBounds(H, *Mgr);
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] PlacedRebuild '%ls': %d entries, %d instances rendered; deferred static=%d skinned=%d composite=%d; poll=%d",
		*GetName(), Entries.Num(), InstanceToSourceEntity.Num(),
		DeferredStaticEntries, DeferredSkinnedEntries, DeferredCompositeEntries, bAnyDeferred);

	EnsureRenderReadinessPoll(bAnyDeferred);

	// Re-select the instances that were selected before teardown deselected them, so the transform gizmo
	// survives the rebuild. Handles are minted fresh above (new registry slots, not pointer-equal to the
	// old ones) but are deterministically re-acquirable by FArcMassEntityElementId. Acquire with
	// bAllowCreate=false: an entry still deferred (mesh/shader compiling) has no owner yet and is skipped —
	// the EnsureRenderReadinessPoll ticker re-snapshots and restores it on a later rebuild.
	if (PendingSelectionRestore.Num() > 0)
	{
		UTypedElementSelectionSet* SelectionSet =
			(GEditor && GEditor->GetSelectedActors()) ? GEditor->GetSelectedActors()->GetElementSelectionSet() : nullptr;
		if (SelectionSet)
		{
			const FTypedElementSelectionOptions Opts =
				FTypedElementSelectionOptions().SetAllowLegacyNotifications(false);
			for (const TPair<int32, int32>& Key : PendingSelectionRestore)
			{
				if (!InstanceToSourceEntity.Contains(Key))
				{
					continue;
				}
				FArcMassEntityElementId Id;
				Id.PartitionActor = this;
				Id.EntryIndex = Key.Key;
				Id.TransformIndex = Key.Value;
				const FTypedElementHandle Handle =
					ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(Id, /*bAllowCreate*/false);
				if (Handle)
				{
					// Additive — must not disturb selections of other actors/elements.
					SelectionSet->SelectElement(Handle, Opts);
				}
			}
		}
		PendingSelectionRestore.Reset();
	}

	OnEditorStateChanged.Broadcast(this, EArcPlacedEntityOutlinerEvent::Rebuilt);
}

void AArcPlacedEntityPartitionActor::TearDownEditorRenderState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcPlacedTearDownEditorRenderState);

	// Keystone for gizmo-selection preservation: clear the snapshot at the very top so it is always
	// re-derived from the *current* selection on every real rebuild (RebuildEditorRenderState calls this
	// internally). This also wipes any snapshot stranded by a teardown-only path (OnPreBeginPIE,
	// BeginDestroy, the Mgr==null early-exit) before the next rebuild repopulates from truth.
	PendingSelectionRestore.Reset();

	if (PendingRenderRebuildTickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PendingRenderRebuildTickHandle);
		PendingRenderRebuildTickHandle.Reset();
	}

	FMassEntityManager* Mgr = GetEditorEntityManager();

	if (!Mgr)
	{
		HolderBucket.Empty();
		SkinnedHolderBucket.Empty();
		InstanceToSourceEntity.Empty();
		return;
	}

	// Deselect each handle from the editor selection set before destroying its element.
	// DestroyElement resets the internal type ID to 0, but a still-selected handle leaves a stale
	// (DataPtr!=null, TypeId==0) entry that trips the "type ID not registered" ensure on the next
	// viewport interaction. Covers all teardown paths: delete, rebuild, PIE-enter, undo, BeginDestroy.
	UTypedElementSelectionSet* SelectionSet =
		(GEditor && GEditor->GetSelectedActors()) ? GEditor->GetSelectedActors()->GetElementSelectionSet() : nullptr;

	for (TPair<TPair<int32, int32>, FArcComponentlessInstanceInfo>& P : InstanceToSourceEntity)
	{
		FArcMassEntityElementId Id;
		Id.PartitionActor = this;
		Id.EntryIndex = P.Key.Key;
		Id.TransformIndex = P.Key.Value;

		if (SelectionSet)
		{
			const FTypedElementHandle Handle =
				ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(Id, /*bAllowCreate*/false);
			if (Handle)
			{
				// Remember selected instances so the rebuild can re-select the fresh handles and keep the gizmo.
				if (SelectionSet->IsElementSelected(Handle, FTypedElementIsSelectedOptions()))
				{
					PendingSelectionRestore.Add(P.Key);
				}
				SelectionSet->DeselectElement(Handle, FTypedElementSelectionOptions());
			}
		}

		ArcMassEntityElementUtils::DestroyEditorArcMassEntityElement(Id);
	}

	// Clear holder render state
	for (TPair<FArcISMHolderKey, FMassEntityHandle>& HP : HolderBucket)
	{
		HolderRegistry.ClearHolder(HP.Value, *Mgr);
	}

	// Clear skinned holder render state — adjacent to the static holder clear so both happen
	// before the shared source-entity destroy + flush below.
	// The shared deselect-before-destroy loop and the source-entity Defer().DestroyEntity loop above
	// already cover skinned instances (same NAME_ArcMassEntity elements, same InstanceToSourceEntity map).
	// Only the skinned-specific ClearHolder loop + bucket reset is added here.
	for (TPair<FArcSkinnedISMHolderKey, FMassEntityHandle>& SkinnedPair : SkinnedHolderBucket)
	{
		SkinnedHolderRegistry.ClearHolder(SkinnedPair.Value, *Mgr);
	}
	SkinnedHolderBucket.Empty();

	// Defer-destroy source entities
	for (TPair<TPair<int32, int32>, FArcComponentlessInstanceInfo>& P : InstanceToSourceEntity)
	{
		if (Mgr->IsEntityValid(P.Value.Source))
		{
			Mgr->Defer().DestroyEntity(P.Value.Source);
		}
	}

	// Editor-only flush: no guaranteed Mass phase tick in editor world, must flush synchronously
	// before the next rebuild. Never appropriate inside a processor.
	Mgr->FlushCommands();

	HolderBucket.Empty();
	InstanceToSourceEntity.Empty();

	OnEditorStateChanged.Broadcast(this, EArcPlacedEntityOutlinerEvent::TornDown);
}

void AArcPlacedEntityPartitionActor::RebuildAllEditorState()
{
	TearDownEditorRenderState();
	DestroyEditorPreviewISMCs();
	RebuildEditorRenderState();
	RebuildEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::RequestEditorMeshLoad()
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}
	// A previous request is still streaming — its assets will land and the readiness poll
	// rebuilds; re-requesting the same set would just churn handles.
	if (EditorMeshLoadHandle.IsValid() && !EditorMeshLoadHandle->HasLoadCompleted())
	{
		return;
	}

	TArray<FSoftObjectPath> Paths;
	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		const UArcUnifiedMeshVisualizationTrait* Unified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		if (Unified == nullptr || !Unified->IsValid())
		{
			continue;
		}
		bool bAnyMissing = false;
		for (const FArcUnifiedMeshEntry& UnifiedEntry : Unified->GetMeshEntries())
		{
			if ((!UnifiedEntry.StaticMesh.IsNull() && UnifiedEntry.StaticMesh.Get() == nullptr)
				|| (!UnifiedEntry.SkinnedAsset.IsNull() && UnifiedEntry.SkinnedAsset.Get() == nullptr))
			{
				bAnyMissing = true;
				break;
			}
		}
		if (bAnyMissing)
		{
			Unified->CollectSoftPaths(Paths);
		}
	}

	if (Paths.Num() == 0)
	{
		return;
	}

	EditorMeshLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		MoveTemp(Paths), FStreamableDelegate());
}

bool AArcPlacedEntityPartitionActor::AnyDeferredComponentlessEntryNowReady() const
{
	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];
		if (!IsComponentlessEntry(Entry))
		{
			continue;
		}
		// Already rendered? Its {E,0} key is present in InstanceToSourceEntity.
		if (InstanceToSourceEntity.Contains(TPair<int32, int32>(E, 0)))
		{
			continue;
		}
		const UArcUnifiedMeshVisualizationTrait* Unified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		const UStaticMesh* Mesh = Unified != nullptr ? Unified->GetSingleStaticMeshDirect() : nullptr;
		if (Mesh != nullptr && !Mesh->IsCompiling()
			&& Mesh->GetRenderData() != nullptr && Mesh->GetRenderData()->IsInitialized())
		{
			return true;
		}
	}

	// Skinned componentless check — mirrors the Step 5 skinned rebuild gate.
	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];
		if (!IsSkinnedComponentlessEntry(Entry))
		{
			continue;
		}
		// Already rendered? Its {E,0} key is present in InstanceToSourceEntity.
		if (InstanceToSourceEntity.Contains(TPair<int32, int32>(E, 0)))
		{
			continue;
		}
		const UArcUnifiedMeshVisualizationTrait* Unified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		USkinnedAsset* Asset = Unified != nullptr ? Unified->GetSingleSkinnedAssetDirect() : nullptr;
		if (Asset != nullptr && Asset->GetResourceForRendering() != nullptr)
		{
			return true;
		}
	}

	// Composite componentless check — mirrors the Step 4 composite rebuild gate.
	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];
		if (!IsCompositeComponentlessEntry(Entry))
		{
			continue;
		}
		if (InstanceToSourceEntity.Contains(TPair<int32, int32>(E, 0)))
		{
			continue;   // already rendered
		}
		// Any READY sub-entry (static render-ready, or skinned with render
		// resources) triggers a rebuild attempt; the rebuild re-defers if others still stream.
		const UArcUnifiedMeshVisualizationTrait* Unified =
			ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
		if (Unified == nullptr)
		{
			continue;
		}
		for (const FArcUnifiedMeshEntry& UnifiedEntry : Unified->GetMeshEntries())
		{
			if (!UnifiedEntry.IsValidEntry())
			{
				continue;
			}
			if (UnifiedEntry.IsSkinnedEntry())
			{
				USkinnedAsset* Asset = UnifiedEntry.SkinnedAsset.Get();
				if (Asset != nullptr && Asset->GetResourceForRendering() != nullptr)
				{
					return true;
				}
			}
			else
			{
				const UStaticMesh* Mesh = UnifiedEntry.StaticMesh.Get();
				if (Mesh != nullptr && !Mesh->IsCompiling()
					&& Mesh->GetRenderData() != nullptr && Mesh->GetRenderData()->IsInitialized())
				{
					return true;
				}
			}
		}
	}
	return false;
}

void AArcPlacedEntityPartitionActor::EnsureRenderReadinessPoll(bool bNeeded)
{
	if (!bNeeded || PendingRenderRebuildTickHandle.IsValid())
	{
		return;
	}
	TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakSelf(this);
	PendingRenderRebuildTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakSelf](float) -> bool
		{
			AArcPlacedEntityPartitionActor* Self = WeakSelf.Get();
			if (Self == nullptr)
			{
				return false;
			}
			if (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling())
			{
				// Timeout guard: ~300s at 0.25s tick to prevent pinning the ticker on a stuck compile.
				if (++Self->PostCompileRebuildPollTicks > 1200)
				{
					Self->bForcePostCompileRebuild = false;
					Self->PendingRenderRebuildTickHandle.Reset();
					UE_LOGF(LogMass, Warning,"AArcPlacedEntityPartitionActor: post-compile rebuild gave up waiting on shader compilation");
					return false;
				}
				return true;
			}
			// Shaders settled.
			if (Self->bForcePostCompileRebuild)
			{
				Self->bForcePostCompileRebuild = false;
				Self->PostCompileRebuildPollTicks = 0;
				Self->PendingRenderRebuildTickHandle.Reset();
				Self->RebuildEditorRenderState();
				return false;
			}
			if (!Self->AnyDeferredComponentlessEntryNowReady())
			{
				return true;
			}
			Self->PendingRenderRebuildTickHandle.Reset();
			Self->RebuildEditorRenderState();
			return false;
		}),
		0.25f);
}

void AArcPlacedEntityPartitionActor::OnMaterialCompilationFinished(UMaterialInterface* /*Material*/)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->IsGameWorld())
	{
		return;
	}
	bForcePostCompileRebuild = true;
	PostCompileRebuildPollTicks = 0;
	EnsureRenderReadinessPoll(true);
}

bool AArcPlacedEntityPartitionActor::GetMassEntityInstanceMeshBounds(const FArcMassEntityElementId& InId, FBoxSphereBounds& OutBounds) const
{
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	if (!ResolveElementId(InId, E, T))
	{
		return false;
	}

	const FArcPlacedEntityEntry& Entry = Entries[E];
	if (Entry.EntityConfig == nullptr)
	{
		return false;
	}

	const UArcUnifiedMeshVisualizationTrait* Unified =
		ArcPlacedEntityPartitionActor_Componentless::FindUnifiedTrait(Entry);
	const FTransform Base = Entry.InstanceTransforms[T] * GetActorTransform();

	if (IsComponentlessEntry(Entry))
	{
		UStaticMesh* Mesh = Unified != nullptr ? Unified->GetSingleStaticMeshDirect() : nullptr;
		if (Mesh == nullptr)
		{
			return false;
		}
		OutBounds = Mesh->GetBounds().TransformBy(GetEntryComponentTransform(Entry) * Base);
		return true;
	}

	if (IsSkinnedComponentlessEntry(Entry))
	{
		USkinnedAsset* Asset = Unified != nullptr ? Unified->GetSingleSkinnedAssetDirect() : nullptr;
		if (Asset == nullptr)
		{
			return false;
		}
		OutBounds = Asset->GetBounds().TransformBy(GetEntryComponentTransform(Entry) * Base);
		return true;
	}

	if (!IsCompositeComponentlessEntry(Entry))
	{
		return false;
	}

	if (Unified == nullptr)
	{
		return false;
	}

	bool bHasAny = false;
	FBoxSphereBounds Aggregate(ForceInit);
	for (const FArcUnifiedMeshEntry& UnifiedEntry : Unified->GetMeshEntries())
	{
		if (!UnifiedEntry.IsValidEntry())
		{
			continue;
		}
		FBoxSphereBounds SubBounds(ForceInit);
		if (UnifiedEntry.IsSkinnedEntry())
		{
			USkinnedAsset* SubAsset = UnifiedEntry.SkinnedAsset.Get();
			if (SubAsset == nullptr)
			{
				continue;
			}
			SubBounds = SubAsset->GetBounds().TransformBy(UnifiedEntry.RelativeTransform * Base);
		}
		else
		{
			UStaticMesh* SubMesh = UnifiedEntry.StaticMesh.Get();
			if (SubMesh == nullptr)
			{
				continue;
			}
			SubBounds = SubMesh->GetBounds().TransformBy(UnifiedEntry.RelativeTransform * Base);
		}
		Aggregate = bHasAny ? Aggregate + SubBounds : SubBounds;
		bHasAny = true;
	}

	if (!bHasAny)
	{
		return false;
	}
	OutBounds = Aggregate;
	return true;
}

void AArcPlacedEntityPartitionActor::PostEditUndo()
{
	Super::PostEditUndo();
	RebuildAllEditorState();
}

#endif // WITH_EDITOR
