// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityActor.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/EntityFragments.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ActorComponent.h" // FRegisterComponentContext (skinned proxy rebuild in UpdateInstanceRender)

#include "ArcMass/PlacedEntities/ArcLinkableGuid.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
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
#include "Misc/App.h"
#include "ShaderCompiler.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
// Skinned componentless render path
#include "ArcMass/SkinnedMeshVisualization/ArcSkinnedMeshHolderRegistry.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAlwaysLoadedEntityActor)

// -----------------------------------------------------------------------------
// File-scoped helpers (componentless path)
// -----------------------------------------------------------------------------

#if WITH_EDITOR
namespace ArcAlwaysLoadedEntityActor_Componentless
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
	Out.CastShadow                        = Props.bCastShadow;
	Out.CastShadowAsTwoSided              = Props.bCastShadowAsTwoSided;
	Out.CastHiddenShadow                  = Props.bCastHiddenShadow;
	Out.CastFarShadow                     = Props.bCastFarShadow;
	Out.CastInsetShadow                   = Props.bCastInsetShadow;
	Out.CastContactShadow                 = Props.bCastContactShadow;
	Out.HiddenInGame                      = Props.bHiddenInGame;
	Out.ReceivesDecals                    = Props.bReceivesDecals;
	Out.NeverDistanceCull                 = Props.bNeverDistanceCull;
	Out.RenderCustomDepth                 = Props.bRenderCustomDepth;
	Out.CustomDepthStencilValue           = Props.CustomDepthStencilValue;
	Out.TranslucencySortPriority          = Props.TranslucencySortPriority;
	Out.AffectDynamicIndirectLighting     = Props.bAffectDynamicIndirectLighting;
	Out.AffectIndirectLightingWhileHidden = Props.bAffectIndirectLightingWhileHidden;
	Out.LightingChannels                  = Props.LightingChannels;
	Out.VisibleInReflections              = Props.bVisibleInReflections;
	Out.VisibleInRayTracing               = Props.bVisibleInRayTracing;
	Out.VisibleInReflectionCaptures       = Props.bVisibleInReflectionCaptures;
	Out.MinDrawDistance                   = Props.MinDrawDistance;
	Out.LDMaxDrawDistance                 = Props.LDMaxDrawDistance;
	Out.CustomDepthStencilWriteMask       = Props.CustomDepthStencilWriteMask;
}

} // namespace ArcAlwaysLoadedEntityActor_Componentless
#endif // WITH_EDITOR

AArcAlwaysLoadedEntityActor::AArcAlwaysLoadedEntityActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

// -----------------------------------------------------------------------------
// IArcMassEntityManagerProvider
// -----------------------------------------------------------------------------

IArcMassEntityManager* AArcAlwaysLoadedEntityActor::GetMassEntityManager(const FArcMassEntityElementId& InId)
{
	return (InId.PartitionActor.Get() == this) ? this : nullptr;
}

// -----------------------------------------------------------------------------
// IArcMassEntityManager — stubs and implementations
// -----------------------------------------------------------------------------

bool AArcAlwaysLoadedEntityActor::CanEditMassEntityInstance(const FArcMassEntityElementId& InId) const
{
#if WITH_EDITOR
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	return ResolveElementId(InId, E, T);
#else
	return false;
#endif
}

bool AArcAlwaysLoadedEntityActor::CanMoveMassEntityInstance(const FArcMassEntityElementId& InId, ETypedElementWorldType WorldType) const
{
#if WITH_EDITOR
	int32 E = INDEX_NONE;
	int32 T = INDEX_NONE;
	return ResolveElementId(InId, E, T);
#else
	return false;
#endif
}

bool AArcAlwaysLoadedEntityActor::GetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, FTransform& OutTransform, bool bWorldSpace) const
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

bool AArcAlwaysLoadedEntityActor::SetMassEntityInstanceTransform(const FArcMassEntityElementId& InId, const FTransform& InTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
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

void AArcAlwaysLoadedEntityActor::NotifyMassEntityInstanceMovementStarted(const FArcMassEntityElementId& InId)
{
	// No-op
}

void AArcAlwaysLoadedEntityActor::NotifyMassEntityInstanceMovementOngoing(const FArcMassEntityElementId& InId)
{
	// No-op
}

void AArcAlwaysLoadedEntityActor::NotifyMassEntityInstanceMovementEnded(const FArcMassEntityElementId& InId)
{
#if WITH_EDITOR
	Modify();
#endif
}

void AArcAlwaysLoadedEntityActor::NotifyMassEntityInstanceSelectionChanged(const FArcMassEntityElementId& InId, bool bIsSelected)
{
	// No-op — selection highlight not yet implemented for the componentless path
}

bool AArcAlwaysLoadedEntityActor::DeleteMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds)
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

	// Singleton — no self-destruct when entries are empty (unlike the partition actor)
	RebuildGuidIndex();
	RebuildEditorRenderState();
	RebuildEditorPreviewISMCs();
	return true;
#else
	return false;
#endif
}

bool AArcAlwaysLoadedEntityActor::DuplicateMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds, TArray<FArcMassEntityElementId>& OutNewIds)
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

// -----------------------------------------------------------------------------
// Runtime Entity Spawning
// -----------------------------------------------------------------------------

void AArcAlwaysLoadedEntityActor::BeginPlay()
{
	Super::BeginPlay();

	// Dedicated servers never load meshes (BuildTemplate skips vis fragments) — spawn immediately.
	UWorld* World = GetWorld();
	if (World != nullptr && World->GetNetMode() == NM_DedicatedServer)
	{
		SpawnEntities();
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
			UE_LOGF(LogMass, Warning,"AArcAlwaysLoadedEntityActor::BeginPlay: config '%ls' has no valid PrimaryAssetId — "
					"AssetManager hasn't registered it; bundle gate bypassed for this config.",
				*GetPathNameSafe(Entry.EntityConfig));
		}
	}

	if (PrimaryAssetIds.Num() == 0)
	{
		SpawnEntities();
		return;
	}

	bPendingBundleSpawn = true;

	TWeakObjectPtr<AArcAlwaysLoadedEntityActor> WeakSelf(this);
	FStreamableDelegate OnComplete = FStreamableDelegate::CreateLambda(
		[WeakSelf]()
		{
			AArcAlwaysLoadedEntityActor* Self = WeakSelf.Get();
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

void AArcAlwaysLoadedEntityActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Pending bundle completion must not spawn into a world we're leaving.
	bPendingBundleSpawn = false;

	DespawnEntities();
	Super::EndPlay(EndPlayReason);
}

void AArcAlwaysLoadedEntityActor::SpawnEntities()
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
		UE_LOGF(LogMass, Warning,"AArcAlwaysLoadedEntityActor::SpawnEntities: UArcMassSpawnerSubsystem not found");
		return;
	}

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

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig == nullptr || Entry.InstanceTransforms.Num() == 0)
		{
			continue;
		}

		UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(Entry.EntityConfig);
		if (ArcCfg == nullptr)
		{
			UE_LOGF(LogMass, Warning,"AArcAlwaysLoadedEntityActor::SpawnEntities: EntityConfig '%ls' is not UArcMassEntityConfigAsset — skipping.",
				*GetNameSafe(Entry.EntityConfig));
			continue;
		}

		const int32 Count = Entry.InstanceTransforms.Num();
		for (int32 i = 0; i < Count; ++i)
		{
			const FTransform Xform = Entry.InstanceTransforms[i] * GetActorTransform();

			TConstArrayView<FInstancedStruct> OverrideView;
			const FArcPlacedEntityFragmentOverrides* Overrides = Entry.PerInstanceOverrides.Find(i);
			if (Overrides != nullptr)
			{
				OverrideView = MakeArrayView(Overrides->FragmentValues);
			}

			FMassEntityHandle Handle = Spawner->SpawnEntityWithOverrides(ArcCfg, Xform, OverrideView);
			if (!Handle.IsValid())
			{
				continue;
			}

			// If entity has both LinkGuid and PersistenceGuid, unify them
			FStructView LinkGuidView = EntityManager.GetFragmentDataStruct(Handle, FArcLinkableGuidFragment::StaticStruct());
			if (LinkGuidView.IsValid())
			{
				const FArcLinkableGuidFragment& LinkGuidFrag = LinkGuidView.Get<const FArcLinkableGuidFragment>();
				if (LinkGuidFrag.LinkGuid.IsValid())
				{
					FStructView PersistView = EntityManager.GetFragmentDataStruct(Handle, FArcMassPersistenceFragment::StaticStruct());
					if (PersistView.IsValid())
					{
						FArcMassPersistenceFragment& PersistFrag = PersistView.Get<FArcMassPersistenceFragment>();
						PersistFrag.PersistenceGuid = LinkGuidFrag.LinkGuid;
					}
				}
			}

			SpawnedEntities.Add(Handle);
		}
	}
}

void AArcAlwaysLoadedEntityActor::DespawnEntities()
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
		EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	}

	SpawnedEntities.Reset();
}

void AArcAlwaysLoadedEntityActor::BeginDestroy()
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
#endif
	Super::BeginDestroy();
}

// -----------------------------------------------------------------------------
// Editor
// -----------------------------------------------------------------------------

#if WITH_EDITOR

void AArcAlwaysLoadedEntityActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		RebuildEditorPreviewISMCs();
		RebuildEditorRenderState();

		UE::Editor::PIE::OnPreBegin.Remove(PreBeginPIEHandle);
		UE::Editor::PIE::OnEnd.Remove(EndPIEHandle);
		PreBeginPIEHandle = UE::Editor::PIE::OnPreBegin.AddUObject(this, &AArcAlwaysLoadedEntityActor::OnPreBeginPIE);
		EndPIEHandle = UE::Editor::PIE::OnEnd.AddUObject(this, &AArcAlwaysLoadedEntityActor::OnEndPIE);

		UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
		MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddWeakLambda(
			this, [this](UMaterialInterface* Material) { OnMaterialCompilationFinished(Material); });
	}
}

void AArcAlwaysLoadedEntityActor::OnPreBeginPIE(const bool bIsSimulating)
{
	TearDownEditorRenderState();
	DestroyEditorPreviewISMCs();
}

void AArcAlwaysLoadedEntityActor::OnEndPIE(const bool bIsSimulating)
{
	RebuildEditorPreviewISMCs();
	RebuildEditorRenderState();
}

void AArcAlwaysLoadedEntityActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildGuidIndex();
	RebuildEditorPreviewISMCs();
	RebuildEditorRenderState();
}

void AArcAlwaysLoadedEntityActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	FArcPlacedEntityEditorEvents::OnInstanceTransformChanged.Broadcast();
}

void AArcAlwaysLoadedEntityActor::PostLoad()
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

	UWorld* W = GetWorld();
	if (W != nullptr && !W->IsGameWorld())
	{
		RebuildEditorPreviewISMCs();
		RebuildEditorRenderState();
	}
}

void AArcAlwaysLoadedEntityActor::AutoAssignLinkGuidIfNeeded(FArcPlacedEntityEntry& Entry, int32 InstanceIndex)
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

void AArcAlwaysLoadedEntityActor::AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex)
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

void AArcAlwaysLoadedEntityActor::RebuildEditorPreviewISMCs()
{
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

void AArcAlwaysLoadedEntityActor::DestroyEditorPreviewISMCs()
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

void AArcAlwaysLoadedEntityActor::RebuildGuidIndex()
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

bool AArcAlwaysLoadedEntityActor::FindInstanceByGuid(const FGuid& Guid, int32& OutEntryIndex, int32& OutTransformIndex) const
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

// -----------------------------------------------------------------------------
// Componentless editor render state
// -----------------------------------------------------------------------------

FMassEntityManager* AArcAlwaysLoadedEntityActor::GetEditorEntityManager() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	UMassEntitySubsystem* S = W->GetSubsystem<UMassEntitySubsystem>();
	return S ? &S->GetMutableEntityManager() : nullptr;
}

bool AArcAlwaysLoadedEntityActor::IsComponentlessEntry(const FArcPlacedEntityEntry& Entry) const
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

bool AArcAlwaysLoadedEntityActor::IsSkinnedComponentlessEntry(const FArcPlacedEntityEntry& Entry) const
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

#if WITH_EDITOR
FTransform AArcAlwaysLoadedEntityActor::GetEntryComponentTransform(const FArcPlacedEntityEntry& Entry) const
{
	// Returns the per-entry pivot offset that RebuildEditorRenderState pre-multiplies into the
	// instance transform, so the gizmo pivot matches the visible mesh.
	const UArcUnifiedMeshVisualizationTrait* Unified =
		ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
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

bool AArcAlwaysLoadedEntityActor::ResolveElementId(const FArcMassEntityElementId& InId, int32& OutEntryIndex, int32& OutTransformIndex) const
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

bool AArcAlwaysLoadedEntityActor::UpdateInstanceRender(int32 EntryIndex, int32 TransformIndex, const FTransform& WorldTransform)
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

void AArcAlwaysLoadedEntityActor::RebuildEditorRenderState()
{
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
		EnsureRenderReadinessPoll(true);
		return;
	}

	FMassEntityManager* Mgr = GetEditorEntityManager();
	if (!Mgr)
	{
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

	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];

		if (!IsComponentlessEntry(Entry))
		{
			continue;
		}

		const UArcUnifiedMeshVisualizationTrait* Unified =
			ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
		if (Unified == nullptr)
		{
			continue;
		}

		const FTransform EntryPivot = GetEntryComponentTransform(Entry);
		UStaticMesh* Mesh = Unified->GetSingleStaticMeshDirect();
		FArcMassVisRenderingProps RenderProps = Unified->GetSingleRenderingProps();
		TArray<TObjectPtr<UMaterialInterface>> ResolvedMats = Unified->GetSingleMaterialOverridesDirect();

		// Non-loading: an unloaded mesh defers the entry; RequestEditorMeshLoad has already
		// queued the async load and the readiness poll rebuilds when it lands.
		if (Mesh == nullptr || Mesh->IsCompiling()
			|| Mesh->GetRenderData() == nullptr || !Mesh->GetRenderData()->IsInitialized())
		{
			bAnyDeferred = true;
			continue;
		}

		// Build config fragments from the trait
		FArcMassStaticMeshConfigFragment StaticCfg(Mesh);

		FArcMassVisualizationMeshConfigFragment VisCfg;
		ArcAlwaysLoadedEntityActor_Componentless::BuildVisFragmentFromProps(RenderProps, VisCfg);

		FArcOverrideMaterialsFragment OverrideMats;
		OverrideMats.OverrideMaterials = MoveTemp(ResolvedMats);
		const bool bHasMats = OverrideMats.OverrideMaterials.Num() > 0;
		const FArcOverrideMaterialsFragment* OverrideMatsPtr = bHasMats ? &OverrideMats : nullptr;

		FArcStaticMeshFragment SMFrag = ArcMass::Visualization::ToEngineFragment(StaticCfg);
		FArcVisualizationMeshFragment VisFrag = ArcMass::Visualization::ToEngineFragment(VisCfg);

		FArcISMHolderKey Key;
		Key.Mesh = StaticCfg.Mesh;
		Key.ConfigHash = ArcAlwaysLoadedEntityActor_Componentless::ComputeConfigHash(VisFrag, OverrideMatsPtr);

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

	TSet<FMassEntityHandle> SkinnedTouchedHolders;

	for (int32 E = 0; E < Entries.Num(); ++E)
	{
		const FArcPlacedEntityEntry& Entry = Entries[E];

		if (!IsSkinnedComponentlessEntry(Entry))
		{
			continue;
		}

		const UArcUnifiedMeshVisualizationTrait* SkinnedUnified =
			ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
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
			bAnyDeferred = true;
			continue;
		}

		// Build shared fragments from the trait
		FArcMassSkinnedMeshFragment SkinnedMeshFrag;
		SkinnedMeshFrag.SkinnedAsset = Asset;
		SkinnedMeshFrag.TransformProvider = TransformProvider;

		FArcMassVisualizationMeshConfigFragment SkinnedVisCfg;
		ArcAlwaysLoadedEntityActor_Componentless::BuildVisFragmentFromProps(SkinnedRenderProps, SkinnedVisCfg);

		FArcOverrideMaterialsFragment SkinnedOverrideMats;
		SkinnedOverrideMats.OverrideMaterials = MoveTemp(SkinnedResolvedMats);
		const bool bSkinnedHasMats = SkinnedOverrideMats.OverrideMaterials.Num() > 0;
		const FArcOverrideMaterialsFragment* SkinnedOverrideMatsPtr = bSkinnedHasMats ? &SkinnedOverrideMats : nullptr;

		FArcVisualizationMeshFragment SkinnedVisFrag = ArcMass::Visualization::ToEngineFragment(SkinnedVisCfg);

		FArcSkinnedISMHolderKey SkinnedKey;
		SkinnedKey.Asset = Asset;
		SkinnedKey.ConfigHash = ArcAlwaysLoadedEntityActor_Componentless::ComputeConfigHash(SkinnedVisFrag, SkinnedOverrideMatsPtr);

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

	EnsureRenderReadinessPoll(bAnyDeferred);
}

void AArcAlwaysLoadedEntityActor::TearDownEditorRenderState()
{
	if (PendingRenderRebuildTickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PendingRenderRebuildTickHandle);
		PendingRenderRebuildTickHandle.Reset();
	}

	FMassEntityManager* Mgr = GetEditorEntityManager();

	if (!Mgr)
	{
		HolderBucket.Empty();
		InstanceToSourceEntity.Empty();
		return;
	}

	// Deselect each handle from the editor selection set before destroying its element.
	// DestroyElement resets the internal type ID to 0, but a still-selected handle leaves a stale
	// (DataPtr!=null, TypeId==0) entry that trips the "type ID not registered" ensure on the next
	// viewport interaction.
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
}

void AArcAlwaysLoadedEntityActor::RebuildAllEditorState()
{
	TearDownEditorRenderState();
	DestroyEditorPreviewISMCs();
	RebuildEditorRenderState();
	RebuildEditorPreviewISMCs();
}

void AArcAlwaysLoadedEntityActor::RequestEditorMeshLoad()
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
			ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
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

bool AArcAlwaysLoadedEntityActor::AnyDeferredComponentlessEntryNowReady() const
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
			ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
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
			ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
		USkinnedAsset* Asset = Unified != nullptr ? Unified->GetSingleSkinnedAssetDirect() : nullptr;
		if (Asset != nullptr && Asset->GetResourceForRendering() != nullptr)
		{
			return true;
		}
	}
	return false;
}

void AArcAlwaysLoadedEntityActor::EnsureRenderReadinessPoll(bool bNeeded)
{
	if (!bNeeded || PendingRenderRebuildTickHandle.IsValid())
	{
		return;
	}
	TWeakObjectPtr<AArcAlwaysLoadedEntityActor> WeakSelf(this);
	PendingRenderRebuildTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakSelf](float) -> bool
		{
			AArcAlwaysLoadedEntityActor* Self = WeakSelf.Get();
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
					UE_LOGF(LogMass, Warning,"AArcAlwaysLoadedEntityActor: post-compile rebuild gave up waiting on shader compilation");
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

void AArcAlwaysLoadedEntityActor::OnMaterialCompilationFinished(UMaterialInterface* /*Material*/)
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

bool AArcAlwaysLoadedEntityActor::GetMassEntityInstanceMeshBounds(const FArcMassEntityElementId& InId, FBoxSphereBounds& OutBounds) const
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
		ArcAlwaysLoadedEntityActor_Componentless::FindUnifiedTrait(Entry);
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

	bool bHasAny = false;
	FBoxSphereBounds Aggregate(ForceInit);

	if (Unified != nullptr && Unified->GetInferredMode() == EArcUnifiedMeshVisMode::Composite)
	{
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
	}

	if (!bHasAny)
	{
		return false;
	}
	OutBounds = Aggregate;
	return true;
}

void AArcAlwaysLoadedEntityActor::PostEditUndo()
{
	Super::PostEditUndo();
	RebuildAllEditorState();
}

#endif // WITH_EDITOR
