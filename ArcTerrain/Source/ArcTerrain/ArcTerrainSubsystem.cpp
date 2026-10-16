// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainDebugDraw.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainSettings.h"
#include "ArcTerrainInvokers.h"
#include "ArcTerrainModule.h"
#include "Collision/ArcTerrainCollisionBuilder.h"
#include "Collision/ArcTerrainCollisionComponent.h"
#include "Collision/ArcTerrainHeightfieldRef.h"
#include "Composite/ArcTerrainCompositeShaders.h"
#include "Composite/ArcTerrainCpuComposite.h"
#include "DataMaps/ArcTerrainDataMapSample.h"
#include "Lighting/ArcTerrainFarOcclusionComponent.h"
#include "Lighting/ArcTerrainLightingComponent.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainCellMath.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Render/ArcTerrainHeightBounds.h"
#include "Render/ArcTerrainRenderComponent.h"
#include "Render/ArcTerrainVertexFactory.h"
#include "Stamps/ArcTerrainMeshModifier.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Surface/ArcTerrainSurfaceSample.h"
#include "Weights/ArcTerrainWeightSlicePacking.h" // GArcTerrainWeightMaxSlices (atlas slice budget)
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h" // FTextureSource dims (native density resolve) + CPU source cache
#include "Engine/Texture2DArray.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PooledRenderTarget.h"
#include "RHIGPUReadback.h"
#include "RHIGlobals.h"
#include "RenderResource.h"
#include "RenderCommandFence.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/ObjectKey.h"

#include <atomic>

#if WITH_EDITOR
#include "ArcTerrainTileBake.h" // tile lattice + ComputeTileCacheKey
#include "ArcTerrainTileBlobs.h" // the three rails' DDC payloads
#include "ArcTerrainTileCache.h" // async DDC probe handle polling (height + weight bake rails)
#include "ArcTerrainTileIO.h"
#include "ArcTerrainTileNames.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "DataMaps/ArcTerrainDataMapKey.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Subsystems/ImportSubsystem.h"
#include "Weights/ArcTerrainWeightBuilder.h" // FArcTerrainWeightPlane + ComputeCellWeightKey
#include "UObject/UnrealType.h" // FPropertyChangedEvent + GET_MEMBER_NAME_CHECKED (settings-changed handler)
#endif

UArcTerrainSubsystem::~UArcTerrainSubsystem() = default;

namespace ArcTerrainRenderOwnershipLocal_4f61
{
	std::atomic<uint64> GNextWorldInstanceToken { 1 };
	std::atomic<uint64> GNextTerrainInstanceId { 1 };
	std::atomic<uint64> GNextOwnerEpoch { 1 };
}

void UArcTerrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RenderTargetPool.Initialize(*this);
	PreviewManager.Initialize(*this);
	WorldInstanceToken = ArcTerrainRenderOwnershipLocal_4f61::GNextWorldInstanceToken.fetch_add(
		1, std::memory_order_relaxed);
	TryClaimRendererWorld();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UArcTerrainSubsystem::HandleWorldCleanup);
#if WITH_EDITOR
	CollisionMailbox = MakeShared<ArcTerrainCollision::FArcTerrainCollisionCompletionMailbox, ESPMode::ThreadSafe>();
#endif

#if WITH_EDITOR
	// The cache budgets only trim on insert, so a live lowering of SourceCacheMB in Project Settings
	// would not reclaim until the next source upload — subscribe to force an immediate re-trim. Every
	// other knob is read fresh per-tick and needs no delegate.
	SettingsChangedHandle = GetMutableDefault<UArcTerrainSettings>()->OnSettingChanged().AddUObject(
		this, &UArcTerrainSubsystem::HandleTerrainSettingsChanged);
	if (GEditor != nullptr)
	{
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			AssetReimportHandle = ImportSubsystem->OnAssetReimport.AddUObject(
				this, &UArcTerrainSubsystem::HandleAssetReimport);
		}
	}
#endif
}

void UArcTerrainSubsystem::Deinitialize()
{
	if (FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet())
	{
		Coordinator->AbandonOwner(GetRuntimeOwnerId());
	}
	ReleasePendingAtlasFills(0);
	RetireRendererOwnership(true, false);
	ClearStreamReleaseTimer();
#if WITH_EDITOR
	bBakeShuttingDown.store(true, std::memory_order_release);
	ClearBakeRetryTimer();
	ArcTerrainDiag::CancelBakeProgress(GetWorld());
#endif
#if WITH_EDITOR
	if (SettingsChangedHandle.IsValid())
	{
		GetMutableDefault<UArcTerrainSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}
	if (AssetReimportHandle.IsValid())
	{
		if (GEditor != nullptr)
		{
			if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
			{
				ImportSubsystem->OnAssetReimport.Remove(AssetReimportHandle);
			}
		}
		AssetReimportHandle.Reset();
	}
#endif

	// Teardown ordering (pattern: ArcRenderStateHelper.cpp — consumers out before resources die):
	// 0a. Collision first: drop in-flight builds (each render-thread poll chain owns its
	//     FRHIGPUTextureReadback and destroys it ON the render thread when the chain ends — the PCG
	//     lifetime rule; abandoning our TSharedPtr here never touches RHI objects from the game
	//     thread) and destroy the physics components + heightfield refs (pure CPU Chaos geometry,
	//     no render-thread affinity — safe before the flush; the flush in step 2 also drains any
	//     still-executing readback copies).
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		for (TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair :
			TerrainPair.Value.CollisionPatches)
		{
			DestroyPatchCollision(PatchPair.Value);
		}
		TerrainPair.Value.CollisionPatches.Empty();
		ArcTerrainCollisionIndex_e7b2::ClearScheduling(TerrainPair.Value);
		for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& LightingPair :
			TerrainPair.Value.LightingPatches)
		{
			DestroyLightingPatch(LightingPair.Value);
		}
		TerrainPair.Value.LightingPatches.Empty();
		ArcTerrainLightingIndex_6b19::ClearScheduling(TerrainPair.Value);
		for (TPair<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch>& FarPair :
			TerrainPair.Value.FarBlockPatches)
		{
			DestroyFarBlockPatch(FarPair.Value);
		}
		TerrainPair.Value.FarBlockPatches.Empty();
		ArcTerrainFarBlockIndex_2c08::ClearScheduling(TerrainPair.Value);
		// 0a2. Drop the atlas publication tags before the cell states are destroyed.
		for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : TerrainPair.Value.Cells)
		{
			ArcTerrainSubsystemPrivate::ResetCellAtlasPublication(CellPair.Value);
		}
	}

	PreviewManager.Shutdown();

	// 1. Release every tile to the pool (no cell references RTs anymore).
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : TerrainPair.Value.Cells)
		{
			CellPair.Value.TileSet.Release(RenderTargetPool);
		}
	}
	Terrains.Empty();
	TerrainsWithDirtyCells.Empty();
#if WITH_EDITOR
	NextBakeTerrain.Reset();
	ClearBakeRetryTimer();
	CollisionMailbox.Reset();
#endif

	// 2. Drain in-flight composite/upload render commands that still reference RT resources and
	//    source-cache entries.
	FlushRenderingCommands();

	// 3. Now the RHI refs and UObjects can go: cache entries release their FTextureRHIRefs, the
	//    pool free-lists drop, and un-rooting the RTs lets GC destroy them.
	SourceCache.Empty();
	RenderTargetPool.Reset();
	OwnedRenderTargets.Empty();

	ArcTerrainDiag::ReleaseWorld(GetWorld());
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}

	Super::Deinitialize();
}

void UArcTerrainSubsystem::HandleWorldCleanup(UWorld* World, bool, bool)
{
	if (World == nullptr || World != GetWorld())
	{
		return;
	}
	if (FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet())
	{
		Coordinator->AbandonOwner(GetRuntimeOwnerId());
	}
	ReleasePendingAtlasFills(0);
	RetireRendererOwnership(true, true);
}

void UArcTerrainSubsystem::TryClaimRendererWorld()
{
	if (bRendererOwnershipReleased)
	{
		return;
	}

	UWorld* World = GetWorld();
	FSceneInterface* Scene = World != nullptr ? World->Scene : nullptr;
	if (Scene == nullptr || ClaimedRendererSceneKey == Scene)
	{
		return;
	}

	if (ClaimedRendererSceneKey != nullptr)
	{
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			InvalidateCBTBaseMesh(TerrainPair.Value);
		}
	}

	FArcTerrainCBTWorldClaim Claim;
	Claim.WorldInstanceToken = WorldInstanceToken;
	Claim.WorldType = static_cast<EWorldType::Type>(World->WorldType);
	Claim.SceneKey = Scene;
	GArcTerrainCBTRendererExtension.ClaimWorld_GameThread(Claim);
	ClaimedRendererSceneKey = Scene;
}

void UArcTerrainSubsystem::RetireRendererOwnership(bool bReleaseWorld, bool bWaitForRenderThread)
{
	if (!bRendererOwnershipReleased)
	{
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			FArcTerrainState& State = TerrainPair.Value;
			if (State.RenderOwner.IsValid())
			{
				GArcTerrainCBTRendererExtension.RetireTerrain_GameThread(State.RenderOwner);
			}
		}
		if (bReleaseWorld && WorldInstanceToken != 0 && ClaimedRendererSceneKey != nullptr)
		{
			GArcTerrainCBTRendererExtension.ReleaseWorld_GameThread(WorldInstanceToken);
		}
		bRendererOwnershipReleased = bReleaseWorld;

	}

	if (bWaitForRenderThread)
	{
		PreviewManager.Shutdown();
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			if (AArcTerrain* Terrain = TerrainPair.Key.Get())
			{
				ArcTerrainTileAccess::ReleaseTerrainTiles(*Terrain);
			}
			for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : TerrainPair.Value.Cells)
			{
				CellPair.Value.TileSet.Release(RenderTargetPool);
				ArcTerrainSubsystemPrivate::ResetCellAtlasPublication(CellPair.Value);
			}
			TerrainPair.Value.TileAtlas.Reset();
		}
		FRenderCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();
	}
}

void UArcTerrainSubsystem::RefreshRendererRhiEpoch()
{
	const uint64 CurrentRhiEpoch = GArcTerrainCBTRendererExtension.GetRhiEpoch();
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		FArcTerrainState& State = TerrainPair.Value;
		if (State.LastObservedRhiEpoch == CurrentRhiEpoch)
		{
			continue;
		}
		State.LastObservedRhiEpoch = CurrentRhiEpoch;
		State.CbtPublishCache = FArcTerrainCBTPublishCache();
		State.PublishedRenderIdentity = FArcTerrainRenderIdentity();
		if (AArcTerrain* Terrain = TerrainPair.Key.Get())
		{
			PreviewManager.InvalidateBindingSignature(*Terrain);
		}
	}
}

FArcTerrainRuntimeOwnerId UArcTerrainSubsystem::GetRuntimeOwnerId() const
{
	FArcTerrainRuntimeOwnerId Id;
	Id.Value = GetUniqueID();
	return Id;
}

void UArcTerrainSubsystem::PublishRuntimeLaneLimits()
{
	FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet();
	if (Coordinator == nullptr)
	{
		return;
	}
	const FArcTerrainRuntimeOwnerId OwnerId = GetRuntimeOwnerId();
	const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
	const int32 AtlasLimit = Settings != nullptr
		? Settings->RuntimeAtlasLaunchesPerFrame
		: FArcTerrainRuntimeWorkCoordinator::DefaultAtlasLaunchesPerFrame;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		const uint64 TerrainId = TerrainPair.Value.CollisionTerrainId;
		if (TerrainId == 0)
		{
			continue;
		}
		Coordinator->SetTerrainLaneLaunchLimit(OwnerId, TerrainId, EArcTerrainRuntimeLane::Atlas, AtlasLimit);
		const int32 LightingLimit = Terrain != nullptr
			? FMath::Max(Terrain->MaxLightingBuildsPerFrame, 0)
			: FArcTerrainRuntimeWorkCoordinator::DefaultLightingLaunchesPerFrame;
		const int32 FarLimit = Terrain != nullptr
			? FMath::Max(Terrain->MaxFarBlockBuildsPerFrame, 0)
			: FArcTerrainRuntimeWorkCoordinator::DefaultFarLaunchesPerFrame;
		Coordinator->SetTerrainLaneLaunchLimit(OwnerId, TerrainId, EArcTerrainRuntimeLane::Lighting,
			LightingLimit);
		Coordinator->SetTerrainLaneLaunchLimit(OwnerId, TerrainId, EArcTerrainRuntimeLane::Far, FarLimit);
	}
}

void UArcTerrainSubsystem::DrainRuntimeCompletions()
{
	FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet();
	if (Coordinator == nullptr)
	{
		return;
	}
	Coordinator->DrainGrantedCompletions(GetRuntimeOwnerId(),
		[this](FArcTerrainRuntimeCompletion&& Completion)
		{
			switch (Completion.Lane)
			{
			case EArcTerrainRuntimeLane::Atlas:
				ApplyAtlasRuntimeCompletion(MoveTemp(Completion));
				break;
			case EArcTerrainRuntimeLane::Lighting:
				ApplyLightingRuntimeCompletion(MoveTemp(Completion));
				break;
			case EArcTerrainRuntimeLane::Far:
				ApplyFarRuntimeCompletion(MoveTemp(Completion));
				break;
			default:
				Completion.Reservation.Release();
				break;
			}
		});
}

#if WITH_EDITOR
void UArcTerrainSubsystem::HandleTerrainSettingsChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainSettings, SourceCacheMB)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainSettings, SourceCachePinMB))
	{
		// The source cache only trims on insert; force it to reclaim to the new (resolved) budget now.
		// Pins shed FIRST: an unevictable entry is bytes the trim cannot reclaim.
		SourceCache.ShedPinnedCpuFloatsToBudget();
		SourceCache.ReEnforceBudgets();
	}
	// All scheduler knobs are read fresh per-tick, so they self-apply next frame without any
	// handling here.
}

namespace ArcTerrainTileSeedLocal_c9f2
{
	void AccumulateAffectedTiles(FArcTerrainState& State, TConstArrayView<FArcTerrainBakeTileRef> Affected);
}

namespace ArcTerrainReimportLocal_4c19
{
	bool StampReferencesTexture(const UArcTerrainStampComponent& Stamp, const UTexture2D& Texture)
	{
		FArcStampSourceDesc Desc;
		if (Stamp.GetSourceDescriptor(Desc))
		{
			if (Desc.Texture == &Texture)
			{
				return true;
			}
			for (const TPair<FName, FArcStampLayerDesc>& Layer : Desc.Layers)
			{
				if (Layer.Value.Texture == &Texture)
				{
					return true;
				}
			}
		}

		const UArcTerrainTexturePatchModifier* Modifier = Cast<UArcTerrainTexturePatchModifier>(&Stamp);
		if (Modifier == nullptr)
		{
			return false;
		}
		for (const TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>& SourceEntry :
			Modifier->GetDataMapSources())
		{
			if (SourceEntry.Value != nullptr && SourceEntry.Value->Texture == &Texture)
			{
				return true;
			}
		}
		return false;
	}
}

void UArcTerrainSubsystem::HandleAssetReimport(UObject* ReimportedAsset)
{
	const UTexture2D* ReimportedTexture = Cast<UTexture2D>(ReimportedAsset);
	if (ReimportedTexture == nullptr)
	{
		return;
	}

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		FArcTerrainState& State = TerrainPair.Value;
		for (FArcTerrainStampEntry& Entry : State.Stamps)
		{
			const UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
			if (Stamp == nullptr)
			{
				continue;
			}
			if (ArcTerrainReimportLocal_4c19::StampReferencesTexture(*Stamp, *ReimportedTexture))
			{
				RefreshStampWeightSourceDim(State, Entry);
				if (!State.bDerivedGridRebuildPending)
				{
					ArcTerrainTileSeedLocal_c9f2::AccumulateAffectedTiles(State, Entry.CoveredTiles);
				}
			}
		}
		if (State.bDerivedGridRebuildPending)
		{
			if (AArcTerrain* Terrain = TerrainPair.Key.Get())
			{
				FlushPendingDerivedGridRebuild(*Terrain, State);
			}
		}
	}
}
#endif // WITH_EDITOR

void UArcTerrainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	// PIE-ENTRY RULE. This subsystem instance is FRESH — world subsystems are per-world, so NOTHING
	// migrates from the editor world's instance. In this game world the fast-edit machinery stays
	// suppressed (edit pins: OnStampChanged returns early for game worlds; the selection-pin phase 0
	// is editor-world-only) and the section lifecycle NEVER bakes — it only LOADS what the editor
	// built.

	// PIE/game-entry SYNC: pawns don't exist yet, so anchor on every APlayerStart and build the
	// collision patches inside the ring BEFORE physics needs them (spawned pawns must not
	// fall through). Late anchors (possession after BeginPlay, spawn-point overrides) are covered
	// by the tick-side sync window — see Tick phase 4.
	SyncCollisionWindowEndTime =
		InWorld.GetTimeSeconds() + ArcTerrainSubsystemPrivate::GSyncCollisionWindowSeconds;

	TArray<FVector> Anchors;
	for (TActorIterator<APlayerStart> It(&InWorld); It; ++It)
	{
		Anchors.Add(It->GetActorLocation());
	}

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			continue;
		}
		const double CollisionRadius =
			ArcTerrainSubsystemPrivate::ResolveResidency(*Terrain).CollisionRadius;
		for (const FVector& Anchor : Anchors)
		{
			SyncBuildCollisionAroundAnchor(*Terrain, TerrainPair.Value, Anchor, CollisionRadius);
		}
	}
}

bool UArcTerrainSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

namespace ArcTerrainPipelineDiagLocal_73d4
{
	bool Contains(const ArcTerrainSubsystemPrivate::FSectorWindow& Window, FIntPoint Key)
	{
		return Key.X >= Window.MinX && Key.X <= Window.MaxX && Key.Y >= Window.MinY && Key.Y <= Window.MaxY;
	}

	bool HasHeightInFlight(const FArcTerrainAtlasSchedule& Schedule, FArcTerrainSectorId SectorId)
	{
		for (const FArcTerrainAtlasInFlightKey& Key : Schedule.InFlight)
		{
			if (Key.SectorId == SectorId
				&& (Key.Kind == EArcTerrainAtlasPrepareKind::HeightCell
					|| Key.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse))
			{
				return true;
			}
		}
		return false;
	}

	uint32 HeightPayloadId(const AArcTerrain& Terrain, const FArcTerrainState& State,
		FArcTerrainSectorId SectorId)
	{
		if (!ArcTerrainTileAccess::IsValidTier(SectorId.Tier))
		{
			return 0;
		}
		FArcTerrainTileGrid Grid;
		if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, SectorId.Tier, Grid)
			|| !Grid.IsValid())
		{
			return 0;
		}
		FIntPoint TileKey = FIntPoint::ZeroValue;
		if (SectorId.Tier == 0)
		{
			if (!State.TileAtlas.IsValid() || !State.TileAtlas->GetLayout().IsValid())
			{
				return 0;
			}
			const int32 SectorShift = State.TileAtlas->GetLayout().SectorShift;
			const FArcTerrainCellKey CellKey(SectorId.Key.X << SectorShift, SectorId.Key.Y << SectorShift);
			TileKey = ArcTerrainTileAccess::TileKeyFromCell(Grid, CellKey);
		}
		else
		{
			if (!State.TierLadder.Tiers.IsValidIndex(SectorId.Tier))
			{
				return 0;
			}
			TileKey = ArcTerrainSubsystemPrivate::CoarseSectorTileKey(Grid, SectorId.Key,
				State.TierLadder.Tiers[SectorId.Tier].CellSectorShift);
		}
		const FArcTerrainTilePayloadPtr Payload = ArcTerrainTileAccess::FindLoadedTile(
			Terrain, EArcTerrainRail::Height, SectorId.Tier, TileKey);
		return Payload.IsValid() ? Payload->PayloadId : 0;
	}

	bool IsTileSourceReadyWithoutDisk(const AArcTerrain& Terrain, int32 Tier, FIntPoint TileKey)
	{
		TArray<EArcTerrainTileResidency> Residency;
		ArcTerrainTileAccess::GatherTileResidency(Terrain, EArcTerrainRail::Height, Tier,
			TileKey, TileKey, Residency);
		if (Residency.Num() != 1)
		{
			return false;
		}
		return Residency[0] == EArcTerrainTileResidency::Resident
			|| Residency[0] == EArcTerrainTileResidency::Absent
			|| Residency[0] == EArcTerrainTileResidency::Unusable;
	}

	bool IsPendingHeightSourceReady(const AArcTerrain& Terrain, const FArcTerrainState& State,
		FArcTerrainSectorId SectorId)
	{
		FArcTerrainTileGrid Grid;
		if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, SectorId.Tier, Grid)
			|| !Grid.IsValid())
		{
			return SectorId.Tier == 0;
		}
		if (SectorId.Tier != 0)
		{
			if (!State.TierLadder.Tiers.IsValidIndex(SectorId.Tier))
			{
				return false;
			}
			const FIntPoint TileKey = ArcTerrainSubsystemPrivate::CoarseSectorTileKey(Grid, SectorId.Key,
				State.TierLadder.Tiers[SectorId.Tier].CellSectorShift);
			const FArcTerrainTilePayloadPtr Payload = ArcTerrainTileAccess::FindLoadedTile(
				Terrain, EArcTerrainRail::Height, SectorId.Tier, TileKey);
			return Payload.IsValid();
		}

		if (!State.TileAtlas.IsValid() || !State.TileAtlas->GetLayout().IsValid())
		{
			return false;
		}
		const int32 SectorShift = State.TileAtlas->GetLayout().SectorShift;
		const int32 CellsPerSectorEdge = 1 << SectorShift;
		for (int32 OffsetY = 0; OffsetY < CellsPerSectorEdge; ++OffsetY)
		{
			for (int32 OffsetX = 0; OffsetX < CellsPerSectorEdge; ++OffsetX)
			{
				const FArcTerrainCellKey CellKey(
					(SectorId.Key.X << SectorShift) + OffsetX,
					(SectorId.Key.Y << SectorShift) + OffsetY);
				const FArcTerrainCellState* Cell = State.Cells.Find(CellKey);
				if (Cell == nullptr || Cell->Version > 0)
				{
					continue;
				}
				const FIntPoint TileKey = ArcTerrainTileAccess::TileKeyFromCell(Grid, CellKey);
				if (!IsTileSourceReadyWithoutDisk(Terrain, 0, TileKey))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool IsCollisionInsideGate(const FArcTerrainState& State, FArcTerrainCollisionKey Key,
		bool bHasComponent)
	{
		if (State.CollisionBootstrapKeys.Contains(Key))
		{
			return true;
		}
		for (const FArcTerrainCachedInvokerWindow& Invoker : State.CollisionSelectionCache.Invokers)
		{
			const ArcTerrainSubsystemPrivate::FSectorWindow& Window =
				bHasComponent ? Invoker.OuterWindow : Invoker.InnerWindow;
			if (Contains(Window, Key))
			{
				return true;
			}
		}
		return false;
	}

	void Gather(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FArcTerrainTileStoreStats& StoreStats, const UArcTerrainSettings& Settings,
		int32 PendingAtlasFills, FArcTerrainPipelineSnapshot& Snapshot)
	{
		Snapshot.WorldLabel = Terrain.GetWorld() != nullptr
			? Terrain.GetWorld()->GetDebugDisplayName()
			: FString(TEXT("<null>"));
		Snapshot.TerrainPath = Terrain.GetPathName();
		Snapshot.StateAgeTicks = State.PipelineTelemetry.StateAgeTicks;
		Snapshot.Interval = State.PipelineTelemetry.Interval;
		Snapshot.Lifetime = State.PipelineTelemetry.Lifetime;
		Snapshot.Store = StoreStats;
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			if (ArcTerrainTileAccess::IsValidTier(Sector.Id.Tier))
			{
				++Snapshot.ResidentHeightByTier[Sector.Id.Tier];
			}
		}
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			if (ArcTerrainTileAccess::IsValidTier(Group.Tier))
			{
				++Snapshot.ResidentWeightByTier[Group.Tier];
			}
		}

		const FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
		Snapshot.PendingAtlasAuthorityEvents = State.PendingAtlasAuthorityEvents.Num();
		Snapshot.WokenHeightAdmissions = Schedule.WokenHeightAdmissions.Num();
		Snapshot.WokenWeightAdmissions = Schedule.WokenWeightAdmissions.Num();
		Snapshot.DirtyHeightSectors = Schedule.DirtyHeightSectors.Num();
		Snapshot.DirtyWeightGroups = Schedule.DirtyWeightGroups.Num();
		Snapshot.DirtyAtlasCells = Schedule.DirtyAtlasCells.Num();
		Snapshot.PendingRefillCells = State.PendingRefillCells.Num();
		Snapshot.AtlasInFlight = Schedule.InFlight.Num();
		Snapshot.PendingAtlasFills = PendingAtlasFills;
		Snapshot.bCbtTopologyInFlight = Schedule.bCbtTopologyInFlight;
		Snapshot.bCbtUnpublished = !State.CbtPublishCache.bPublished;
		Snapshot.bResidencyRevisionLag =
			Schedule.ResidencyPublicationRevision != Schedule.ConsumedResidencyPublicationRevision;
		Snapshot.bDesiredAdmissionRevisionLag =
			Schedule.DesiredAdmissionRevision != Schedule.ConsumedDesiredAdmissionRevision;
		Snapshot.bLayoutGenerationLag = Schedule.LayoutGeneration != Schedule.ConsumedLayoutGeneration;
		Snapshot.bRenderKnobRevisionLag = Schedule.RenderKnobRevision != Schedule.ConsumedRenderKnobRevision;

		for (const FArcTerrainSectorId& SectorId : Schedule.PendingHeightAdmissions)
		{
			if (IsPendingHeightSourceReady(Terrain, State, SectorId))
			{
				++Snapshot.PendingHeightSourceReady;
			}
			else
			{
				++Snapshot.PendingHeightSourceMissing;
			}
		}

		if (State.TileAtlas.IsValid())
		{
			TArray<FArcTerrainSectorId> Admitted;
			State.TileAtlas->GetAdmittedSectors(Admitted);
			for (const FArcTerrainSectorId& SectorId : Admitted)
			{
				Snapshot.AdmittedHeightReady += State.TileAtlas->IsSectorRingReady(SectorId) ? 1 : 0;
				Snapshot.AdmittedHeightAwaiting += State.TileAtlas->IsSectorAwaitingTile(SectorId) ? 1 : 0;
				const uint32 PayloadId = HeightPayloadId(Terrain, State, SectorId);
				if (PayloadId != 0
					&& PayloadId != State.TileAtlas->GetSectorHeightTileFingerprint(SectorId)
					&& !Schedule.DirtyHeightSectors.Contains(SectorId)
					&& !HasHeightInFlight(Schedule, SectorId))
				{
					++Snapshot.StaleSleeping;
				}
			}
			Snapshot.bCbtResourceGenerationMatch = State.CbtPublishCache.ResourceGeneration
				== State.TileAtlas->GetResourceGeneration();
		}
		Snapshot.bCbtPublished = State.CbtPublishCache.bPublished;

		for (const TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : State.Cells)
		{
			const FArcTerrainCellState& Cell = CellPair.Value;
			if (Cell.Version > 0)
			{
				if (Cell.AtlasHeightVersion == Cell.Version)
				{
					++Snapshot.CellsCompositePublished;
				}
				else
				{
					++Snapshot.CellsCompositeLagging;
				}
			}
			if (Cell.HeightTileSerial > 0)
			{
				if (Cell.AtlasHeightTileSerial == Cell.HeightTileSerial)
				{
					++Snapshot.CellsTilePublished;
				}
				else
				{
					++Snapshot.CellsTileLagging;
				}
			}
			Snapshot.CellsRefillPending += Cell.bAtlasRefillPending ? 1 : 0;
			Snapshot.CellsCompositeRetry += Cell.bCompositeResolveRetryPending ? 1 : 0;
		}

		Snapshot.CollisionPatches = State.CollisionPatches.Num();
		Snapshot.CollisionReadySet = State.CollisionReadySet.Num();
		Snapshot.CollisionReadyOrdered = State.CollisionReadyOrdered.Num();
		Snapshot.CollisionRetry = State.CollisionRetryHeap.Num();
		Snapshot.CollisionAuthorityEvents = State.PendingCollisionAuthorityEvents.Num();
		const double Now = FPlatformTime::Seconds();
		for (const TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair : State.CollisionPatches)
		{
			const FArcTerrainCollisionPatch& Patch = PatchPair.Value;
			const bool bHasComponent = Patch.CollisionComponent.IsValid();
			Snapshot.CollisionComponents += bHasComponent ? 1 : 0;
			Snapshot.CollisionHeightSourceLatches += Patch.bCollisionHeightSourceMissing ? 1 : 0;
			bool bNeedsBuild = Patch.AppliedDirtySerial != Patch.DirtySerial || !bHasComponent;
#if WITH_EDITOR
			bNeedsBuild = bNeedsBuild || (Patch.bCollisionBuiltIncomplete && Now >= Patch.CollisionRetryAfter);
#endif
			if (bNeedsBuild && !Patch.bCollisionHeightSourceMissing
				&& IsCollisionInsideGate(State, PatchPair.Key, bHasComponent)
				&& !State.CollisionReadySet.Contains(PatchPair.Key))
			{
				++Snapshot.CollisionEligibleButUnqueued;
			}
		}
		Snapshot.CollisionBudgetCount = FMath::Max(Settings.MaxCompositesPerFrame, 1);
		Snapshot.CollisionBudgetMs = Settings.CompositeBudgetMs;
	}
}

void UArcTerrainSubsystem::Tick(float DeltaTime)
{
	using namespace ArcTerrainSubsystemPrivate;

	UWorld* World = GetWorld();
	ArcTerrainDiag::SetTickingWorld(World);
	TryClaimRendererWorld();
	RefreshRendererRhiEpoch();

	TArray<FVector> InvokerPositions;
	int32 NumSubstitutedInvokers = 0;
	ArcTerrain::GatherInvokers(World, InvokerCache, InvokerPositions, NumSubstitutedInvokers);

	// The scheduler budgets are read fresh each frame so a Project Settings edit auto-applies next tick
	// with no delegate; every shape knob is a per-terrain AArcTerrain property, re-resolved inside each
	// phase loop through ResolveResidency for the same reason.
	const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
	const bool bInSyncWindow = World != nullptr && World->IsGameWorld()
		&& World->GetTimeSeconds() < SyncCollisionWindowEndTime;
	const double Now = FPlatformTime::Seconds();

	const double PhaseStart0 = FPlatformTime::Seconds();

	PublishRuntimeLaneLimits();
	DrainRuntimeCompletions();
	TickEditPins(Now);

	const double PhaseStart1 = FPlatformTime::Seconds();

	TArray<FCompositeCandidate> Candidates;
	int32 NumSectorCells = 0;
	int32 NumGrantedSectors = 0;
	int32 NumEvictCandidates = 0;
	int32 NumEvicted = 0;
	int32 NumHeldByPin = 0;
	int32 NumHeldByQueue = 0;
	int32 NumCompositeCandidates = 0;
	int32 NumCompositeAttempted = 0;
	int32 NumCompositeDone = 0;
#if WITH_EDITORONLY_DATA
	const int32 MaxCompositesPerFrame = FMath::Max(Settings->MaxCompositesPerFrame, 1);
#endif
	TickResidency(InvokerPositions, Now, *Settings, Candidates,
		NumSectorCells, NumGrantedSectors, NumEvictCandidates, NumEvicted,
		NumHeldByPin, NumHeldByQueue, NumCompositeCandidates);

	const double PhaseStartTileStream = FPlatformTime::Seconds();

	// ---- Phase 1b: the disk stream. Independent of everything phase 1 decided — its radius is its own
	// and its unit is the tile, not the sector.
	TickTileStreaming(InvokerPositions);

	// ---- Phase 1c: the lighting ring — its own lattice and radius, no cell and no atlas slice.
	double LightingSeconds = 0.0;
	double FarBlockSeconds = 0.0;
	TickLightingPatches(InvokerPositions, LightingSeconds, FarBlockSeconds);

	const double PhaseStart2 = FPlatformTime::Seconds();

#if WITH_EDITORONLY_DATA
	// ---- Phase 2: budgeted composites — edit-pinned cells first (the artist is actively working
	// them), then nearest-to-invoker (deadline + count budget, pattern: ArcTQSQuerySubsystem::Tick).
	// At least one dirty cell always composites so a tiny budget can't starve the queue.
	// EDITOR-ONLY: the composite reads stamp rasters out of FTextureSource, which no cooked build has.
	// The cooked height authority is the baked section, attached to the slice by phase 2b's fill.
	const double CompositeLoopStart = FPlatformTime::Seconds();
	const double Deadline =
		CompositeLoopStart + static_cast<double>(Settings->CompositeBudgetMs) / 1000.0;
	for (const FCompositeCandidate& Candidate : Candidates)
	{
		// A failed attempt still costs budget: CompositeCell can early-out without composing,
		// so only the deadline (not the success count) can bound a run of futile attempts.
		if (NumCompositeDone >= MaxCompositesPerFrame
			|| (NumCompositeAttempted > 0 && FPlatformTime::Seconds() >= Deadline))
		{
			break;
		}

		++NumCompositeAttempted;
		FArcTerrainCellState& Cell = Candidate.State->Cells.FindChecked(Candidate.Key);
		const double CompositeCallStart = FPlatformTime::Seconds();
		const bool bComposited = CompositeCell(*Candidate.Terrain, *Candidate.State, Candidate.Key, Cell);
		ArcTerrainDiag::NoteBlockingGameThread(TEXT("composite cell"), Candidate.Key,
			FPlatformTime::Seconds() - CompositeCallStart);
		if (bComposited)
		{
			FinalizeCompositeAttempt(*Candidate.State, Candidate.Key, Candidate.State->Hot(Cell), Cell, Now);
			++NumCompositeDone;
		}
	}
#endif // WITH_EDITORONLY_DATA

	const double PhaseStart2b = FPlatformTime::Seconds();

	// ---- Phase 2b: tile-atlas admission — after the composites so a cell composited THIS tick lands in
	// its slice the same frame.
	TickTileAtlases();

	const double PhaseStart3 = FPlatformTime::Seconds();

	int32 NumCollisionBuilds = 0;
	int32 NumCollisionAttempts = 0;
	TickCollision(InvokerPositions, bInSyncWindow, *Settings, NumCollisionBuilds, NumCollisionAttempts);

	const double PhaseStart4 = FPlatformTime::Seconds();

	if (bInSyncWindow && World != nullptr)
	{
		for (const FVector& InvokerPosition : InvokerPositions)
		{
			for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
			{
				AArcTerrain* Terrain = TerrainPair.Key.Get();
				if (Terrain == nullptr)
				{
					continue;
				}
				const double ProbePatchSize = Terrain->CollisionPatchSize();
				if (ProbePatchSize <= 0.0)
				{
					continue;
				}
				const FVector Local = Terrain->WorldToTerrainLocal(InvokerPosition);
				const FBox ProbeBounds(
					Local - FVector(GPawnCellProbeExtent, GPawnCellProbeExtent, 0.0),
					Local + FVector(GPawnCellProbeExtent, GPawnCellProbeExtent, 0.0));
				TArray<FArcTerrainCollisionKey> ProbePatches;
				ArcTerrain::CollisionPatchesIntersectingLocalBounds(ProbeBounds, ProbePatchSize,
					ProbePatches);
				for (FArcTerrainCollisionKey ProbeKey : ProbePatches)
				{
					FArcTerrainCollisionPatch& Patch =
						ArcTerrainCollisionIndex_e7b2::EnsurePatch(TerrainPair.Value, ProbeKey);
					ArcTerrainCollisionIndex_e7b2::NoteBootstrapKey(TerrainPair.Value, ProbeKey);
					SyncBuildPatchCollision(*Terrain, TerrainPair.Value, ProbeKey, Patch);
				}
			}
		}
	}

	const double PhaseStart5 = FPlatformTime::Seconds();

	// ---- Phase 5: settle counters + the on-disk tile bakes — after the composites (a bake reads this
	// tick's tile) and after collision.
	TickCellBakeLifecycle(World, InvokerPositions);

	const double PhaseEnd = FPlatformTime::Seconds();

	ArcTerrainDiag::FTimings TickTimings;
	TickTimings.TotalSeconds = PhaseEnd - PhaseStart0;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::EditPins)] =
		PhaseStart1 - PhaseStart0;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::Residency)] =
		PhaseStartTileStream - PhaseStart1;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::TileStream)] =
		PhaseStart2 - PhaseStartTileStream - LightingSeconds - FarBlockSeconds;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::Lighting)] =
		LightingSeconds;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::FarBlocks)] =
		FarBlockSeconds;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::Composite)] =
		PhaseStart2b - PhaseStart2;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::Atlas)] =
		PhaseStart3 - PhaseStart2b;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::Collision)] =
		PhaseStart4 - PhaseStart3;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::SyncAnchor)] =
		PhaseStart5 - PhaseStart4;
	TickTimings.PhaseSeconds[static_cast<int32>(ArcTerrainDiag::EPhase::Bake)] = PhaseEnd - PhaseStart5;
	ArcTerrainDiag::CompleteTickTimings(TickTimings);

	ArcTerrainDiag::NoteCompositeCandidates(NumCompositeCandidates);

	// Timed and reported apart from the phase total: the gauge walk is the instrument's own cost, and
	// folding it into the terrain's tick would inflate the very number a regression is sized against.
	const double DiagGatherStart = FPlatformTime::Seconds();
	ArcTerrainDiag::FGauges Gauges;
	TArray<FArcTerrainPipelineSnapshot> PipelineSnapshots;
	const bool bGatherFullDiagnostics = ArcTerrainDiag::IsCounting();
	const bool bGatherBakeProgress = ArcTerrainDiag::GBakeProgress != 0;
	if (bGatherFullDiagnostics || bGatherBakeProgress)
	{
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			FArcTerrainState& DiagState = TerrainPair.Value;
			if (bGatherFullDiagnostics)
			{
				const AArcTerrain* DiagTerrain = TerrainPair.Key.Get();
				if (DiagTerrain != nullptr)
				{
					AttributeCollisionSaturation(DiagState);
					++Gauges.Terrains;
					Gauges.Cells += DiagState.Cells.Num();
					Gauges.CollisionPatches += DiagState.CollisionPatches.Num();
					for (const TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair :
						DiagState.CollisionPatches)
					{
						Gauges.PatchesMissingHeightSource += PatchPair.Value.bCollisionHeightSourceMissing ? 1 : 0;
					}

					FArcTerrainTileStoreStats Stats;
					ArcTerrainTileAccess::GatherStoreStats(*DiagTerrain, false, Stats);
					for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
					{
						for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
						{
							Gauges.TilesLoaded[Rail][Tier] += Stats.Loaded[Rail][Tier];
							Gauges.TilesInFlight[Rail][Tier] += Stats.InFlight[Rail][Tier];
							Gauges.TilesUnusable[Rail][Tier] += Stats.Unusable[Rail][Tier];
							Gauges.ManifestEntries[Rail][Tier] += Stats.ManifestEntries[Rail][Tier];
						}
					}
					int32 MatchingPendingAtlasFills = 0;
					for (const FPendingAtlasFill& PendingFill : PendingAtlasFills)
					{
						if (PendingFill.TerrainId == DiagState.CollisionTerrainId)
						{
							++MatchingPendingAtlasFills;
						}
					}
					FArcTerrainPipelineSnapshot& Snapshot = PipelineSnapshots.AddDefaulted_GetRef();
					ArcTerrainPipelineDiagLocal_73d4::Gather(*DiagTerrain, DiagState, Stats, *Settings,
						MatchingPendingAtlasFills, Snapshot);
				}
			}
#if WITH_EDITOR
			if (bGatherFullDiagnostics)
			{
				for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
				{
					for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
					{
						const FArcTerrainTileRailState& RailState = DiagState.TileRails[Rail][Tier];
						Gauges.BakeInFlight[Rail] += RailState.Pending.Num();
						Gauges.BakeProbes[Rail][Tier] += RailState.ProbesInFlight;
						Gauges.BakeEvaluations[Rail][Tier] += RailState.EvaluationsInFlight;
						Gauges.BakeWrites[Rail][Tier] += RailState.WritesInFlight.Num();
						Gauges.BakeDeferred[Rail][Tier] += RailState.RetriesDeferred;
					}
				}
				for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
				{
					Gauges.BakeQueue[static_cast<int32>(EArcTerrainRail::Height)][Tier] +=
						DiagState.PendingHeightTiles[Tier].Num();
					Gauges.BakeQueue[static_cast<int32>(EArcTerrainRail::Weight)][Tier] +=
						DiagState.PendingWeightTiles[Tier].Num();
				}
				Gauges.BakeQueue[static_cast<int32>(EArcTerrainRail::Data)][0] +=
					DiagState.PendingDataTiles.Num();
			}
#endif
		}
#if WITH_EDITOR
		if (!bGatherFullDiagnostics && bGatherBakeProgress && bBakeProgressSessionActive
			&& bBakeProgressSnapshotValid)
		{
			for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
			{
				for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
				{
					Gauges.BakeProbes[Rail][Tier] = BakeProgressSnapshot.Probes[Rail][Tier];
					Gauges.BakeEvaluations[Rail][Tier] = BakeProgressSnapshot.Evaluations[Rail][Tier];
					Gauges.BakeWrites[Rail][Tier] = BakeProgressSnapshot.Writes[Rail][Tier];
					Gauges.BakeDeferred[Rail][Tier] = BakeProgressSnapshot.Deferred[Rail][Tier];
					Gauges.BakeQueue[Rail][Tier] = BakeProgressSnapshot.Queue[Rail][Tier];
					Gauges.BakeInFlight[Rail] += BakeProgressSnapshot.Probes[Rail][Tier]
						+ BakeProgressSnapshot.Evaluations[Rail][Tier]
						+ BakeProgressSnapshot.Writes[Rail][Tier];
				}
			}
		}
#endif
	}
	const double DiagGatherSeconds = FPlatformTime::Seconds() - DiagGatherStart;

	ArcTerrainDiag::SetTickStats(TickTimings, Gauges, DiagGatherSeconds);
	ArcTerrainDiag::EndTick(TickTimings, FApp::GetDeltaTime(), DiagGatherSeconds);

	// Outside the phase total for the same reason the gauge walk is: it charges itself to its own counter
	// so it can never be read as terrain work. Per-terrain gate: a terrain with every bDraw* flag false
	// never reaches ResolveResidency/GatherTileResidency at all.
	for (const TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		const AArcTerrain* DrawTerrain = TerrainPair.Key.Get();
		if (DrawTerrain != nullptr && DrawTerrain->HasDebugOverlayEnabled())
		{
			ArcTerrainDebugDraw::DrawTerrain(*DrawTerrain, TerrainPair.Value, InvokerPositions);
		}
	}

	ArcTerrainDiag::LogBakeProgress(Gauges);

	if (ArcTerrainDiag::ShouldEmit())
	{
		if (ArcTerrainDiag::IsOn())
		{
			ArcTerrainDiag::LogFrame(Gauges);
			for (const FArcTerrainPipelineSnapshot& Snapshot : PipelineSnapshots)
			{
				ArcTerrainDiag::LogTerrainPipeline(Snapshot);
			}
			for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
			{
				AArcTerrain* Terrain = TerrainPair.Key.Get();
				if (Terrain != nullptr)
				{
					TerrainPair.Value.PipelineTelemetry.ResetInterval();
					ArcTerrainTileAccess::ResetStoreIntervalStats(*Terrain);
				}
			}
		}
		// After the level-1 emit so a hitch window carries both, and outside its gate: a hitch reports
		// itself whatever the diagnostics level is.
		ArcTerrainDiag::LogHitch();
		ArcTerrainDiag::ResetFrame();
	}
}

TStatId UArcTerrainSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcTerrainSubsystem, STATGROUP_Tickables);
}

void UArcTerrainSubsystem::RegisterTerrain(AArcTerrain& Terrain)
{
	const UWorld* World = Terrain.GetWorld();
	UE_LOGF(LogArcTerrain, Log,"ArcTerrain: RegisterTerrain '%ls' on world '%ls'.",
		*Terrain.GetName(), World != nullptr ? *World->GetName() : TEXT("<null>"));

	FArcTerrainState& State = Terrains.FindOrAdd(&Terrain);
	if (!State.RenderOwner.IsValid())
	{
		State.RenderOwner.Key.WorldInstanceToken = WorldInstanceToken;
		State.RenderOwner.Key.TerrainInstanceId =
			ArcTerrainRenderOwnershipLocal_4f61::GNextTerrainInstanceId.fetch_add(1, std::memory_order_relaxed);
		State.RenderOwner.OwnerEpoch =
			ArcTerrainRenderOwnershipLocal_4f61::GNextOwnerEpoch.fetch_add(1, std::memory_order_relaxed);
		State.LastObservedRhiEpoch = GArcTerrainCBTRendererExtension.GetRhiEpoch();
	}
	if (State.CollisionTerrainId == 0)
	{
		State.CollisionTerrainId = NextCollisionTerrainId++;
		State.PipelineTelemetry.ConsumedCollisionCountSaturationSerial = CollisionCountSaturationSerial;
		State.PipelineTelemetry.ConsumedCollisionDeadlineSaturationSerial = CollisionDeadlineSaturationSerial;
	}
	++State.StreamLayoutRevision;
	++State.ResidencyConfigRevision;
	InvalidateStreamWindows(State);

	RefreshResolvedWeightQuads(Terrain, State);
	FlushPendingDerivedGridRebuild(Terrain, State);
	ArcTerrainResidencyIndex_c4e8::RebuildIndexes(Terrain, State);
	ArcTerrainCollisionIndex_e7b2::RebuildStampIndex(State);
	ArcTerrainCollisionIndex_e7b2::RebuildCollisionMeshIndex(Terrain, State);

	// After the state exists: the weight lattice resolves through this map.
	ArcTerrainDiag::LogTerrainGrid(Terrain, State);
}

void UArcTerrainSubsystem::NotifyLocalTileAuthority(const AArcTerrain& Terrain, const FArcTerrainTileAuthorityEvent& Event)
{
	if (FArcTerrainState* State = Terrains.Find(const_cast<AArcTerrain*>(&Terrain)))
	{
		PublishLocalTileAuthorityEvent(*State, Event);
	}
}

void UArcTerrainSubsystem::UnregisterTerrain(AArcTerrain& Terrain)
{
	if (FArcTerrainState* RegisteredState = Terrains.Find(&Terrain))
	{
		GArcTerrainCBTRendererExtension.RetireTerrain_GameThread(RegisteredState->RenderOwner);
		if (FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet())
		{
			Coordinator->AbandonTerrain(GetRuntimeOwnerId(), RegisteredState->CollisionTerrainId);
		}
		ReleasePendingAtlasFills(RegisteredState->CollisionTerrainId);
	}
	TerrainsWithDirtyCells.Remove(&Terrain);
#if WITH_EDITOR
	ArcTerrainDiag::CancelBakeProgress(Terrain.GetWorld());
#endif
	// Destroy previews before releasing tiles (MIDs reference the tile RTs).
	PreviewManager.ReleaseTerrainRender(&Terrain);

	ArcTerrainTileAccess::ReleaseTerrainTiles(Terrain);
	ArcTerrainDebugDraw::EvictCoverageCache(Terrain);
	if (FArcTerrainState* RegisteredState = Terrains.Find(&Terrain))
	{
		++RegisteredState->StreamLayoutRevision;
		InvalidateStreamWindows(*RegisteredState);
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
#if WITH_EDITOR
				FArcTerrainTileRailState& RailState = RegisteredState->TileRails[RailIndex][Tier];
				if (RailIndex == static_cast<int32>(EArcTerrainRail::Data) && Tier == 0)
				{
					NumDataInFlight = 0;
					NumDataPendingTiles = 0;
				}
				AbandonRailPendingTileBakes(RailState);
				InvalidateRailWrites(RailState);
#endif
			}
		}
	}

	FArcTerrainState State;
	if (Terrains.RemoveAndCopyValue(&Terrain, State))
	{
		for (TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair : State.CollisionPatches)
		{
			DestroyPatchCollision(PatchPair.Value);
		}
		for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& LightingPair : State.LightingPatches)
		{
			DestroyLightingPatch(LightingPair.Value);
		}
		for (TPair<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch>& FarPair : State.FarBlockPatches)
		{
			DestroyFarBlockPatch(FarPair.Value);
		}
		for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : State.Cells)
		{
			CellPair.Value.TileSet.Release(RenderTargetPool);
			ArcTerrainSubsystemPrivate::ResetCellAtlasPublication(CellPair.Value);
		}
	}

	if (Terrains.IsEmpty())
	{
		ClearStreamReleaseTimer();
	}
#if WITH_EDITOR
	if (Terrains.IsEmpty())
	{
		ClearBakeRetryTimer();
	}
	if (!HasPendingDataTileWork())
	{
		EndDataBakePass();
	}
#endif
}

void UArcTerrainSubsystem::OnTerrainMaterialChanged(AArcTerrain& Terrain)
{
	// Cosmetic-only: drop the cached resolution so the terrain's material instance is rebuilt against
	// the new base material. No recomposite, no tile churn — the render component survives.
	PreviewManager.RefreshTerrainMaterial(Terrain);

	if (FArcTerrainState* State = Terrains.Find(&Terrain))
	{
		++State->LightingConfigGeneration;
		ArcTerrainLightingIndex_6b19::OrphanInFlight(*State);
		for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& LightingPair : State->LightingPatches)
		{
			DestroyLightingPatch(LightingPair.Value);
		}
		ArcTerrainLightingIndex_6b19::SeedCurrentWindow(*State, Terrain);
	}
}

void UArcTerrainSubsystem::OnTerrainLightingChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}

	++State->LightingConfigGeneration;
	++State->FarBlockConfigGeneration;
	ArcTerrainLightingIndex_6b19::OrphanInFlight(*State);
	ArcTerrainFarBlockIndex_2c08::OrphanInFlight(*State);
	for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& LightingPair : State->LightingPatches)
	{
		DestroyLightingPatch(LightingPair.Value);
	}
	State->LightingPatches.Empty();
	ArcTerrainLightingIndex_6b19::ClearScheduling(*State);
	for (TPair<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch>& FarPair : State->FarBlockPatches)
	{
		DestroyFarBlockPatch(FarPair.Value);
	}
	State->FarBlockPatches.Empty();
	ArcTerrainFarBlockIndex_2c08::ClearScheduling(*State);
}

int32 UArcTerrainSubsystem::ResolveStampMaxWeightSourceDim(const UArcTerrainStampComponent& Stamp) const
{
	// Disabled / failed descriptor / no positive layer dim → 0 (must match the old scanner).
	if (Stamp.bIsDisabled)
	{
		return 0;
	}
	FArcStampSourceDesc Desc;
	if (!Stamp.GetSourceDescriptor(Desc))
	{
		return 0;
	}
	int32 MaxSourceDim = 0;
	for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
	{
		const FArcStampLayerDesc& Layer = LayerPair.Value;
		MaxSourceDim = FMath::Max3(MaxSourceDim, Layer.SourceDims.X, Layer.SourceDims.Y);
	}
	return MaxSourceDim;
}

void UArcTerrainSubsystem::AddWeightSourceDim(FArcTerrainState& State, int32 SourceDim)
{
	if (SourceDim <= 0)
	{
		return;
	}
	int32& Count = State.WeightSourceDimCounts.FindOrAdd(SourceDim);
	++Count;
	if (SourceDim > State.MaxWeightSourceDim)
	{
		State.MaxWeightSourceDim = SourceDim;
	}
}

void UArcTerrainSubsystem::RemoveWeightSourceDim(FArcTerrainState& State, int32 SourceDim)
{
	if (SourceDim <= 0)
	{
		return;
	}
	int32* Count = State.WeightSourceDimCounts.Find(SourceDim);
	if (Count == nullptr)
	{
		return;
	}
	--(*Count);
	if (*Count > 0)
	{
		return;
	}
	State.WeightSourceDimCounts.Remove(SourceDim);
	if (SourceDim != State.MaxWeightSourceDim)
	{
		return;
	}
	int32 NewMax = 0;
	for (const TPair<int32, int32>& Pair : State.WeightSourceDimCounts)
	{
		NewMax = FMath::Max(NewMax, Pair.Key);
	}
	State.MaxWeightSourceDim = NewMax;
}

void UArcTerrainSubsystem::RefreshStampWeightSourceDim(FArcTerrainState& State, FArcTerrainStampEntry& Entry)
{
	RemoveWeightSourceDim(State, Entry.CachedMaxWeightSourceDim);
	const UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
	Entry.CachedMaxWeightSourceDim = Stamp != nullptr ? ResolveStampMaxWeightSourceDim(*Stamp) : 0;
	AddWeightSourceDim(State, Entry.CachedMaxWeightSourceDim);

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		if (&TerrainPair.Value == &State)
		{
			if (AArcTerrain* Terrain = TerrainPair.Key.Get())
			{
				RefreshResolvedWeightQuads(*Terrain, State);
			}
			break;
		}
	}
}

void UArcTerrainSubsystem::RefreshResolvedWeightQuads(AArcTerrain& Terrain, FArcTerrainState& State)
{
	const FArcTerrainConfig& Config = Terrain.Config;
	const int32 HeightQuads = ArcTerrainSubsystemPrivate::CompositeQuads(Config);
	int32 WeightQuads = FMath::Clamp(
		Config.AuthoredCellWeightQuads(), HeightQuads, ArcTerrain::MaxWeightSliceSide - 3);
	if (State.MaxWeightSourceDim > 0)
	{
		WeightQuads = FMath::Max(HeightQuads, FMath::Min(WeightQuads, State.MaxWeightSourceDim));
	}
	if (WeightQuads == State.ResolvedWeightQuads)
	{
		return;
	}
	State.ResolvedWeightQuads = WeightQuads;
	++State.DerivedGridRevision;
	++State.StreamLayoutRevision;
	InvalidateStreamWindows(State);
	State.bDerivedGridRebuildPending = true;
}

void UArcTerrainSubsystem::FlushPendingDerivedGridRebuild(AArcTerrain& Terrain, FArcTerrainState& State)
{
	if (!State.bDerivedGridRebuildPending)
	{
		return;
	}
	State.bDerivedGridRebuildPending = false;
#if WITH_EDITOR
	State.PendingTileInvalidations.Reset();
	ArcTerrainTileSeedLocal_c9f2::RebuildAndSeedAllTileRails(Terrain, State);
#else
	(void)Terrain;
#endif
	++State.ResidencyConfigRevision;
	ArcTerrainResidencyIndex_c4e8::RebuildIndexes(Terrain, State);
	FArcTerrainTileAuthorityEvent LayoutEvent;
	LayoutEvent.Reason = EArcTerrainTileAuthorityReason::LayoutChanged;
	PublishLocalTileAuthorityEvent(State, LayoutEvent);
}

void UArcTerrainSubsystem::OnTerrainConfigChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}

	// Before the teardown: the cells re-seed from these stamps, so a stamp whose raster is derived from
	// the terrain's lattice has to re-derive it here or the re-seed reads the field built at the old one.
	TArray<TWeakObjectPtr<UArcTerrainStampComponent>> ConfigChangeStamps;
	ConfigChangeStamps.Reserve(State->Stamps.Num());
	for (const FArcTerrainStampEntry& Entry : State->Stamps)
	{
		ConfigChangeStamps.Add(Entry.Stamp);
	}

	for (const TWeakObjectPtr<UArcTerrainStampComponent>& WeakStamp : ConfigChangeStamps)
	{
		if (UArcTerrainStampComponent* Stamp = WeakStamp.Get())
		{
			Stamp->OnAffectedTerrainConfigChanged();
		}
	}

	// Spatial layout (CellSize / QuadsPerCell / ChannelSet) changed: every cached cell was built
	// against the OLD geometry. Tear it all down (same ordering as UnregisterTerrain — previews first,
	// since their MIDs reference the tile RTs) and empty the cell set. Nothing re-seeds it: cells come
	// back from the stamps and modifiers that shape them, at the new density and with bDirty set — which
	// is also why no tick-rate density comparison exists.
	PreviewManager.ReleaseTerrainRender(&Terrain);
	for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : State->Cells)
	{
		CellPair.Value.TileSet.Release(RenderTargetPool);
		ArcTerrainSubsystemPrivate::ResetCellAtlasPublication(CellPair.Value);
	}
	ArcTerrainDiag::NoteCellsRemoved(State->Cells.Num());
	State->EmptyCells();

	// The collision lattice is anchored on the terrain frame the spatial edit just moved, and every
	// cached heightfield was evaluated against the old one — a patch key no longer names the same
	// ground, so the registry goes with the cells.
	for (TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair : State->CollisionPatches)
	{
		DestroyPatchCollision(PatchPair.Value);
	}
	State->CollisionPatches.Empty();
	ArcTerrainCollisionIndex_e7b2::ClearScheduling(*State);
	ArcTerrainCollisionIndex_e7b2::RebuildCollisionMeshIndex(Terrain, *State);
	++State->LightingConfigGeneration;
	++State->FarBlockConfigGeneration;
	ArcTerrainLightingIndex_6b19::OrphanInFlight(*State);
	ArcTerrainFarBlockIndex_2c08::OrphanInFlight(*State);
	for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& LightingPair : State->LightingPatches)
	{
		DestroyLightingPatch(LightingPair.Value);
	}
	State->LightingPatches.Empty();
	ArcTerrainLightingIndex_6b19::ClearScheduling(*State);
	for (TPair<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch>& FarPair : State->FarBlockPatches)
	{
		DestroyFarBlockPatch(FarPair.Value);
	}
	State->FarBlockPatches.Empty();
	ArcTerrainFarBlockIndex_2c08::ClearScheduling(*State);
	// Slice indices are meaningless against a new spatial frame; the atlas re-admits from scratch.
	State->ResidentSectors.Reset();
	State->ResidentWeightGroups.Reset();
	State->TileAtlas.Reset();
	State->TierLadder = FArcTerrainTierLadder();
	State->WeightLadder = FArcTerrainWeightLadder();
	++State->StreamLayoutRevision;
	InvalidateStreamWindows(*State);

	RefreshResolvedWeightQuads(Terrain, *State);

#if WITH_EDITOR
	// A config edit dirties no bounds, so no tile's DirtySerial moves and the memo would hold every
	// written tile current — on a lattice where the key (0,0) no longer names the same area. In-flight
	// bakes were launched against the old lattice and their results would be recorded at a resolution
	// nothing reads.
	for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
	{
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			FArcTerrainTileRailState& RailState = State->TileRails[RailIndex][Tier];
			RailState.ResetDecisions();
			AbandonRailPendingTileBakes(RailState);
			InvalidateRailWrites(RailState);
			RailState.ResetPending();
			RailState.ResetWorkQueue();
			State->PendingTiles(static_cast<EArcTerrainRail>(RailIndex), Tier).Reset();
		}
	}

	ArcTerrainTileSeedLocal_c9f2::DropRemovedAuthoredTiers(Terrain, *State, EArcTerrainRail::Height);
	ArcTerrainTileSeedLocal_c9f2::DropRemovedAuthoredTiers(Terrain, *State, EArcTerrainRail::Weight);
	ArcTerrainTileSeedLocal_c9f2::RebuildAndSeedAllTileRails(Terrain, *State);
#endif
	State->bDerivedGridRebuildPending = false;
	++State->ResidencyConfigRevision;
	ArcTerrainResidencyIndex_c4e8::RebuildIndexes(Terrain, *State);
	FArcTerrainTileAuthorityEvent LayoutEvent;
	LayoutEvent.Reason = EArcTerrainTileAuthorityReason::LayoutChanged;
	PublishLocalTileAuthorityEvent(*State, LayoutEvent);
}

void UArcTerrainSubsystem::OnTerrainCollisionRasterChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}

	// Both raster knobs are folded into the built heightfield and into its DDC key, but nothing dirties a
	// patch when they move — so an already-built patch would keep the old raster forever. The whole
	// registry is dropped rather than each entry reset, because an EXTENT edit re-keys the lattice: patch
	// (0,0) at 64 m and at 128 m are different ground, and a surviving entry would carry one's state onto
	// the other. Phase 3 re-populates from the ring on the next tick.
	for (TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair : State->CollisionPatches)
	{
		DestroyPatchCollision(PatchPair.Value);
	}
	State->CollisionPatches.Empty();
	ArcTerrainCollisionIndex_e7b2::ClearScheduling(*State);
	ArcTerrainCollisionIndex_e7b2::RebuildCollisionMeshIndex(Terrain, *State);
}

void UArcTerrainSubsystem::OnTerrainRenderChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}
	++State->AtlasSchedule.RenderKnobRevision;
	PreviewManager.InvalidateBindingSignature(Terrain);
}

void UArcTerrainSubsystem::OnTerrainHeightOptionsChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}

	// BaseHeightCm is the value the height chain composites FROM, so every cell's composited tile is
	// stale: bDirty re-runs the composite.
	for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : State->Cells)
	{
		State->Hot(CellPair.Value).bDirty = 1;
		CellPair.Value.bCompositeResolveRetryPending = false;
		CellPair.Value.CompositeResolveRetryAfter = 0.0;
		CellPair.Value.CompositeResolveRetryCount = 0;
	}

	// BaseHeightCm is hashed into the collision recipe key, and a height-options edit dirties no bounds —
	// so without this every built patch would keep pre-edit ground under a key its memo never recomputes.
	for (TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair : State->CollisionPatches)
	{
		++PatchPair.Value.DirtySerial;
		if (State->CollisionWindowUnion.Contains(PatchPair.Key)
			|| State->CollisionBootstrapKeys.Contains(PatchPair.Key))
		{
			ArcTerrainCollisionIndex_e7b2::EnqueueReady(*State, Terrain, PatchPair.Key);
		}
	}

	++State->LightingConfigGeneration;
	++State->FarBlockConfigGeneration;
	ArcTerrainLightingIndex_6b19::OrphanInFlight(*State);
	ArcTerrainFarBlockIndex_2c08::OrphanInFlight(*State);
	ArcTerrainLightingIndex_6b19::RecreateFlatPackedHeightTexture(Terrain, *State);
	for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& LightingPair : State->LightingPatches)
	{
		DestroyLightingPatch(LightingPair.Value);
	}
	ArcTerrainLightingIndex_6b19::SeedCurrentWindow(*State, Terrain);
	for (TPair<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch>& FarPair : State->FarBlockPatches)
	{
		DestroyFarBlockPatch(FarPair.Value);
	}
	ArcTerrainFarBlockIndex_2c08::SeedCurrentWindow(*State, Terrain);

#if WITH_EDITOR
	// The tile rails memoise their decisions per tile and re-key only when something dirties the tile;
	// BaseHeightCm is terrain-wide and dirties no bounds, so without this every already-written tile
	// would keep the old heights under a key nothing recomputes.
	for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
	{
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			FArcTerrainTileRailState& RailState = State->TileRails[RailIndex][Tier];
			RailState.ResetDecisions();
			AbandonRailPendingTileBakes(RailState);
			InvalidateRailWrites(RailState);
			RailState.ResetPending();
			RailState.ResetWorkQueue();
			State->PendingTiles(static_cast<EArcTerrainRail>(RailIndex), Tier).Reset();
		}
	}

	// Re-keying alone changes nothing: the bake phase only visits tiles in the pending sets, and a
	// height-options edit dirties no bounds. ALL THREE RAILS, not height alone: BaseHeightCm is folded into
	// the recipe key every rail keys on, and the weight and data payloads are derived from the height and
	// slope the edit just moved.
	ArcTerrainTileSeedLocal_c9f2::RebuildAndSeedAllTileRails(Terrain, *State);
#endif
}

void UArcTerrainSubsystem::OnTerrainHeightTiersChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}

	// Every quantity a coarse tier is addressed by follows S(T): the sector lattice, the tile lattice and
	// the cache key. Drop the resolved ladder so nothing answers on the old shifts, and drop every coarse
	// registration — its key names different ground now. Tier 0 is left alone; S(0) is 0 whatever the
	// ladder says, and the budget split reaches it through the layout's own inequality check.
	State->TierLadder = FArcTerrainTierLadder();
	++State->StreamLayoutRevision;
	InvalidateStreamWindows(*State);
	State->ResidentSectors.RemoveAll([](const FArcTerrainResidentSector& Sector)
	{
		return Sector.Id.Tier != 0;
	});
	if (State->TileAtlas.IsValid())
	{
		TArray<FArcTerrainSectorId> Admitted;
		State->TileAtlas->GetAdmittedSectors(Admitted);
		for (const FArcTerrainSectorId& SectorId : Admitted)
		{
			if (SectorId.Tier != 0)
			{
				State->TileAtlas->EvictSector(SectorId);
			}
		}
	}

	RefreshResolvedWeightQuads(Terrain, *State);

#if WITH_EDITOR
	// A ladder edit dirties no bounds, so the per-tile memo would hold every written coarse tile current
	// under a key it never recomputes — on a lattice where that key no longer names the same area.
	for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
	{
		FArcTerrainTileRailState& RailState =
			State->TileRails[static_cast<int32>(EArcTerrainRail::Height)][Tier];
		RailState.ResetDecisions();
		AbandonRailPendingTileBakes(RailState);
		InvalidateRailWrites(RailState);
		RailState.ResetPending();
		State->PendingTiles(EArcTerrainRail::Height, Tier).Reset();
		RailState.ResetWorkQueue();
	}

	ArcTerrainTileSeedLocal_c9f2::DropRemovedAuthoredTiers(Terrain, *State, EArcTerrainRail::Height);
	ArcTerrainTileSeedLocal_c9f2::RebuildAndSeedAllTileRails(Terrain, *State);
#endif
	State->bDerivedGridRebuildPending = false;
	++State->ResidencyConfigRevision;
	ArcTerrainResidencyIndex_c4e8::RebuildIndexes(Terrain, *State);
	FArcTerrainTileAuthorityEvent LayoutEvent;
	LayoutEvent.Reason = EArcTerrainTileAuthorityReason::LayoutChanged;
	PublishLocalTileAuthorityEvent(*State, LayoutEvent);
}

void UArcTerrainSubsystem::OnTerrainWeightTiersChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}

	State->WeightLadder = FArcTerrainWeightLadder();
	++State->StreamLayoutRevision;
	InvalidateStreamWindows(*State);
	State->ResidentWeightGroups.RemoveAll([](const FArcTerrainResidentWeightGroup& Group)
	{
		return Group.Tier != 0;
	});
	if (State->TileAtlas.IsValid())
	{
		TArray<FArcTerrainWeightGroupId> AdmittedWeightGroups;
		State->TileAtlas->GetAdmittedWeightGroups(AdmittedWeightGroups);
		for (const FArcTerrainWeightGroupId& GroupId : AdmittedWeightGroups)
		{
			if (GroupId.Tier != 0)
			{
				State->TileAtlas->EvictWeightGroup(GroupId);
			}
		}
	}

	RefreshResolvedWeightQuads(Terrain, *State);

#if WITH_EDITOR
	for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
	{
		FArcTerrainTileRailState& RailState =
			State->TileRails[static_cast<int32>(EArcTerrainRail::Weight)][Tier];
		RailState.ResetDecisions();
		AbandonRailPendingTileBakes(RailState);
		InvalidateRailWrites(RailState);
		RailState.ResetPending();
		State->PendingTiles(EArcTerrainRail::Weight, Tier).Reset();
		RailState.ResetWorkQueue();
	}

	ArcTerrainTileSeedLocal_c9f2::DropRemovedAuthoredTiers(Terrain, *State, EArcTerrainRail::Weight);
	ArcTerrainTileSeedLocal_c9f2::RebuildAndSeedAllTileRails(Terrain, *State);
#endif
	State->bDerivedGridRebuildPending = false;
	++State->ResidencyConfigRevision;
	ArcTerrainResidencyIndex_c4e8::RebuildIndexes(Terrain, *State);
	FArcTerrainTileAuthorityEvent LayoutEvent;
	LayoutEvent.Reason = EArcTerrainTileAuthorityReason::LayoutChanged;
	PublishLocalTileAuthorityEvent(*State, LayoutEvent);
}

void UArcTerrainSubsystem::RegisterStamp(UArcTerrainStampComponent& Stamp, AArcTerrain& Terrain)
{
	FArcTerrainState& State = Terrains.FindOrAdd(&Terrain);

	const bool bAlreadyRegistered = State.Stamps.ContainsByPredicate(
		[&Stamp](const FArcTerrainStampEntry& Entry) { return Entry.Stamp.Get() == &Stamp; });
	if (bAlreadyRegistered)
	{
		return;
	}

	const UWorld* World = Terrain.GetWorld();
	UE_LOGF(LogArcTerrain, Log,"ArcTerrain: RegisterStamp '%ls' -> terrain '%ls' on world '%ls'.",
		*Stamp.GetName(), *Terrain.GetName(), World != nullptr ? *World->GetName() : TEXT("<null>"));

	const FBox Bounds = Stamp.ComputeBounds();
	FArcTerrainStampEntry& Entry = State.Stamps.AddDefaulted_GetRef();
	Entry.Stamp = &Stamp;
	Entry.Bounds = Bounds;
	if (Stamp.IsA<UArcTerrainMeshModifier>())
	{
		++State.MeshModifierRevision;
	}

	RefreshStampWeightSourceDim(State, Entry);
	ArcTerrainResidencyIndex_c4e8::AddStampCoverage(Terrain, State, Entry);
	ArcTerrainCollisionIndex_e7b2::NoteStampInserted(State, Stamp, State.Stamps.Num() - 1);
	ArcTerrainCollisionIndex_e7b2::UpdateStampCollisionCoverage(Terrain, State, Entry);

#if WITH_EDITOR
	// Incremental coverage must stop when ResolvedWeightQuads just changed.
	if (!State.bDerivedGridRebuildPending)
	{
		TArray<FArcTerrainBakeTileRef> NewCoverage;
		ArcTerrainSubsystemPrivate::GatherBakeTileRefsForBounds(Terrain, State, Bounds, NewCoverage);
		TArray<FArcTerrainBakeTileRef> Affected;
		ArcTerrainStampGraph_8b21::UpdateStampTileDependencies(
			State, Terrain, Entry, TConstArrayView<FArcTerrainBakeTileRef>(), NewCoverage, Affected);
		ArcTerrainTileSeedLocal_c9f2::AccumulateAffectedTiles(State, Affected);
	}
#endif

	MarkBoundsDirty(State, Terrain, Bounds);
}

void UArcTerrainSubsystem::UnregisterStamp(UArcTerrainStampComponent& Stamp, const FBox& LastBounds)
{
	// The stamp's terrain binding may already have changed — remove from every state it appears in
	// (bulk RemoveAll; never RemoveAt in a per-element loop) and dirty where the terrain resolves.
	const bool bMeshModifier = Stamp.IsA<UArcTerrainMeshModifier>();
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		FArcTerrainState& State = TerrainPair.Value;
		FArcTerrainStampEntry* Entry = State.Stamps.FindByPredicate(
			[&Stamp](const FArcTerrainStampEntry& Candidate) { return Candidate.Stamp.Get() == &Stamp; });
		if (Entry == nullptr)
		{
			continue;
		}

		AArcTerrain* StateTerrain = TerrainPair.Key.Get();
		ArcTerrainResidencyIndex_c4e8::RemoveStampCoverage(State, *Entry);
		ArcTerrainCollisionIndex_e7b2::RemoveStampCollisionCoverage(State, *Entry);
		RemoveWeightSourceDim(State, Entry->CachedMaxWeightSourceDim);
		Entry->CachedMaxWeightSourceDim = 0;
		if (StateTerrain != nullptr)
		{
			RefreshResolvedWeightQuads(*StateTerrain, State);
		}

#if WITH_EDITOR
		if (!State.bDerivedGridRebuildPending)
		{
			TArray<FArcTerrainBakeTileRef> OldCoverage = Entry->CoveredTiles;
			if (StateTerrain != nullptr)
			{
				TArray<FArcTerrainBakeTileRef> Affected;
				ArcTerrainStampGraph_8b21::UpdateStampTileDependencies(
					State, *StateTerrain, *Entry, OldCoverage,
					TConstArrayView<FArcTerrainBakeTileRef>(), Affected);
				ArcTerrainTileSeedLocal_c9f2::AccumulateAffectedTiles(State, Affected);
			}
			else
			{
				for (const FArcTerrainBakeTileRef& Ref : OldCoverage)
				{
					ArcTerrainStampGraph_8b21::RemoveStampDependency(State, Ref, Stamp);
				}
				Entry->CoveredTiles.Reset();
			}
		}
#endif

		State.Stamps.RemoveAll(
			[&Stamp](const FArcTerrainStampEntry& Candidate) { return Candidate.Stamp.Get() == &Stamp; });
		ArcTerrainCollisionIndex_e7b2::RebuildStampIndex(State);
		if (bMeshModifier)
		{
			++State.MeshModifierRevision;
		}
		if (StateTerrain != nullptr)
		{
			MarkBoundsDirty(State, *StateTerrain, LastBounds);
			if (State.bDerivedGridRebuildPending)
			{
				FlushPendingDerivedGridRebuild(*StateTerrain, State);
			}
		}
	}
}

void UArcTerrainSubsystem::OnStampChanged(UArcTerrainStampComponent& Stamp, const FBox& OldLocalBounds,
	const FBox& NewLocalBounds)
{
	AArcTerrain* Terrain = Stamp.GetResolvedTerrain();
	if (Terrain == nullptr)
	{
		return;
	}
	FArcTerrainState* State = Terrains.Find(Terrain);
	if (State == nullptr)
	{
		return;
	}
	FArcTerrainStampEntry* Entry = nullptr;
	if (const int32* StampIndex = State->StampByObject.Find(TObjectKey<UArcTerrainStampComponent>(&Stamp)))
	{
		Entry = &State->Stamps[*StampIndex];
	}
	if (Entry != nullptr)
	{
		ArcTerrainResidencyIndex_c4e8::RemoveStampCoverage(*State, *Entry);
		ArcTerrainCollisionIndex_e7b2::RemoveStampCollisionCoverage(*State, *Entry);
		RefreshStampWeightSourceDim(*State, *Entry);
	}
#if WITH_EDITOR
	if (Entry != nullptr && !State->bDerivedGridRebuildPending)
	{
		TArray<FArcTerrainBakeTileRef> OldCoverage = Entry->CoveredTiles;
		TArray<FArcTerrainBakeTileRef> NewCoverage;
		ArcTerrainSubsystemPrivate::GatherBakeTileRefsForBounds(*Terrain, *State, NewLocalBounds, NewCoverage);
		TArray<FArcTerrainBakeTileRef> Affected;
		ArcTerrainStampGraph_8b21::UpdateStampTileDependencies(
			*State, *Terrain, *Entry, OldCoverage, NewCoverage, Affected);
		ArcTerrainTileSeedLocal_c9f2::AccumulateAffectedTiles(*State, Affected);
	}
#endif
	if (Entry != nullptr)
	{
		Entry->Bounds = NewLocalBounds;
		ArcTerrainResidencyIndex_c4e8::AddStampCoverage(*Terrain, *State, *Entry);
		ArcTerrainCollisionIndex_e7b2::UpdateStampCollisionCoverage(*Terrain, *State, *Entry);
	}
	if (Stamp.IsA<UArcTerrainMeshModifier>())
	{
		++State->MeshModifierRevision;
	}

	const FBox DirtyBounds = OldLocalBounds.Equals(NewLocalBounds, 0.0)
		? NewLocalBounds
		: (OldLocalBounds + NewLocalBounds);
	MarkBoundsDirty(*State, *Terrain, DirtyBounds);
	if (State->bDerivedGridRebuildPending)
	{
		FlushPendingDerivedGridRebuild(*Terrain, *State);
	}

	// Edit-pin the Old ∪ New cells: a stamp footprint larger than the preview ring must still show
	// its whole edit result — pinned cells count as wanted-resident regardless of invoker distance
	// for a rolling EditPinSeconds window (each further edit extends it).
	// Editor-only UX feature: at runtime a moving stamp must NOT pin its footprint (tiles/previews
	// nobody can see, exempt from the cap) — the invoker ring already covers everything visible.
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		return;
	}
	const double EditPinSeconds = static_cast<double>(Terrain->EditPinSeconds);
	if (EditPinSeconds > 0.0)
	{
		const double PinnedUntil = FPlatformTime::Seconds() + EditPinSeconds;
		PinCellsInBounds(*State, Terrain->Config, OldLocalBounds, PinnedUntil);
		PinCellsInBounds(*State, Terrain->Config, NewLocalBounds, PinnedUntil);
	}
}

int32 UArcTerrainSubsystem::GetResolvedWeightQuads(const AArcTerrain& Terrain) const
{
	const FArcTerrainState* State = Terrains.Find(&Terrain);
	return State != nullptr ? State->ResolvedWeightQuads : 0;
}

FArcTerrainTierLadder UArcTerrainSubsystem::GetResolvedTierLadder(const AArcTerrain& Terrain) const
{
	check(IsInGameThread());

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	return State != nullptr ? State->TierLadder : FArcTerrainTierLadder();
}

FArcTerrainWeightLadder UArcTerrainSubsystem::GetResolvedWeightTierLadder(const AArcTerrain& Terrain) const
{
	check(IsInGameThread());

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	return State != nullptr ? State->WeightLadder : FArcTerrainWeightLadder();
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainDataMapRegionLocal_9f42
{
	// The terrain is an infinite plane, so the region form's cell walk is bounded by fiat rather than
	// by geometry: a caller handing in a box far larger than the residency ring gets a loud false, not
	// a rectangle limited only by int32.
	constexpr int32 MaxRegionCellsPerAxis = 64;

	// Grid ceiling: 2048 texels per axis, 2048^2 floats = 16 MiB total.
	constexpr int32 MaxRegionTexelsPerAxis = 2048;
	constexpr int64 MaxRegionTexels = 4194304;

	// Terrain-local AABB -> its max-EXCLUSIVE cell rectangle. False when a coordinate is non-finite,
	// outside representable cell indices, or the rectangle exceeds MaxRegionCellsPerAxis on an axis.
	bool ComputeRegionCellRect(const FBox& LocalBounds, double CellSize, FIntRect& OutRect)
	{
		const double MinXCell = FMath::Floor(LocalBounds.Min.X / CellSize);
		const double MinYCell = FMath::Floor(LocalBounds.Min.Y / CellSize);
		const double MaxXCell = FMath::Floor(LocalBounds.Max.X / CellSize);
		const double MaxYCell = FMath::Floor(LocalBounds.Max.Y / CellSize);

		constexpr double CellIndexLimit = 1.0e9;
		if (!FMath::IsFinite(MinXCell) || !FMath::IsFinite(MinYCell)
			|| !FMath::IsFinite(MaxXCell) || !FMath::IsFinite(MaxYCell)
			|| FMath::Abs(MinXCell) > CellIndexLimit || FMath::Abs(MinYCell) > CellIndexLimit
			|| FMath::Abs(MaxXCell) > CellIndexLimit || FMath::Abs(MaxYCell) > CellIndexLimit)
		{
			return false;
		}

		OutRect = FIntRect(static_cast<int32>(MinXCell), static_cast<int32>(MinYCell),
			static_cast<int32>(MaxXCell) + 1, static_cast<int32>(MaxYCell) + 1);
		return OutRect.Width() > 0 && OutRect.Height() > 0
			&& OutRect.Width() <= MaxRegionCellsPerAxis && OutRect.Height() <= MaxRegionCellsPerAxis;
	}
}

bool UArcTerrainSubsystem::SampleDataMap(AArcTerrain& Terrain, EArcTerrainDataMap Map,
	const FVector& WorldPosition, float& OutValue) const
{
	check(IsInGameThread());

	OutValue = 0.0f;

	const FArcTerrainConfig& Config = Terrain.Config;
	if (!FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0 || WorldPosition.ContainsNaN())
	{
		return false;
	}

	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Data, 0, Grid))
	{
		return false;
	}

	const FVector Local = Terrain.WorldToTerrainLocal(WorldPosition);
	// Guarded BEFORE the tile division: TileKeyFromLocal floors into an int32, and FMath::Clamp of a NaN
	// returns its upper bound, so a non-finite position must never reach either.
	constexpr double CellIndexLimit = 1.0e9;
	if (Local.ContainsNaN()
		|| FMath::Abs(Local.X) > CellIndexLimit * Config.CellSize()
		|| FMath::Abs(Local.Y) > CellIndexLimit * Config.CellSize())
	{
		return false;
	}

	const FVector2D LocalXY(Local.X, Local.Y);
	FIntPoint TileKey = FIntPoint::ZeroValue;
	if (!ArcTerrainTileAccess::TileKeyFromLocal(Grid, LocalXY, TileKey))
	{
		return false;
	}

	const FArcTerrainTilePayloadPtr Payload =
		ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Data, 0, TileKey);
	if (!Payload.IsValid())
	{
		return false;
	}

	return ArcTerrainDataMaps::SampleTileMask(*Payload, Grid, Map, LocalXY, OutValue);
}

bool UArcTerrainSubsystem::SampleDataMapRegion(AArcTerrain& Terrain, EArcTerrainDataMap Map,
	const FBox& WorldBounds, TArray<float>& OutValues, TBitArray<>& OutAvailable, FIntPoint& OutDims) const
{
	using namespace ArcTerrainDataMapRegionLocal_9f42;

	check(IsInGameThread());

	OutValues.Reset();
	OutAvailable.Reset();
	OutDims = FIntPoint::ZeroValue;

	const FArcTerrainConfig& Config = Terrain.Config;
	if (!WorldBounds.IsValid || !FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
	{
		return false;
	}

	// All eight corners, so a rotated terrain frame widens the local AABB instead of shearing it.
	const FBox LocalBounds = WorldBounds.TransformBy(Terrain.GetTerrainTransform().Inverse());
	if (LocalBounds.Min.ContainsNaN() || LocalBounds.Max.ContainsNaN())
	{
		return false;
	}

	FIntRect CellRect;
	if (!ComputeRegionCellRect(LocalBounds, Config.CellSize(), CellRect))
	{
		UE_LOGF(LogArcTerrain, Warning,"[DataMap] Region sample of terrain '%ls' spans an unusable cell rectangle (local %ls..%ls, "
				 "cell size %.0f) — at most %d cells per axis are read.",
			*Terrain.GetName(), *LocalBounds.Min.ToString(), *LocalBounds.Max.ToString(),
			Config.CellSize(), MaxRegionCellsPerAxis);
		return false;
	}

	// The data rail is one lattice for the whole terrain, so the grid's raster is that lattice's own
	// spacing — there is no per-cell quad count left to take a maximum over.
	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Data, 0, Grid))
	{
		return false;
	}

	const double Spacing =
		static_cast<double>(Grid.TileExtentM) * 100.0 / static_cast<double>(Grid.TileQuads);
	const double ExtentX = FMath::Max(LocalBounds.Max.X - LocalBounds.Min.X, 0.0);
	const double ExtentY = FMath::Max(LocalBounds.Max.Y - LocalBounds.Min.Y, 0.0);

	// Wanted dims computed in int64 BEFORE the per-axis clamp, so the warning below can name what the
	// finest resident raster actually asked for rather than the already-clamped count.
	const int64 WantedDimsX = ExtentX > 0.0 ? FMath::RoundToInt64(ExtentX / Spacing) + 1 : 1;
	const int64 WantedDimsY = ExtentY > 0.0 ? FMath::RoundToInt64(ExtentY / Spacing) + 1 : 1;

	int32 DimsX = ExtentX > 0.0
		? static_cast<int32>(FMath::Clamp<int64>(WantedDimsX, 2, MaxRegionTexelsPerAxis)) : 1;
	int32 DimsY = ExtentY > 0.0
		? static_cast<int32>(FMath::Clamp<int64>(WantedDimsY, 2, MaxRegionTexelsPerAxis)) : 1;
	bool bGridCoarsened = DimsX != WantedDimsX || DimsY != WantedDimsY;

	const int64 TexelsAfterAxisClamp = static_cast<int64>(DimsX) * static_cast<int64>(DimsY);
	if (TexelsAfterAxisClamp > MaxRegionTexels)
	{
		const double Scale = FMath::Sqrt(static_cast<double>(MaxRegionTexels)
			/ static_cast<double>(TexelsAfterAxisClamp));
		DimsX = FMath::Max(FMath::FloorToInt32(static_cast<double>(DimsX) * Scale), ExtentX > 0.0 ? 2 : 1);
		DimsY = FMath::Max(FMath::FloorToInt32(static_cast<double>(DimsY) * Scale), ExtentY > 0.0 ? 2 : 1);
		bGridCoarsened = true;
	}

	// GRID CONTRACT (see the declaration): texel (0,0) sits at the local AABB's min corner and the last
	// texel at its max corner, so the caller re-derives the spacing from its own bounds and OutDims.
	const double StepX = DimsX > 1 ? ExtentX / static_cast<double>(DimsX - 1) : 0.0;
	const double StepY = DimsY > 1 ? ExtentY / static_cast<double>(DimsY - 1) : 0.0;

	if (bGridCoarsened)
	{
		UE_LOGF(LogArcTerrain, Warning,"[DataMap] Region sample of terrain '%ls' at the data tile raster wanted %.1f "
				 "cm/texel; capped to %.1fx%.1f cm/texel (%dx%d texels).",
			*Terrain.GetName(), Spacing, StepX, StepY, DimsX, DimsY);
	}

	OutValues.SetNumUninitialized(DimsX * DimsY);
	OutAvailable.Init(false, DimsX * DimsY);

	FIntPoint CachedTileKey = FIntPoint::ZeroValue;
	FArcTerrainTilePayloadPtr CachedPayload;
	bool bCacheValid = false;
	int32 NumAvailable = 0;

	for (int32 Row = 0; Row < DimsY; ++Row)
	{
		const double LocalY = LocalBounds.Min.Y + static_cast<double>(Row) * StepY;
		for (int32 Column = 0; Column < DimsX; ++Column)
		{
			const double LocalX = LocalBounds.Min.X + static_cast<double>(Column) * StepX;
			const int32 OutIndex = Row * DimsX + Column;
			float& Out = OutValues[OutIndex];
			Out = 0.0f;

			const FVector2D LocalXY(LocalX, LocalY);
			FIntPoint TileKey = FIntPoint::ZeroValue;
			if (!ArcTerrainTileAccess::TileKeyFromLocal(Grid, LocalXY, TileKey))
			{
				continue;
			}
			if (!bCacheValid || TileKey != CachedTileKey)
			{
				CachedTileKey = TileKey;
				CachedPayload =
					ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Data, 0, TileKey);
				bCacheValid = true;
			}
			if (!CachedPayload.IsValid())
			{
				continue;
			}

			float Value = 0.0f;
			if (ArcTerrainDataMaps::SampleTileMask(*CachedPayload, Grid, Map, LocalXY, Value))
			{
				Out = Value;
				OutAvailable[OutIndex] = true;
				++NumAvailable;
			}
		}
	}

	if (NumAvailable == 0)
	{
		OutValues.Reset();
		OutAvailable.Reset();
		OutDims = FIntPoint::ZeroValue;
		return false;
	}

	OutDims = FIntPoint(DimsX, DimsY);
	return true;
}

bool UArcTerrainSubsystem::SampleTerrainHeightTexels(const AArcTerrain& Terrain, FIntPoint MinTexel,
	FIntPoint MaxTexel, TArray<float>& OutHeights, FIntPoint& OutDims, bool& bOutComplete)
{
	using namespace ArcTerrainSubsystemPrivate;

	check(IsInGameThread());

	OutHeights.Reset();
	OutDims = FIntPoint::ZeroValue;
	bOutComplete = false;

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return false;
	}

	const FArcTerrainConfig& Config = Terrain.Config;
	const double TexelToLocal = static_cast<double>(Config.TexelSizeCm());
	if (TexelToLocal <= 0.0)
	{
		return false;
	}

	if (MaxTexel.X < MinTexel.X || MaxTexel.Y < MinTexel.Y)
	{
		return false;
	}

	constexpr int64 MaxSamplesPerSide = 4096;
	const int64 SpanX = static_cast<int64>(MaxTexel.X) - static_cast<int64>(MinTexel.X) + 1;
	const int64 SpanY = static_cast<int64>(MaxTexel.Y) - static_cast<int64>(MinTexel.Y) + 1;
	const int64 Side = FMath::Max(SpanX, SpanY);
	if (Side < 2)
	{
		return false;
	}
	if (Side > MaxSamplesPerSide)
	{
		UE_LOGF(LogArcTerrain, Warning,"[Height] Texel sample of terrain '%ls' spans %lld x %lld texels — at most %lld samples "
				 "per side are refused; nothing evaluated.",
			*Terrain.GetName(), SpanX, SpanY, MaxSamplesPerSide);
		return false;
	}

	ArcTerrainCpuComposite::FCellRaster Raster;
	Raster.TexelOrigin = MinTexel;
	Raster.TexelToLocal = static_cast<float>(TexelToLocal);
	Raster.SamplesPerSide = static_cast<int32>(Side);

	const FBox SampleBounds = RasterGatherBounds(Raster.TexelOrigin, Raster.SamplesPerSide, TexelToLocal);

	TArray<UArcTerrainStampComponent*> SortedStamps;
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN height GatherSortedStamps t=%.6f", FPlatformTime::Seconds());
	GatherSortedStamps(*State, SampleBounds, SortedStamps);
	UE_LOGF(LogTemp, Display,"[RiverGen] END height GatherSortedStamps t=%.6f", FPlatformTime::Seconds());

	ArcTerrainCpuComposite::FCellHeightEval Eval;
	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("height texel gather ops"), MinTexel);
		UE_LOGF(LogTemp, Display,"[RiverGen] HEIGHT raster origin=%ls side=%d stamps=%d", *MinTexel.ToString(), Raster.SamplesPerSide, SortedStamps.Num());
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN height GatherCellHeightOps t=%.6f", FPlatformTime::Seconds());
		ArcTerrainCpuComposite::GatherCellHeightOps(SourceCache, Config, Terrain.GetTerrainTransform(), Raster,
			Terrain.HeightOptions.BaseHeightCm, SortedStamps, Eval);
		UE_LOGF(LogTemp, Display,"[RiverGen] END height GatherCellHeightOps t=%.6f", FPlatformTime::Seconds());
	}

	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("height texel evaluate"), MinTexel);
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN height EvaluateCellHeights t=%.6f", FPlatformTime::Seconds());
		ArcTerrainCpuComposite::EvaluateCellHeights(Eval, OutHeights);
		UE_LOGF(LogTemp, Display,"[RiverGen] END height EvaluateCellHeights t=%.6f", FPlatformTime::Seconds());
	}
	if (OutHeights.Num() != Raster.SamplesPerSide * Raster.SamplesPerSide)
	{
		OutHeights.Reset();
		return false;
	}

	OutDims = FIntPoint(Raster.SamplesPerSide, Raster.SamplesPerSide);
	bOutComplete = Eval.bComplete;
	return true;
}

bool UArcTerrainSubsystem::SampleTerrainHeightTexelsBeforeStamp(const AArcTerrain& Terrain,
	const UArcTerrainStampComponent& BeforeStamp, FIntPoint MinTexel, FIntPoint MaxTexel,
	TArray<float>& OutHeights, FIntPoint& OutDims, bool& bOutComplete)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN SampleTerrainHeightTexelsBeforeStamp t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END SampleTerrainHeightTexelsBeforeStamp elapsed=%.3fs", FPlatformTime::Seconds() - RiverLogStart);
	};
	using namespace ArcTerrainSubsystemPrivate;

	check(IsInGameThread());

	OutHeights.Reset();
	OutDims = FIntPoint::ZeroValue;
	bOutComplete = false;

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return false;
	}

	const FArcTerrainConfig& Config = Terrain.Config;
	const double TexelToLocal = static_cast<double>(Config.TexelSizeCm());
	if (TexelToLocal <= 0.0)
	{
		return false;
	}

	if (MaxTexel.X < MinTexel.X || MaxTexel.Y < MinTexel.Y)
	{
		return false;
	}

	constexpr int64 MaxSamplesPerSide = 4096;
	const int64 SpanX = static_cast<int64>(MaxTexel.X) - static_cast<int64>(MinTexel.X) + 1;
	const int64 SpanY = static_cast<int64>(MaxTexel.Y) - static_cast<int64>(MinTexel.Y) + 1;
	const int64 Side = FMath::Max(SpanX, SpanY);
	if (Side < 2)
	{
		return false;
	}
	if (Side > MaxSamplesPerSide)
	{
		UE_LOGF(LogArcTerrain, Warning,"[Height] Texel sample of terrain '%ls' spans %lld x %lld texels - at most %lld samples "
				 "per side are refused; nothing evaluated.",
			*Terrain.GetName(), SpanX, SpanY, MaxSamplesPerSide);
		return false;
	}

	ArcTerrainCpuComposite::FCellRaster Raster;
	Raster.TexelOrigin = MinTexel;
	Raster.TexelToLocal = static_cast<float>(TexelToLocal);
	Raster.SamplesPerSide = static_cast<int32>(Side);

	const FBox SampleBounds = RasterGatherBounds(Raster.TexelOrigin, Raster.SamplesPerSide, TexelToLocal);

	TArray<UArcTerrainStampComponent*> SortedStamps;
	GatherSortedStamps(*State, SampleBounds, SortedStamps);
	// Compare against the boundary because a disabled or nonintersecting stamp is absent from the gather.
	SortedStamps.RemoveAll([&BeforeStamp](const UArcTerrainStampComponent* Stamp)
	{
		return !IsStampOrderedBefore(*Stamp, BeforeStamp);
	});

	ArcTerrainCpuComposite::FCellHeightEval Eval;
	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("height texel gather ops"), MinTexel);
		ArcTerrainCpuComposite::GatherCellHeightOps(SourceCache, Config, Terrain.GetTerrainTransform(), Raster,
			Terrain.HeightOptions.BaseHeightCm, SortedStamps, Eval);
	}

	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("height texel evaluate"), MinTexel);
		ArcTerrainCpuComposite::EvaluateCellHeights(Eval, OutHeights);
	}
	if (OutHeights.Num() != Raster.SamplesPerSide * Raster.SamplesPerSide)
	{
		OutHeights.Reset();
		return false;
	}

	OutDims = FIntPoint(Raster.SamplesPerSide, Raster.SamplesPerSide);
	bOutComplete = Eval.bComplete;
	return true;
}

#if WITH_EDITOR
bool UArcTerrainSubsystem::ComputeStampFootprintCellRegion(
	const AArcTerrain& Terrain, FIntRect& OutRegion) const
{
	OutRegion = FIntRect(FIntPoint::ZeroValue, FIntPoint::ZeroValue);

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return false;
	}

	const double CellSize = Terrain.Config.CellSize();
	if (CellSize <= 0.0)
	{
		return false;
	}

	// Union the terrain-local footprints of every enabled, resolvable stamp — the SAME enabled/valid
	// filter GatherSortedStamps applies (a disabled stamp does not composite, so it is not content to
	// bake). FBox starts invalid and becomes valid on the first accumulated footprint.
	FBox UnionBounds(ForceInit);
	for (const FArcTerrainStampEntry& Entry : State->Stamps)
	{
		const UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
		if (Stamp == nullptr || Stamp->bIsDisabled)
		{
			continue;
		}
		const FBox StampBounds = Stamp->ComputeBounds();
		if (!StampBounds.IsValid)
		{
			continue;
		}
		UnionBounds += StampBounds;
	}

	if (!UnionBounds.IsValid)
	{
		return false;
	}

	// Cover EXACTLY the cells MarkBoundsDirty creates for the union AABB. A plain floor/ceil snap
	// under-covers: ArcTerrain::CellsIntersectingLocalBounds is edge-inclusive with a boundary
	// decrement (ArcTerrainTypes.cpp) — a stamp edge exactly on k*CellSize yields BOTH neighbour
	// cells, whereas floor/ceil excludes the lower neighbour, leaving a cell that renders live at runtime
	// with no baked section (the cooked game would show the flat fallback there). Reuse the single
	// geometric authority so bake ⊇ runtime cells at every boundary, with no floor/ceil edge reasoning.
	TArray<FArcTerrainCellKey> UnionCells;
	ArcTerrain::CellsIntersectingLocalBounds(UnionBounds, Terrain.Config, UnionCells);
	if (UnionCells.Num() == 0)
	{
		return false;
	}

	// Bounding FIntRect of the intersected keys, Max made max-EXCLUSIVE (+1) so a `< Region.Max` walk
	// covers every one. Edge-inclusive intersection yields a contiguous rect, so this
	// is always ≥1 cell.
	FIntPoint MinCell = UnionCells[0];
	FIntPoint MaxCell = UnionCells[0];
	for (const FArcTerrainCellKey& Key : UnionCells)
	{
		MinCell.X = FMath::Min(MinCell.X, Key.X);
		MinCell.Y = FMath::Min(MinCell.Y, Key.Y);
		MaxCell.X = FMath::Max(MaxCell.X, Key.X);
		MaxCell.Y = FMath::Max(MaxCell.Y, Key.Y);
	}
	OutRegion = FIntRect(MinCell, FIntPoint(MaxCell.X + 1, MaxCell.Y + 1));
	return true;
}
#endif

namespace ArcTerrainDiagCommand_3b6c
{
	FString YesNo(bool bValue)
	{
		return bValue ? TEXT("yes") : TEXT("no");
	}

	FString ChannelName(const UArcTerrainChannelSet& ChannelSet, int32 ChannelIndex)
	{
		return ChannelSet.Channels.IsValidIndex(ChannelIndex)
			? ChannelSet.Channels[ChannelIndex].ToString()
			: FString::Printf(TEXT("<unnamed %d>"), ChannelIndex);
	}

	/** Weight texel of the cell's own composite raster covering a terrain-local position: the CPU tile
	 *  index (apron included), which is also the slice texel once WeightDestOrigin is added. False when
	 *  the position falls outside the cell the caller resolved. */
	bool ResolveWeightTexel(const FVector& Local, FArcTerrainCellKey CellKey, int32 WeightQuads,
		double CellSize, FIntPoint& OutTexel, FIntPoint& OutGlobalTexel)
	{
		if (WeightQuads <= 0 || CellSize <= 0.0)
		{
			return false;
		}
		const double TexelToLocal = CellSize / static_cast<double>(WeightQuads);
		const int64 GlobalX = FMath::FloorToInt64(Local.X / TexelToLocal);
		const int64 GlobalY = FMath::FloorToInt64(Local.Y / TexelToLocal);
		OutGlobalTexel = FIntPoint(static_cast<int32>(GlobalX), static_cast<int32>(GlobalY));
		// Tile texel 0 is the apron texel at global CellKey * WeightQuads - 1, so the inner texel of this
		// position sits one further in — the same index the material's slice-UV remap lands on.
		const int64 Col = GlobalX - (static_cast<int64>(CellKey.X) * WeightQuads - 1);
		const int64 Row = GlobalY - (static_cast<int64>(CellKey.Y) * WeightQuads - 1);
		if (Col < 0 || Row < 0 || Col > WeightQuads + 2 || Row > WeightQuads + 2)
		{
			return false;
		}
		OutTexel = FIntPoint(static_cast<int32>(Col), static_cast<int32>(Row));
		return true;
	}

	struct FWeightTierHit
	{
		int32 Tier = INDEX_NONE;
		FIntPoint GroupKey = FIntPoint::ZeroValue;
		int32 Slice = INDEX_NONE;
	};

	int32 WeightTierSliceShift(const FArcTerrainTileAtlasLayout& Layout,
		const FArcTerrainWeightLadder& Ladder, int32 Tier)
	{
		return Ladder.Tiers.IsValidIndex(Tier) ? Ladder.Tiers[Tier].SliceShift : Layout.WeightSliceShift;
	}

	FWeightTierHit ResolveWeightTierHit(const FArcTerrainTileAtlas& Atlas,
		const FArcTerrainWeightLadder& Ladder, FArcTerrainCellKey CellKey)
	{
		const FArcTerrainTileAtlasLayout& Layout = Atlas.GetLayout();
		FWeightTierHit Hit;
		Hit.GroupKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, Layout.WeightSliceShift);

		const int32 NumTiers = FMath::Max(Ladder.Tiers.Num(), 1);
		for (int32 Tier = 0; Tier < NumTiers; ++Tier)
		{
			const int32 SliceShift = WeightTierSliceShift(Layout, Ladder, Tier);
			const FIntPoint GroupKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, SliceShift);
			const int32 Slice = Atlas.FindWeightSlice(FArcTerrainWeightGroupId{ Tier, GroupKey });
			if (Slice >= 0)
			{
				Hit.Tier = Tier;
				Hit.GroupKey = GroupKey;
				Hit.Slice = Slice;
				break;
			}
		}
		return Hit;
	}

	int32 WeightTierCellQuads(const FArcTerrainTileAtlasLayout& Layout,
		const FArcTerrainWeightLadder& Ladder, int32 Tier)
	{
		return Ladder.Tiers.IsValidIndex(Tier) ? Ladder.Tiers[Tier].CellWeightQuads : Layout.WeightQuads;
	}

	struct FWeightRingHit
	{
		int32 Tier = INDEX_NONE;
		FIntPoint GroupKey = FIntPoint::ZeroValue;
		int32 CumulativeShift = 0;
		int32 SliceShift = 0;
		int32 CellWeightQuads = 0;
		uint32 Slice = 0;
		int32 Radius = -1;
		FIntPoint RingCenter = FIntPoint::ZeroValue;
		int32 RingSliceCount = 0;
		int32 RingIndex = INDEX_NONE;
	};

	void BuildWeightRings(const FArcTerrainState& State, const FArcTerrainTileAtlas& Atlas,
		FArcTerrainResidencyRings& OutRings)
	{
		// The rings the vertex factory received are a per-frame local in TickAtlasResidency, so they are
		// rebuilt here from the same atlas and the same stored ladder.
		Atlas.BuildResidencyRings(State.TierLadder, State.WeightLadder, OutRings);
	}

	/** The tier the SHADER lands on: ArcTerrainVertexFactory.ush walks the RING WINDOW per tier, not atlas
	 *  admission, so a group admitted outside its tier's window falls through to a coarser tier or the fill
	 *  slice. FArcTerrainSceneProxy forces a slice-less ring's radius to -1 before arc_ResidencyRingIndex's
	 *  own range test, so the mirror forces it too. */
	FWeightRingHit ResolveWeightRingHit(const FArcTerrainTileAtlasLayout& Layout,
		const FArcTerrainWeightLadder& Ladder, const FArcTerrainResidencyRings& Rings,
		FArcTerrainCellKey CellKey)
	{
		const FArcTerrainResidencyRing EmptyRing;
		const FArcTerrainResidencyRing& BaseRing =
			Rings.Weight.IsValidIndex(0) ? Rings.Weight[0] : EmptyRing;

		FWeightRingHit Hit;
		Hit.SliceShift = WeightTierSliceShift(Layout, Ladder, 0);
		Hit.CellWeightQuads = WeightTierCellQuads(Layout, Ladder, 0);
		Hit.GroupKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, Hit.SliceShift);
		Hit.Slice = BaseRing.FillSlice;
		Hit.Radius = BaseRing.Slices.IsEmpty() ? -1 : BaseRing.Radius;
		Hit.RingCenter = BaseRing.Center;
		Hit.RingSliceCount = BaseRing.Slices.Num();

		for (int32 Tier = 0; Tier < Rings.Weight.Num(); ++Tier)
		{
			const FArcTerrainResidencyRing& TierRing = Rings.Weight[Tier];
			const int32 TierRadius = TierRing.Slices.IsEmpty() ? -1 : TierRing.Radius;
			const int32 TierSliceShift = Layout.WeightSliceShift + TierRing.CumulativeShift;
			const FIntPoint TierKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, TierSliceShift);
			const int32 TierSide = 2 * TierRadius + 1;
			const FIntPoint TierOffset = TierKey - TierRing.Center + FIntPoint(TierRadius, TierRadius);
			const bool bInTierRing = TierRadius >= 0 && TierOffset.X >= 0 && TierOffset.Y >= 0
				&& TierOffset.X < TierSide && TierOffset.Y < TierSide;
			const int32 TierIndex = bInTierRing ? TierOffset.Y * TierSide + TierOffset.X : INDEX_NONE;
			if (!TierRing.Slices.IsValidIndex(TierIndex) || TierRing.Slices[TierIndex] == TierRing.FillSlice)
			{
				continue;
			}
			Hit.Tier = Tier;
			Hit.CumulativeShift = TierRing.CumulativeShift;
			Hit.SliceShift = TierSliceShift;
			Hit.CellWeightQuads = WeightTierCellQuads(Layout, Ladder, Tier);
			Hit.GroupKey = TierKey;
			Hit.Slice = TierRing.Slices[TierIndex];
			Hit.Radius = TierRadius;
			Hit.RingCenter = TierRing.Center;
			Hit.RingSliceCount = TierRing.Slices.Num();
			Hit.RingIndex = TierIndex;
			break;
		}
		return Hit;
	}

	FIntPoint WeightCellDestOrigin(FArcTerrainCellKey CellKey, FIntPoint GroupKey, int32 SliceShift,
		int32 CellWeightQuads)
	{
		return FIntPoint((CellKey.X - (GroupKey.X << SliceShift)) * CellWeightQuads,
			(CellKey.Y - (GroupKey.Y << SliceShift)) * CellWeightQuads);
	}

	double WeightGroupRectDistSq(const AArcTerrain& Terrain, FIntPoint GroupKey, int32 SliceShift,
		TConstArrayView<FVector> InvokerPositions)
	{
		const double GroupSpan =
			Terrain.Config.CellSize() * static_cast<double>(static_cast<int64>(1) << SliceShift);
		const double MinX = static_cast<double>(GroupKey.X) * GroupSpan;
		const double MinY = static_cast<double>(GroupKey.Y) * GroupSpan;
		return ArcTerrainSubsystemPrivate::RectInvokerDistSq(MinX, MinY, MinX + GroupSpan,
			MinY + GroupSpan, Terrain.GetActorTransform(), InvokerPositions);
	}

	double DistSqToMetres(double DistSq)
	{
		return DistSq < TNumericLimits<double>::Max() ? FMath::Sqrt(DistSq) * 0.01 : -1.0;
	}

	void LogWeightResidency(const AArcTerrain& Terrain, const FArcTerrainState& State,
		FArcTerrainInvokerCache& InvokerCache, FArcTerrainCellKey CellKey)
	{
		if (!State.TileAtlas.IsValid())
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight residency: this terrain has no tile atlas yet.");
			return;
		}
		const FArcTerrainTileAtlas& Atlas = *State.TileAtlas;
		const FArcTerrainTileAtlasLayout& Layout = Atlas.GetLayout();

		TArray<FVector> InvokerPositions;
		int32 NumSubstitutedInvokers = 0;
		ArcTerrain::GatherInvokers(Terrain.GetWorld(), InvokerCache, InvokerPositions,
			NumSubstitutedInvokers);

		const int32 NumTiers = FMath::Max(State.WeightLadder.Tiers.Num(), 1);
		for (int32 Tier = 0; Tier < NumTiers; ++Tier)
		{
			const int32 SliceShift = WeightTierSliceShift(Layout, State.WeightLadder, Tier);
			const FIntPoint GroupKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, SliceShift);
			const FArcTerrainWeightGroupId GroupId{ Tier, GroupKey };
			const int32 Slice = Atlas.FindWeightSlice(GroupId);

			bool bInResidentList = false;
			bool bGroupPinned = false;
			double GroupDistSq = WeightGroupRectDistSq(Terrain, GroupKey, SliceShift, InvokerPositions);
			for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
			{
				if (Group.Tier == Tier && Group.Key == GroupKey)
				{
					bInResidentList = true;
					GroupDistSq = Group.DistSq;
					bGroupPinned = Group.bPinned;
					break;
				}
			}

			UE_LOGF(LogArcTerrainDiag, Log,
				"[DiagAt]   weight residency: tier=%d group=(%d,%d) shift=%d inresidentlist=%ls "
				"(dist=%.1f m off the %ls pinned=%ls) admitted=%ls slice=%d | %ls",
				Tier, GroupKey.X, GroupKey.Y, SliceShift, *YesNo(bInResidentList),
				DistSqToMetres(GroupDistSq),
				bInResidentList ? TEXT("resident record") : TEXT("group rect"), *YesNo(bGroupPinned),
				*YesNo(Atlas.IsWeightGroupAdmitted(GroupId)), Slice,
				*Atlas.DescribeWeightAdmission(GroupId, GroupDistSq));
		}

		const FWeightTierHit Hit = ResolveWeightTierHit(Atlas, State.WeightLadder, CellKey);
		FArcTerrainResidencyRings Rings;
		BuildWeightRings(State, Atlas, Rings);
		const FWeightRingHit RingHit = ResolveWeightRingHit(Layout, State.WeightLadder, Rings, CellKey);
		UE_LOGF(LogArcTerrainDiag, Log,
			"[DiagAt]   weight resolved: admittedtier=%d of %d group=(%d,%d) slice=%d | shadertier=%d "
			"of %d group=(%d,%d) ringcenter=(%d,%d) radius=%d ringindex=%d | material samples %ls",
			Hit.Tier, NumTiers, Hit.GroupKey.X, Hit.GroupKey.Y, Hit.Slice,
			RingHit.Tier, Rings.Weight.Num(), RingHit.GroupKey.X, RingHit.GroupKey.Y,
			RingHit.RingCenter.X, RingHit.RingCenter.Y, RingHit.Radius, RingHit.RingIndex,
			RingHit.Tier != INDEX_NONE
				? *FString::Printf(TEXT("the tier-%d ring's slice %u"), RingHit.Tier, RingHit.Slice)
				: *FString::Printf(TEXT("the FILL slice %d (no tier's ring window reaches this cell)"),
					Layout.GetWeightFillSlice()));

		UE_LOGF(LogArcTerrainDiag, Log,
			"[DiagAt]   weight layout: quads=%d sliceside=%d arrays=%d arrayslices=%d "
			"(allocatable %d + 1 fill) | authored MaxResidentWeightCells=%d effective=%d | "
			"slice=%.1f MB budget=%.0f MB | residentgroups=%d admittedgroups=%d",
			Layout.WeightQuads, Layout.GetWeightSliceSide(), Layout.NumWeightArrays,
			Layout.GetWeightArraySize(), Layout.GetWeightAllocatableSlices(),
			Terrain.MaxResidentWeightCells, Layout.MaxResidentWeightCells,
			static_cast<double>(ArcTerrainAtlas::WeightSliceBytes(Layout.GetWeightSliceSide(),
				Layout.NumWeightArrays)) / (1024.0 * 1024.0),
			static_cast<double>(ArcTerrainAtlas::GetWeightBudgetBytes()) / (1024.0 * 1024.0),
			State.ResidentWeightGroups.Num(), Atlas.GetAdmittedWeightGroupCount());
	}

#if WITH_EDITOR
	/** The bake's own gather+evaluate pairing on the CELL raster, indexed at one texel. */
	void LogCpuWeightProbe(const AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainSourceCache& SourceCache, const FVector& Local, FArcTerrainCellKey CellKey)
	{
		using namespace ArcTerrainSubsystemPrivate;

		const FArcTerrainConfig& Config = Terrain.Config;
		if (Config.ChannelSet == nullptr)
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight CPU: the terrain has no ChannelSet — no weight plane can exist.");
			return;
		}

		const int32 WeightQuads = ResolveWeightQuads(Config, State);
		const double CellSize = Config.CellSize();
		FIntPoint Texel = FIntPoint::ZeroValue;
		FIntPoint GlobalTexel = FIntPoint::ZeroValue;
		if (!ResolveWeightTexel(Local, CellKey, WeightQuads, CellSize, Texel, GlobalTexel))
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight CPU: no texel resolves for this position (weightquads=%d cellsize=%.1f).",
				WeightQuads, CellSize);
			return;
		}

		const ArcTerrainCpuComposite::FCellRaster Raster =
			ArcTerrainCpuComposite::MakeWeightRaster(CellKey, WeightQuads, CellSize);
		const FBox GatherBounds = RasterGatherBounds(Raster.TexelOrigin, Raster.SamplesPerSide,
			CellSize / static_cast<double>(WeightQuads));

		TArray<UArcTerrainStampComponent*> SortedStamps;
		GatherSortedStamps(State, GatherBounds, SortedStamps);

		TArray<int32> PresentChannels;
		GatherPresentWeightChannels(Config, SortedStamps, PresentChannels);
		if (PresentChannels.Num() == 0)
		{
			UE_LOGF(LogArcTerrainDiag, Warning,
				"[DiagAt]   weight CPU: no stamp weight layer resolves against the ChannelSet over this "
				"cell — the composite writes NO plane here, so the material reads whatever the atlas "
				"slice was cleared to.");
			return;
		}

		ArcTerrainCpuComposite::FCellWeightEval Eval;
		ArcTerrainCpuComposite::GatherCellWeightOps(SourceCache, Config, Terrain.GetTerrainTransform(),
			Raster, PresentChannels, SortedStamps, Eval);

		TArray<TArray<uint8>> Planes;
		ArcTerrainCpuComposite::EvaluateCellWeightPlanes(Eval, Planes);
		if (Planes.Num() != PresentChannels.Num())
		{
			UE_LOGF(LogArcTerrainDiag, Warning,
				"[DiagAt]   weight CPU: the evaluation produced %d planes for %d present channels — "
				"nothing to index (raster %d^2, ops %d).",
				Planes.Num(), PresentChannels.Num(), Raster.SamplesPerSide, Eval.Ops.Num());
			return;
		}

		const int32 TexelIndex = Texel.Y * Raster.SamplesPerSide + Texel.X;
		FString Values;
		int32 RawSum = 0;
		float FloatSum = 0.0f;
		for (int32 PlaneIndex = 0; PlaneIndex < Planes.Num(); ++PlaneIndex)
		{
			if (!Planes[PlaneIndex].IsValidIndex(TexelIndex))
			{
				UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight CPU: texel %d is outside plane %d of %d bytes — nothing read.",
					TexelIndex, PlaneIndex, Planes[PlaneIndex].Num());
				return;
			}
			const uint8 Raw = Planes[PlaneIndex][TexelIndex];
			RawSum += Raw;
			FloatSum += static_cast<float>(Raw) / 255.0f;
			Values += FString::Printf(TEXT(" ch%d '%s'=%u(%.4f)"), PresentChannels[PlaneIndex],
				*ChannelName(*Config.ChannelSet, PresentChannels[PlaneIndex]), Raw,
				static_cast<float>(Raw) / 255.0f);
		}

		UE_LOGF(LogArcTerrainDiag, Log,
			"[DiagAt]   weight CPU cell=(%d,%d) texel=(%d,%d) of %d^2 global=(%d,%d) ops=%d complete=%ls "
			"planes=%d :%ls | SUM raw=%d float=%.4f",
			CellKey.X, CellKey.Y, Texel.X, Texel.Y, Raster.SamplesPerSide, GlobalTexel.X, GlobalTexel.Y,
			Eval.Ops.Num(), *YesNo(Eval.bComplete), Planes.Num(), *Values, RawSum, FloatSum);
	}
#endif // WITH_EDITOR

	/** arc.Terrain.Weights.ProbeAtlasReadback: the one measurement that reports what the SHADER sees.
	 *  Blocks the render thread and the GPU, so it is reachable only from this console command. */
	void LogGpuWeightProbe(const AArcTerrain& Terrain, const FArcTerrainState& State, const FVector& Local,
		FArcTerrainCellKey CellKey)
	{
		const FArcTerrainConfig& Config = Terrain.Config;
		if (Config.ChannelSet == nullptr)
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight GPU: the terrain has no ChannelSet — no channel names to report.");
			return;
		}
		if (!State.TileAtlas.IsValid())
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight GPU: no tile atlas on this terrain.");
			return;
		}
		const FArcTerrainTileAtlasLayout Layout = State.TileAtlas->GetLayout();
		TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe> Resources =
			State.TileAtlas->GetResources();
		if (!Resources.IsValid() || Resources->WeightAtlases.Num() == 0)
		{
			UE_LOGF(LogArcTerrainDiag, Warning,
				"[DiagAt]   weight GPU: the weight atlas arrays do not exist yet (nothing has filled "
				"the atlas this session).");
			return;
		}

		FArcTerrainResidencyRings Rings;
		BuildWeightRings(State, *State.TileAtlas, Rings);
		const FWeightRingHit RingHit = ResolveWeightRingHit(Layout, State.WeightLadder, Rings, CellKey);
		const int32 ShaderTier = RingHit.Tier;
		const bool bFillSlice = ShaderTier == INDEX_NONE;
		const int32 Slice = bFillSlice ? Layout.GetWeightFillSlice() : static_cast<int32>(RingHit.Slice);

		FIntPoint Texel = FIntPoint::ZeroValue;
		FIntPoint GlobalTexel = FIntPoint::ZeroValue;
		if (!ResolveWeightTexel(Local, CellKey, RingHit.CellWeightQuads, Config.CellSize(), Texel,
			GlobalTexel))
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight GPU: no texel resolves for this position (tier %d weightquads=%d).",
				ShaderTier, RingHit.CellWeightQuads);
			return;
		}

		// The fill slice is uniform, so only a resolved tier carries a per-cell destination offset.
		const FIntPoint DestOrigin = bFillSlice
			? FIntPoint::ZeroValue
			: WeightCellDestOrigin(CellKey, RingHit.GroupKey, RingHit.SliceShift, RingHit.CellWeightQuads);
		const int32 SliceSide = Layout.GetWeightSliceSide();
		const FIntPoint SliceTexel(FMath::Clamp(DestOrigin.X + Texel.X, 0, SliceSide - 1),
			FMath::Clamp(DestOrigin.Y + Texel.Y, 0, SliceSide - 1));

		TArray<FName> ChannelNames = Config.ChannelSet->Channels;
		const FString TerrainName = Terrain.GetName();

		ENQUEUE_RENDER_COMMAND(ArcTerrainWeightAtlasProbe)(
			[Resources, Slice, SliceTexel, bFillSlice, ShaderTier, ChannelNames, TerrainName, CellKey](
				FRHICommandListImmediate& RHICmdList)
			{
				const int32 NumArrays = Resources->WeightAtlases.Num();
				TArray<TUniquePtr<FRHIGPUTextureReadback>> Readbacks;
				Readbacks.Reserve(NumArrays);
				for (int32 ArrayIndex = 0; ArrayIndex < NumArrays; ++ArrayIndex)
				{
					IPooledRenderTarget* Pooled = Resources->WeightAtlases[ArrayIndex].GetReference();
					FRHITexture* Texture = Pooled != nullptr ? Pooled->GetRHI() : nullptr;
					if (Texture == nullptr || Texture->GetDesc().Format != PF_B8G8R8A8
						|| Slice >= static_cast<int32>(Texture->GetDesc().ArraySize)
						|| SliceTexel.X >= Texture->GetDesc().Extent.X
						|| SliceTexel.Y >= Texture->GetDesc().Extent.Y)
					{
						UE_LOGF(LogArcTerrainDiag, Warning,
							"[DiagAt]   weight GPU array %d: not readable (texture=%ls format=%d "
							"slice %d, texel (%d,%d)).",
							ArrayIndex, Texture != nullptr ? TEXT("valid") : TEXT("null"),
							Texture != nullptr ? static_cast<int32>(Texture->GetDesc().Format) : -1,
							Slice, SliceTexel.X, SliceTexel.Y);
						Readbacks.Add(nullptr);
						continue;
					}

					TUniquePtr<FRHIGPUTextureReadback> Readback =
						MakeUnique<FRHIGPUTextureReadback>(TEXT("ArcTerrainWeightAtlasProbe"));
					RHICmdList.Transition(
						FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
					Readback->EnqueueCopy(RHICmdList, Texture,
						FIntVector(SliceTexel.X, SliceTexel.Y, 0), static_cast<uint32>(Slice),
						FIntVector(1, 1, 1));
					RHICmdList.Transition(
						FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
					Readbacks.Add(MoveTemp(Readback));
				}

				RHICmdList.SubmitAndBlockUntilGPUIdle();

				FString Values;
				int32 RawSum = 0;
				float FloatSum = 0.0f;
				for (int32 ArrayIndex = 0; ArrayIndex < Readbacks.Num(); ++ArrayIndex)
				{
					if (!Readbacks[ArrayIndex].IsValid())
					{
						Values += FString::Printf(TEXT(" [array %d unreadable]"), ArrayIndex);
						continue;
					}
					int32 RowPitchInPixels = 0;
					const FColor* Pixels =
						static_cast<const FColor*>(Readbacks[ArrayIndex]->Lock(RowPitchInPixels));
					if (Pixels == nullptr)
					{
						Values += FString::Printf(TEXT(" [array %d lock failed]"), ArrayIndex);
						continue;
					}
					// PF_B8G8R8A8: component 0..3 of the atlas packing is FColor's R,G,B,A.
					const uint8 Components[4] = { Pixels->R, Pixels->G, Pixels->B, Pixels->A };
					Readbacks[ArrayIndex]->Unlock();

					for (int32 Component = 0; Component < 4; ++Component)
					{
						const int32 ChannelIndex = ArrayIndex * 4 + Component;
						if (!ChannelNames.IsValidIndex(ChannelIndex))
						{
							continue;
						}
						const uint8 Raw = Components[Component];
						RawSum += Raw;
						FloatSum += static_cast<float>(Raw) / 255.0f;
						Values += FString::Printf(TEXT(" ch%d '%s'=%u(%.4f)"), ChannelIndex,
							*ChannelNames[ChannelIndex].ToString(), Raw, static_cast<float>(Raw) / 255.0f);
					}
				}

				UE_LOGF(LogArcTerrainDiag, Log,
					"[DiagAt]   weight GPU '%ls' cell=(%d,%d) shadertier=%d slice=%d(%ls) "
					"slicetexel=(%d,%d) arrays=%d :%ls | SUM raw=%d float=%.4f",
					*TerrainName, CellKey.X, CellKey.Y, ShaderTier, Slice,
					bFillSlice ? TEXT("FILL SLICE — no weight tier's ring window reaches this cell")
						: TEXT("the tier's ring slice"),
					SliceTexel.X, SliceTexel.Y, Readbacks.Num(), *Values, RawSum, FloatSum);
			});

		FlushRenderingCommands();
	}

	/** arc.Terrain.Weights.VerboseComposite: the CPU mirror of the vertex factory's weight ADDRESSING —
	 *  ArcTerrainVertexFactory.ush's tier walk over arc_WeightRingIndexForTier, ShadeWeightSlice and
	 *  ShadeWeightUV, then the material's slice-UV remap from ArcTerrainMaterialExpressions.cpp
	 *  CompileSliceUV — printed next to the texel the readback probe reads. */
	void LogShaderWeightAddressing(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FVector& Local, FArcTerrainCellKey CellKey)
	{
		if (!State.TileAtlas.IsValid())
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   weight addressing: this terrain has no tile atlas — nothing to mirror.");
			return;
		}

		const FArcTerrainConfig& Config = Terrain.Config;
		const FArcTerrainTileAtlasLayout& Layout = State.TileAtlas->GetLayout();
		const float TexelToLocal = ArcTerrainCellMath::CellTexelToLocal(Config.CellSize(), Layout.HeightQuads);
		if (TexelToLocal <= 0.0f || Layout.WeightQuads <= 0)
		{
			UE_LOGF(LogArcTerrainDiag, Warning,
				"[DiagAt]   weight addressing: the layout is not usable (heightquads=%d weightquads=%d "
				"texeltolocal=%.6f).",
				Layout.HeightQuads, Layout.WeightQuads, TexelToLocal);
			return;
		}

		FArcTerrainResidencyRings Rings;
		BuildWeightRings(State, *State.TileAtlas, Rings);
		const FWeightRingHit RingHit = ResolveWeightRingHit(Layout, State.WeightLadder, Rings, CellKey);

		const double TexelX = Local.X / static_cast<double>(TexelToLocal);
		const double TexelY = Local.Y / static_cast<double>(TexelToLocal);
		const double WholeX = FMath::FloorToDouble(TexelX);
		const double WholeY = FMath::FloorToDouble(TexelY);
		const FIntPoint GlobalTexel(static_cast<int32>(WholeX), static_cast<int32>(WholeY));
		const FVector2D TexelFrac(TexelX - WholeX, TexelY - WholeY);

		const int32 BaseWeightQuadsShift = Layout.GetWeightQuadsShift();
		const int32 ShaderTier = RingHit.Tier;
		const uint32 ShaderSlice = RingHit.Slice;
		const bool bShaderFillSlice = ShaderTier == INDEX_NONE;

		const int32 WeightQuadsShift = BaseWeightQuadsShift + RingHit.CumulativeShift;
		const FIntPoint ShaderKey(GlobalTexel.X >> WeightQuadsShift, GlobalTexel.Y >> WeightQuadsShift);

		const double GroupSpan = static_cast<double>(static_cast<int64>(1) << WeightQuadsShift);
		const FVector2D WeightUV(
			FMath::Clamp((static_cast<double>(GlobalTexel.X - (ShaderKey.X << WeightQuadsShift)) + TexelFrac.X)
				/ GroupSpan, 0.0, 1.0),
			FMath::Clamp((static_cast<double>(GlobalTexel.Y - (ShaderKey.Y << WeightQuadsShift)) + TexelFrac.Y)
				/ GroupSpan, 0.0, 1.0));

		const int32 SliceSide = Layout.GetWeightSliceSide();
		float InvRes = 0.0f;
		bool bInvResFromMID = false;
		const UArcTerrainRenderComponent* RenderComponent =
			Terrain.FindComponentByClass<UArcTerrainRenderComponent>();
		UMaterialInstanceDynamic* MID = RenderComponent != nullptr
			? Cast<UMaterialInstanceDynamic>(RenderComponent->GetMaterial(0))
			: nullptr;
		if (MID != nullptr)
		{
			bInvResFromMID = MID->GetScalarParameterValue(
				FHashedMaterialParameterInfo(FName(TEXT("ArcTerrainWeightInvRes"))), InvRes);
		}
		if (!bInvResFromMID)
		{
			InvRes = 1.0f / static_cast<float>(FMath::Max(SliceSide, 1));
		}

		const double RemappedU = WeightUV.X * (1.0 - 3.0 * InvRes) + 1.5 * InvRes;
		const double RemappedV = WeightUV.Y * (1.0 - 3.0 * InvRes) + 1.5 * InvRes;
		const FIntPoint ShaderTexel(FMath::FloorToInt32(RemappedU * static_cast<double>(SliceSide)),
			FMath::FloorToInt32(RemappedV * static_cast<double>(SliceSide)));

		FIntPoint ProbeTexel = FIntPoint::ZeroValue;
		FIntPoint ProbeGlobalTexel = FIntPoint::ZeroValue;
		const bool bProbeResolved = ResolveWeightTexel(Local, CellKey, RingHit.CellWeightQuads,
			Config.CellSize(), ProbeTexel, ProbeGlobalTexel);
		const int32 AdmittedSlice = bShaderFillSlice
			? INDEX_NONE
			: State.TileAtlas->FindWeightSlice(FArcTerrainWeightGroupId{ ShaderTier, RingHit.GroupKey });
		const FIntPoint DestOrigin = AdmittedSlice == INDEX_NONE
			? FIntPoint::ZeroValue
			: WeightCellDestOrigin(CellKey, RingHit.GroupKey, RingHit.SliceShift, RingHit.CellWeightQuads);
		const FIntPoint ProbeSliceTexel(FMath::Clamp(DestOrigin.X + ProbeTexel.X, 0, SliceSide - 1),
			FMath::Clamp(DestOrigin.Y + ProbeTexel.Y, 0, SliceSide - 1));
		const int32 ProbeSlice = AdmittedSlice == INDEX_NONE ? Layout.GetWeightFillSlice() : AdmittedSlice;

		const bool bDiverges = !bProbeResolved || ProbeSliceTexel != ShaderTexel
			|| static_cast<int32>(ShaderSlice) != ProbeSlice || ShaderKey != RingHit.GroupKey;

		UE_LOGF(LogArcTerrainDiag, Log,
			"[DiagAt]   weight addressing: globaltexel=(%d,%d) frac=(%.4f,%.4f) shadertier=%d of %d "
			"weightquadsshift=%d cellweightquads=%d shaderkey=(%d,%d) ringkeyfromcell=(%d,%d) | "
			"ring center=(%d,%d) radius=%d slices=%d ringindex=%d -> slice=%u(%ls) | "
			"weightuv=(%.6f,%.6f) invres=%.9f(%ls) remapped=(%.6f,%.6f) -> SHADER TEXEL=(%d,%d) | "
			"PROBE TEXEL=(%d,%d)%ls slice=%d(%ls) | %ls | "
			"the factory keys on the CBT LEAF CENTROID, unreachable from the subsystem — this uses the "
			"probed position's own height texel",
			GlobalTexel.X, GlobalTexel.Y, TexelFrac.X, TexelFrac.Y, ShaderTier, Rings.Weight.Num(),
			WeightQuadsShift, RingHit.CellWeightQuads, ShaderKey.X, ShaderKey.Y,
			RingHit.GroupKey.X, RingHit.GroupKey.Y,
			RingHit.RingCenter.X, RingHit.RingCenter.Y, RingHit.Radius, RingHit.RingSliceCount,
			RingHit.RingIndex, ShaderSlice,
			bShaderFillSlice ? TEXT("FILL") : TEXT("admitted"),
			WeightUV.X, WeightUV.Y, InvRes,
			bInvResFromMID ? TEXT("read back from the MID") : TEXT("NO MID — derived from the layout"),
			RemappedU, RemappedV, ShaderTexel.X, ShaderTexel.Y,
			ProbeSliceTexel.X, ProbeSliceTexel.Y, bProbeResolved ? TEXT("") : TEXT("(UNRESOLVED)"),
			ProbeSlice, AdmittedSlice == INDEX_NONE ? TEXT("FILL") : TEXT("admitted"),
			bDiverges ? TEXT("DIVERGENT") : TEXT("agrees"));
	}

	void Execute(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		UArcTerrainSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
		if (Subsystem == nullptr)
		{
			Output.Log(TEXT("arc.Terrain.DiagAt: no terrain subsystem in this world."));
			return;
		}

		if (Args.Num() >= 3)
		{
			Subsystem->LogPointDiagnostics(FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]),
				FCString::Atod(*Args[2])));
			return;
		}

		// No explicit position: fall back to the local camera/pawn view, not GatherInvokers' residency
		// anchors — that order is a residency-ring implementation detail, not a view contract.
		FVector ViewPosition;
		if (!ArcTerrain::GetViewPosition(World, ViewPosition))
		{
			Output.Log(TEXT("arc.Terrain.DiagAt: no explicit X Y Z given and no camera/pawn view is ")
				TEXT("available to fall back on."));
			return;
		}
		Subsystem->LogPointDiagnostics(ViewPosition);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GArcTerrainDiagAtCommand(
	TEXT("arc.Terrain.DiagAt"),
	TEXT("arc.Terrain.DiagAt [X Y Z] — dump the cell, the weight-slice residency + admission verdict, the ")
	TEXT("per-channel CPU weight values at the probed texel, the three tile rails and the collision patch ")
	TEXT("covering one WORLD position (cm), for every registered terrain. With no arguments, uses the ")
	TEXT("current camera/pawn view position. arc.Terrain.Weights.ProbeAtlasReadback 1 adds the GPU atlas ")
	TEXT("readback of the same texel."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ArcTerrainDiagCommand_3b6c::Execute));

void UArcTerrainSubsystem::LogPointDiagnostics(const FVector& WorldPosition)
{
	using namespace ArcTerrainDiagCommand_3b6c;

	if (Terrains.IsEmpty())
	{
		UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt] no terrain is registered in this world.");
		return;
	}

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			continue;
		}
		FArcTerrainState& State = TerrainPair.Value;
		const FArcTerrainConfig& Config = Terrain->Config;
		const FVector Local = Terrain->WorldToTerrainLocal(WorldPosition);
		const FArcTerrainCellKey CellKey = ArcTerrain::CellKeyFromLocal(Local, Config);

		UE_LOGF(LogArcTerrainDiag, Log,"[DiagAt] '%ls' world=(%.0f,%.0f,%.0f) local=(%.0f,%.0f,%.0f) cell=(%d,%d)",
			*Terrain->GetName(), WorldPosition.X, WorldPosition.Y, WorldPosition.Z, Local.X, Local.Y, Local.Z,
			CellKey.X, CellKey.Y);

		const FArcTerrainCellState* Cell = State.Cells.Find(CellKey);
		FArcTerrainSectorId SectorId;
		bool bSectorAdmitted = false;
		int32 HeightSlice = INDEX_NONE;
		int32 WeightSlice = INDEX_NONE;
		int32 WeightTier = INDEX_NONE;
		if (State.TileAtlas.IsValid())
		{
			const FArcTerrainTileAtlasLayout& AtlasLayout = State.TileAtlas->GetLayout();
			SectorId.Key = ArcTerrainAtlas::GroupKeyFromCell(CellKey, AtlasLayout.SectorShift);
			bSectorAdmitted = State.TileAtlas->IsSectorAdmitted(SectorId);
			HeightSlice = State.TileAtlas->FindHeightSlice(SectorId);
			const FWeightTierHit WeightHit =
				ResolveWeightTierHit(*State.TileAtlas, State.WeightLadder, CellKey);
			WeightSlice = WeightHit.Slice;
			WeightTier = WeightHit.Tier;
		}
		UE_LOGF(LogArcTerrainDiag, Log,
			"[DiagAt]   cell state=%ls version=%" UINT64_FMT " atlasheightversion=%" UINT64_FMT " "
			"heighttileserial=%" UINT64_FMT " atlasheighttileserial=%" UINT64_FMT " refillpending=%ls | "
			"sector=(%d,%d) admitted=%ls heightslice=%d weightslice=%d admittedweighttier=%d",
			*YesNo(Cell != nullptr),
			Cell != nullptr ? Cell->Version : 0,
			Cell != nullptr ? Cell->AtlasHeightVersion : 0,
			Cell != nullptr ? Cell->HeightTileSerial : 0,
			Cell != nullptr ? Cell->AtlasHeightTileSerial : 0,
			*YesNo(Cell != nullptr && Cell->bAtlasRefillPending),
			SectorId.Key.X, SectorId.Key.Y, *YesNo(bSectorAdmitted), HeightSlice, WeightSlice, WeightTier);

		LogWeightResidency(*Terrain, State, InvokerCache, CellKey);
		if (ArcTerrainDiag::IsWeightCompositeVerbose())
		{
			LogShaderWeightAddressing(*Terrain, State, Local, CellKey);
		}
#if WITH_EDITOR
		LogCpuWeightProbe(*Terrain, State, SourceCache, Local, CellKey);
#else
		UE_LOG(LogArcTerrainDiag, Warning,
			TEXT("[DiagAt]   weight CPU: the present-channel gather is editor-only — no CPU probe in this ")
			TEXT("build."));
#endif
		if (ArcTerrainDiag::IsAtlasReadbackProbeEnabled())
		{
			LogGpuWeightProbe(*Terrain, State, Local, CellKey);
		}

		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			const EArcTerrainRail Rail = static_cast<EArcTerrainRail>(RailIndex);
			FIntPoint TileKey = FIntPoint::ZeroValue;
			FArcTerrainTileGrid RailGrid;
			FArcTerrainTileProbe Probe;
			if (!ArcTerrainTileAccess::ResolveGrid(*Terrain, Rail, 0, RailGrid)
				|| !ArcTerrainTileAccess::TileKeyFromLocal(RailGrid, FVector2D(Local.X, Local.Y), TileKey)
				|| !ArcTerrainTileAccess::ProbeTile(*Terrain, Rail, 0, TileKey, Probe))
			{
				UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   %-6ls NO LATTICE — this rail has no tiles.",
					ArcTerrainDiag::RailName(Rail));
				continue;
			}
			bool bQueuedForRebake = false;
#if WITH_EDITOR
			bQueuedForRebake = State.PendingTiles(Rail, 0).Contains(TileKey);
#endif
			UE_LOGF(LogArcTerrainDiag, Log,
				"[DiagAt]   %-6ls tile=(%d,%d) manifest=%ls file=%ls(%" INT64_FMT " B) parsed=%ls "
				"compression=%u body=%u/%u B matcheslattice=%ls loaded=%ls suppressed=%ls bakequeue=%ls%ls%ls",
				ArcTerrainDiag::RailName(Rail), TileKey.X, TileKey.Y, *YesNo(Probe.bInManifest),
				*YesNo(Probe.bFileExists), Probe.FileBytes, *YesNo(Probe.bParsed),
				static_cast<uint32>(Probe.Header.CompressionMethod), Probe.Header.CompressedBodyBytes,
				Probe.Header.UncompressedBodyBytes, *YesNo(Probe.bCompatible), *YesNo(Probe.bLoaded), *YesNo(Probe.bSuppressedUnusable),
				*YesNo(bQueuedForRebake),
				Probe.Mismatch.IsEmpty() ? TEXT("") : TEXT(" mismatch: "),
				Probe.Mismatch.IsEmpty() ? TEXT("") : *Probe.Mismatch);
			UE_LOGF(LogArcTerrainDiag, Log,"[DiagAt]   %-6ls path='%ls'",
				ArcTerrainDiag::RailName(Rail), *Probe.FilePath);
		}

		const double PatchSize = Terrain->CollisionPatchSize();
		if (PatchSize <= 0.0)
		{
			UE_LOGF(LogArcTerrainDiag, Warning,"[DiagAt]   collision: patch extent is unusable.");
			continue;
		}
		const FArcTerrainCollisionKey PatchKey(
			static_cast<int32>(FMath::FloorToDouble(Local.X / PatchSize)),
			static_cast<int32>(FMath::FloorToDouble(Local.Y / PatchSize)));
		const FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(PatchKey);
		UE_LOGF(LogArcTerrainDiag, Log,"[DiagAt]   patch=(%d,%d) state=%ls component=%ls dirtyserial=%" UINT64_FMT " applied=%"
				UINT64_FMT " heightsourcemissing=%ls",
			PatchKey.X, PatchKey.Y, *YesNo(Patch != nullptr),
			*YesNo(Patch != nullptr && Patch->CollisionComponent.IsValid()),
			Patch != nullptr ? Patch->DirtySerial : 0,
			Patch != nullptr ? Patch->AppliedDirtySerial : 0,
			*YesNo(Patch != nullptr && Patch->bCollisionHeightSourceMissing));
	}
}

void UArcTerrainSubsystem::AddOwnedRenderTarget(UTextureRenderTarget2D& RenderTarget)
{
	OwnedRenderTargets.Add(&RenderTarget);
}
