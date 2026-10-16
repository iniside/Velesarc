// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainSubsystem.h"
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
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/ObjectKey.h"

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

namespace ArcTerrainStreamWindowLocal_e8c4
{
	// THE BOUND ON THE STREAM WALK, and it is a bound on the WALK — not a streaming budget. The radius
	// alone decides which tiles load; this only stops the per-invoker key enumeration (257^2 keys here)
	// from growing without limit when a degenerate terrain scale shrinks the lattice under the radius.
	// At the 1 km default tile it stands 128 km out, which no authored radius reaches.
	constexpr int32 GMaxStreamWindowSpan = 128;
	// Per terrain AND per rail AND per tier: one throttle shared across tiers would let a coarse tier's
	// clamp be swallowed by tier 0's recent entry.
	TMap<TTuple<FObjectKey, uint8, int32>, double> GStreamWindowClampLogTimes;

	// The window walk hit GMaxStreamWindowSpan, so tiles past that span never load whatever the radius
	// says — silence would hide it.
	void LogStreamWindowClamped(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier)
	{
		check(IsInGameThread());

		const TTuple<FObjectKey, uint8, int32> LogKey(FObjectKey(&Terrain), static_cast<uint8>(Rail), Tier);
		const double Now = FPlatformTime::Seconds();
		double& LastSeconds = GStreamWindowClampLogTimes.FindOrAdd(LogKey);
		if (LastSeconds > 0.0 && Now - LastSeconds < 10.0)
		{
			return;
		}
		LastSeconds = Now;

		if (Tier == 0)
		{
			UE_LOGF(LogArcTerrain, Warning,
				"%ls: tile stream window on rail %u cut to %d keys either side of the invoker — the "
				"terrain scale shrinks the tile lattice so far below the radius asked for that the walk "
				"hit its bound, so tiles past that span never load.",
				*Terrain.GetName(), static_cast<uint32>(Rail), GMaxStreamWindowSpan);
		}
		else
		{
			UE_LOGF(LogArcTerrain, Warning,
				"%ls: tile stream window on rail %u tier %d cut to %d keys either side of the invoker — "
				"the terrain scale shrinks the tile lattice so far below the radius asked for that the "
				"walk hit its bound, so tiles past that span never load.",
				*Terrain.GetName(), static_cast<uint32>(Rail), Tier, GMaxStreamWindowSpan);
		}
	}
}

namespace ArcTerrainStreamCacheLocal_b7e2
{
	constexpr int32 GStreamReleaseRetryBudget = 32;
	constexpr double GStreamReleaseRetryBackoffSeconds = 0.25;

	void SortDedupWindows(TArray<ArcTerrainSubsystemPrivate::FSectorWindow>& Windows)
	{
		if (Windows.Num() < 2)
		{
			return;
		}
		Windows.Sort();
		int32 WriteIndex = 1;
		for (int32 ReadIndex = 1; ReadIndex < Windows.Num(); ++ReadIndex)
		{
			if (!(Windows[ReadIndex] == Windows[WriteIndex - 1]))
			{
				Windows[WriteIndex] = Windows[ReadIndex];
				++WriteIndex;
			}
		}
		Windows.SetNum(WriteIndex, EAllowShrinking::No);
	}

	void CollectWindows(TConstArrayView<FVector> InvokerPositions, double WorldRadius, double TileSize,
		const FTransform& TerrainTransform, TArray<ArcTerrainSubsystemPrivate::FSectorWindow>& OutWindows,
		bool& bOutAnyClamped)
	{
		OutWindows.Reset();
		OutWindows.Reserve(InvokerPositions.Num());
		for (const FVector& Invoker : InvokerPositions)
		{
			const ArcTerrainSubsystemPrivate::FSectorWindow Window =
				ArcTerrainSubsystemPrivate::ComputeSectorWindow(Invoker, WorldRadius, 0, TileSize,
					TerrainTransform, static_cast<double>(ArcTerrainStreamWindowLocal_e8c4::GMaxStreamWindowSpan));
			bOutAnyClamped |= Window.bSpanClamped;
			if (Window.MaxX < Window.MinX)
			{
				continue;
			}
			OutWindows.Add(Window);
		}
		SortDedupWindows(OutWindows);
	}

	bool ContainsKey(TConstArrayView<ArcTerrainSubsystemPrivate::FSectorWindow> Windows, FIntPoint Key)
	{
		for (const ArcTerrainSubsystemPrivate::FSectorWindow& Window : Windows)
		{
			if (Key.X >= Window.MinX && Key.X <= Window.MaxX && Key.Y >= Window.MinY && Key.Y <= Window.MaxY)
			{
				return true;
			}
		}
		return false;
	}

	void QueuePendingRelease(FArcTerrainState& State, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		const FArcTerrainStreamPendingRelease Pending{ Rail, Tier, TileKey };
		if (!State.StreamCache.PendingReleaseKeys.Contains(Pending))
		{
			State.StreamCache.PendingReleaseKeys.Add(Pending);
		}
	}

	bool IsPendingKeyInRetainUnion(const FArcTerrainState& State, const FArcTerrainStreamPendingRelease& Pending)
	{
		const int32 RailIndex = static_cast<int32>(Pending.Rail);
		if (RailIndex < 0 || RailIndex >= ArcTerrain::NumTileRails
			|| !ArcTerrainTileAccess::IsValidTier(Pending.Tier))
		{
			return false;
		}
		const FArcTerrainStreamLaneCache& Lane = State.StreamCache.Lanes[RailIndex][Pending.Tier];
		return Lane.bGridValid && ContainsKey(Lane.OuterWindows, Pending.TileKey);
	}

	void DropPendingKeysInRetainUnion(FArcTerrainState& State)
	{
		TArray<FArcTerrainStreamPendingRelease>& PendingKeys = State.StreamCache.PendingReleaseKeys;
		if (PendingKeys.IsEmpty())
		{
			return;
		}
		TArray<FArcTerrainStreamPendingRelease> Kept;
		Kept.Reserve(PendingKeys.Num());
		for (const FArcTerrainStreamPendingRelease& Pending : PendingKeys)
		{
			if (!IsPendingKeyInRetainUnion(State, Pending))
			{
				Kept.Add(Pending);
			}
		}
		PendingKeys = MoveTemp(Kept);
	}

	void ClearLane(FArcTerrainStreamLaneCache& Lane)
	{
		Lane.InnerWindows.Reset();
		Lane.OuterWindows.Reset();
		Lane.RequestedTiles.Reset();
		Lane.RetainedTiles.Reset();
		Lane.TileSize = 0.0;
		Lane.StreamRadiusCm = 0.0;
		Lane.StreamUnloadRadiusCm = 0.0;
		Lane.bGridValid = false;
	}

	bool ResolveLaneRadii(EArcTerrainRail TileRail, int32 Tier, const FArcTerrainTierLadder& Ladder,
		const FArcTerrainWeightLadder& WeightLadder, double& OutStreamRadiusCm,
		double& OutStreamUnloadRadiusCm)
	{
		if (TileRail == EArcTerrainRail::Weight)
		{
			if (!WeightLadder.Tiers.IsValidIndex(Tier))
			{
				return false;
			}
			const FResolvedWeightTier& WeightTierEntry = WeightLadder.Tiers[Tier];
			OutStreamRadiusCm = WeightTierEntry.StreamRadiusCm;
			OutStreamUnloadRadiusCm = WeightTierEntry.StreamUnloadRadiusCm;
			return true;
		}
		if (!Ladder.Tiers.IsValidIndex(Tier))
		{
			return false;
		}
		const FResolvedTier& TierEntry = Ladder.Tiers[Tier];
		OutStreamRadiusCm = TierEntry.StreamRadiusCm;
		OutStreamUnloadRadiusCm = TierEntry.StreamUnloadRadiusCm;
		return true;
	}
}

void UArcTerrainSubsystem::OnTerrainStreamLayoutChanged(AArcTerrain& Terrain)
{
	FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr)
	{
		return;
	}
	++State->StreamLayoutRevision;
	++State->ResidencyConfigRevision;
	InvalidateStreamWindows(*State);
	ArcTerrainResidencyIndex_c4e8::RebuildIndexes(Terrain, *State);
	FArcTerrainTileAuthorityEvent LayoutEvent;
	LayoutEvent.Reason = EArcTerrainTileAuthorityReason::LayoutChanged;
	PublishLocalTileAuthorityEvent(*State, LayoutEvent);
}

void UArcTerrainSubsystem::InvalidateStreamWindows(FArcTerrainState& State)
{
	State.StreamCache.bValid = false;
}

void UArcTerrainSubsystem::ClearStreamReleaseTimer()
{
	ArmedStreamReleaseRetryAt = 0.0;
	UWorld* World = GetWorld();
	if (World != nullptr && StreamReleaseTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(StreamReleaseTimerHandle);
	}
	StreamReleaseTimerHandle.Invalidate();
}

void UArcTerrainSubsystem::ArmStreamReleaseRetry(double RetryAt)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (RetryAt <= Now)
	{
		HandleStreamReleaseRetry();
		return;
	}
	if (StreamReleaseTimerHandle.IsValid() && ArmedStreamReleaseRetryAt > 0.0
		&& ArmedStreamReleaseRetryAt <= RetryAt)
	{
		return;
	}
	ArmedStreamReleaseRetryAt = RetryAt;
	const float DelaySeconds = static_cast<float>(RetryAt - Now);
	World->GetTimerManager().SetTimer(
		StreamReleaseTimerHandle,
		FTimerDelegate::CreateUObject(this, &UArcTerrainSubsystem::HandleStreamReleaseRetry),
		DelaySeconds,
		false);
}

void UArcTerrainSubsystem::HandleStreamReleaseRetry()
{
	using namespace ArcTerrainStreamCacheLocal_b7e2;

	TArray<FVector> InvokerPositions;
	int32 NumSubstitutedInvokers = 0;
	ArcTerrain::GatherInvokers(GetWorld(), InvokerCache, InvokerPositions, NumSubstitutedInvokers);
	if (InvokerPositions.IsEmpty() || NumSubstitutedInvokers == InvokerPositions.Num())
	{
		return;
	}

	ClearStreamReleaseTimer();

	int32 Tried = 0;
	bool bAnyRemaining = false;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		FArcTerrainState& State = TerrainPair.Value;
		TArray<FArcTerrainStreamPendingRelease> Untried;
		TArray<FArcTerrainStreamPendingRelease> StillBlocked;
		for (const FArcTerrainStreamPendingRelease& Pending : State.StreamCache.PendingReleaseKeys)
		{
			if (IsPendingKeyInRetainUnion(State, Pending))
			{
				continue;
			}
			if (Tried >= GStreamReleaseRetryBudget)
			{
				Untried.Add(Pending);
				continue;
			}
			++Tried;
			if (Terrain == nullptr)
			{
				continue;
			}
			if (!ArcTerrainTileAccess::ReleaseTileIfUnreferenced(*Terrain, Pending.Rail, Pending.Tier,
				Pending.TileKey))
			{
				StillBlocked.Add(Pending);
			}
		}
		Untried.Append(StillBlocked);
		State.StreamCache.PendingReleaseKeys = MoveTemp(Untried);
		bAnyRemaining |= State.StreamCache.PendingReleaseKeys.Num() > 0;
	}

	if (bAnyRemaining)
	{
		ArmStreamReleaseRetry(FPlatformTime::Seconds() + GStreamReleaseRetryBackoffSeconds);
	}
}

void UArcTerrainSubsystem::TickTileStreaming(TConstArrayView<FVector> InvokerPositions)
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainStreamCacheLocal_b7e2;
	using namespace ArcTerrainStreamWindowLocal_e8c4;

	// No invoker is no anchor, not "everything is far": releasing on it would drop every payload the
	// editor bakers and the PCG paths loaded synchronously the moment no viewport answers. The manifest
	// write below still runs — a synchronous load is exactly what finds an entry with no file, and it
	// happens in the sessions that have no invoker at all.
	const bool bHasAnchor = !InvokerPositions.IsEmpty();

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			continue; // dead-terrain reclaim is phase 1's job
		}

		FArcTerrainState& State = TerrainPair.Value;
		const FTransform TerrainTransform = Terrain->GetTerrainTransform();
		const FVector Location = TerrainTransform.GetLocation();
		const FQuat Rotation = TerrainTransform.GetRotation();
		const FVector Scale = TerrainTransform.GetScale3D();
		if (!State.StreamCache.bHasTransformKey)
		{
			State.StreamCache.CachedLocation = Location;
			State.StreamCache.CachedRotation = Rotation;
			State.StreamCache.CachedScale = Scale;
			State.StreamCache.bHasTransformKey = true;
		}
		else if (State.StreamCache.CachedLocation != Location
			|| State.StreamCache.CachedRotation != Rotation
			|| State.StreamCache.CachedScale != Scale)
		{
			State.StreamCache.CachedLocation = Location;
			State.StreamCache.CachedRotation = Rotation;
			State.StreamCache.CachedScale = Scale;
			++State.StreamLayoutRevision;
			InvalidateStreamWindows(State);
		}

		if (!bHasAnchor)
		{
			// Cleared, not carried: the layout this was resolved from is not resolved on this path, so a
			// kept ladder would answer for tiers a config edit has since dropped.
			State.TierLadder = FArcTerrainTierLadder();
			State.WeightLadder = FArcTerrainWeightLadder();
			State.StreamCache.bValid = false;
			State.StreamCache.bHadInvokers = false;
			ArcTerrainTileAccess::FlushManifest(*Terrain);
			continue;
		}

		const uint32 ManifestEpoch = ArcTerrainTileAccess::GetManifestCoverageEpoch();
		const uint32 LoadedEpoch = ArcTerrainTileAccess::GetLoadedCoverageEpoch();
		bool bNeedReconcile = !State.StreamCache.bValid
			|| !State.StreamCache.bHadInvokers
			|| State.StreamCache.ConsumedStreamLayoutRevision != State.StreamLayoutRevision
			|| State.StreamCache.ConsumedDerivedGridRevision != State.DerivedGridRevision
			|| State.StreamCache.ConsumedResidentSetRevision != State.ResidentSetRevision
			|| State.StreamCache.ConsumedManifestCoverageEpoch != ManifestEpoch
			|| State.StreamCache.ConsumedLoadedCoverageEpoch != LoadedEpoch;

		if (!bNeedReconcile)
		{
			for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails && !bNeedReconcile; ++Rail)
			{
				for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers && !bNeedReconcile; ++Tier)
				{
					const FArcTerrainStreamLaneCache& Lane = State.StreamCache.Lanes[Rail][Tier];
					if (!Lane.bGridValid)
					{
						continue;
					}
					TArray<FSectorWindow> InnerWindows;
					TArray<FSectorWindow> OuterWindows;
					bool bUnusedClamp = false;
					CollectWindows(InvokerPositions, Lane.StreamRadiusCm, Lane.TileSize, TerrainTransform,
						InnerWindows, bUnusedClamp);
					CollectWindows(InvokerPositions, Lane.StreamUnloadRadiusCm, Lane.TileSize,
						TerrainTransform, OuterWindows, bUnusedClamp);
					if (InnerWindows != Lane.InnerWindows || OuterWindows != Lane.OuterWindows)
					{
						bNeedReconcile = true;
					}
				}
			}
		}

		if (!bNeedReconcile)
		{
			// Pending save only — membership stays on the cached windows.
			ArcTerrainTileAccess::FlushManifest(*Terrain);
			continue;
		}

		const FResolvedResidency Residency = ResolveResidency(*Terrain);
		const FArcTerrainTileAtlasLayout Layout =
			ResolveTerrainAtlasLayout(*Terrain, State, Residency);
		const FArcTerrainTierLadder Ladder =
			ArcTerrain::ResolveTierLadder(*Terrain, Layout);
		State.TierLadder = Ladder;
		State.WeightLadder =
			ArcTerrain::ResolveWeightTierLadder(*Terrain, Layout, Layout.WeightQuads);
		const FArcTerrainWeightLadder& WeightLadder = State.WeightLadder;

		const TWeakObjectPtr<UArcTerrainSubsystem> WeakThis(this);
		const TWeakObjectPtr<AArcTerrain> WeakTerrain(Terrain);
		bool bArmedPendingRelease = false;

		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			const EArcTerrainRail TileRail = static_cast<EArcTerrainRail>(Rail);
			const bool bWeightRail = TileRail == EArcTerrainRail::Weight;
			const int32 NumLadderTiers = bWeightRail ? WeightLadder.Tiers.Num() : Ladder.Tiers.Num();
			const int32 NumRailTiers = TileRail == EArcTerrainRail::Data ? 1 : NumLadderTiers;

			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				FArcTerrainStreamLaneCache& Lane = State.StreamCache.Lanes[Rail][Tier];
				const bool bTierOnLadder = Tier < NumRailTiers;
				if (!bTierOnLadder)
				{
					TArray<FIntPoint> LoadedKeys;
					ArcTerrainTileAccess::GatherLoadedTileKeys(*Terrain, TileRail, Tier, LoadedKeys);
					for (const FIntPoint& TileKey : LoadedKeys)
					{
						if (!ArcTerrainTileAccess::ReleaseTileIfUnreferenced(*Terrain, TileRail, Tier, TileKey))
						{
							QueuePendingRelease(State, TileRail, Tier, TileKey);
							bArmedPendingRelease = true;
						}
					}
					ClearLane(Lane);
					continue;
				}

				double StreamRadiusCm = 0.0;
				double StreamUnloadRadiusCm = 0.0;
				if (!ResolveLaneRadii(TileRail, Tier, Ladder, WeightLadder, StreamRadiusCm,
					StreamUnloadRadiusCm))
				{
					ClearLane(Lane);
					continue;
				}

				FArcTerrainTileGrid Grid;
				if (!ArcTerrainTileAccess::ResolveGrid(*Terrain, TileRail, Tier, Grid))
				{
					ClearLane(Lane);
					continue;
				}

				Lane.bGridValid = true;
				Lane.TileSize = static_cast<double>(Grid.TileExtentM) * 100.0;
				Lane.StreamRadiusCm = StreamRadiusCm;
				Lane.StreamUnloadRadiusCm = StreamUnloadRadiusCm;

				bool bAnyClamped = false;
				CollectWindows(InvokerPositions, StreamRadiusCm, Lane.TileSize, TerrainTransform,
					Lane.InnerWindows, bAnyClamped);
				CollectWindows(InvokerPositions, StreamUnloadRadiusCm, Lane.TileSize, TerrainTransform,
					Lane.OuterWindows, bAnyClamped);
				if (bAnyClamped)
				{
					LogStreamWindowClamped(*Terrain, TileRail, Tier);
				}

				Lane.RequestedTiles.Reset();
				Lane.RetainedTiles.Reset();
				// Rectangular inner/outer windows are the authority; corner overfetch is intended.
				for (const FSectorWindow& Window : Lane.InnerWindows)
				{
					for (int32 KeyY = Window.MinY; KeyY <= Window.MaxY; ++KeyY)
					{
						for (int32 KeyX = Window.MinX; KeyX <= Window.MaxX; ++KeyX)
						{
							const FIntPoint TileKey(KeyX, KeyY);
							if (!ArcTerrainTileAccess::HasTile(*Terrain, TileRail, Tier, TileKey, &Grid))
							{
								continue;
							}
							Lane.RequestedTiles.Add(TileKey);
							Lane.RetainedTiles.Add(TileKey);
							if (ArcTerrainTileAccess::FindLoadedTile(*Terrain, TileRail, Tier, TileKey).IsValid())
							{
								State.PipelineTelemetry.Increment(
									&FArcTerrainPipelineCounters::StreamingSkippedAlreadyLoaded);
								continue;
							}
							State.PipelineTelemetry.Increment(
								&FArcTerrainPipelineCounters::StreamingAsyncRequests);
							ArcTerrainTileAccess::RequestTileAsync(*Terrain, TileRail, Tier, TileKey,
								[WeakThis, WeakTerrain](EArcTerrainRail LoadedRail, int32 LoadedTier,
									FIntPoint LoadedKey, FArcTerrainTilePayloadPtr Payload)
								{
									UArcTerrainSubsystem* Subsystem = WeakThis.Get();
									AArcTerrain* LoadedTerrain = WeakTerrain.Get();
									if (Subsystem != nullptr && LoadedTerrain != nullptr && Payload.IsValid())
									{
										if (FArcTerrainState* LoadedState = Subsystem->Terrains.Find(
											TWeakObjectPtr<AArcTerrain>(LoadedTerrain)))
										{
											LoadedState->PipelineTelemetry.Increment(
												&FArcTerrainPipelineCounters::StreamingValidCallbacks);
										}
										Subsystem->OnStreamedTileArrived(*LoadedTerrain, LoadedRail,
											LoadedTier, LoadedKey);
									}
								});
						}
					}
				}

				TArray<FIntPoint> LoadedKeys;
				ArcTerrainTileAccess::GatherLoadedTileKeys(*Terrain, TileRail, Tier, LoadedKeys);
				for (const FIntPoint& TileKey : LoadedKeys)
				{
					if (ContainsKey(Lane.OuterWindows, TileKey))
					{
						Lane.RetainedTiles.Add(TileKey);
						continue;
					}
					if (!ArcTerrainTileAccess::ReleaseTileIfUnreferenced(*Terrain, TileRail, Tier, TileKey))
					{
						QueuePendingRelease(State, TileRail, Tier, TileKey);
						bArmedPendingRelease = true;
					}
				}
			}
		}

		DropPendingKeysInRetainUnion(State);

		if (bArmedPendingRelease)
		{
			ArmStreamReleaseRetry(FPlatformTime::Seconds() + GStreamReleaseRetryBackoffSeconds);
		}

		ArcTerrainTileAccess::FlushManifest(*Terrain);
		State.StreamCache.ConsumedStreamLayoutRevision = State.StreamLayoutRevision;
		State.StreamCache.ConsumedDerivedGridRevision = State.DerivedGridRevision;
		State.StreamCache.ConsumedResidentSetRevision = State.ResidentSetRevision;
		State.StreamCache.ConsumedManifestCoverageEpoch = ArcTerrainTileAccess::GetManifestCoverageEpoch();
		State.StreamCache.ConsumedLoadedCoverageEpoch = ArcTerrainTileAccess::GetLoadedCoverageEpoch();
		State.StreamCache.bValid = true;
		State.StreamCache.bHadInvokers = true;
	}

	if (!InvokerPositions.IsEmpty()
		&& ArmedStreamReleaseRetryAt > 0.0
		&& FPlatformTime::Seconds() >= ArmedStreamReleaseRetryAt)
	{
		HandleStreamReleaseRetry();
	}
}

void UArcTerrainSubsystem::OnStreamedTileArrived(AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
	FIntPoint TileKey)
{
	const int32 RailIndex = static_cast<int32>(Rail);
	FArcTerrainState* State = Terrains.Find(TWeakObjectPtr<AArcTerrain>(&Terrain));
	if (State != nullptr)
	{
		State->PipelineTelemetry.Increment(&FArcTerrainPipelineCounters::StreamingArrivalEntries);
	}
	if (State != nullptr && State->StreamCache.bValid
		&& RailIndex >= 0 && RailIndex < ArcTerrain::NumTileRails
		&& ArcTerrainTileAccess::IsValidTier(Tier))
	{
		const FArcTerrainStreamLaneCache& Lane = State->StreamCache.Lanes[RailIndex][Tier];
		if (Lane.bGridValid
			&& !ArcTerrainStreamCacheLocal_b7e2::ContainsKey(Lane.OuterWindows, TileKey))
		{
			if (!ArcTerrainTileAccess::ReleaseTileIfUnreferenced(Terrain, Rail, Tier, TileKey))
			{
				ArcTerrainStreamCacheLocal_b7e2::QueuePendingRelease(*State, Rail, Tier, TileKey);
				ArmStreamReleaseRetry(FPlatformTime::Seconds()
					+ ArcTerrainStreamCacheLocal_b7e2::GStreamReleaseRetryBackoffSeconds);
			}
			return;
		}
	}

	// The data rail has no atlas slice — its masks are read straight from the payload — so an arrival
	// there marks nothing.
	if (State != nullptr)
	{
		FArcTerrainTileAuthorityEvent ArrivedEvent;
		ArrivedEvent.Rail = Rail;
		ArrivedEvent.Tier = Tier;
		ArrivedEvent.TileKey = TileKey;
		ArrivedEvent.Reason = EArcTerrainTileAuthorityReason::TileArrived;
		PublishLocalTileAuthorityEvent(*State, ArrivedEvent);
	}

	if (Rail == EArcTerrainRail::Data)
	{
		return;
	}

	if (Tier != 0)
	{
		return;
	}

	FArcTerrainTileGrid Grid;
	if (State == nullptr || !ArcTerrainTileAccess::ResolveGrid(Terrain, Rail, Tier, Grid))
	{
		return;
	}

	// Gated on the same condition as the atlas pass itself: with no render device nothing would ever drain
	// the marks while the stream keeps arriving. HEIGHT NEEDS NO MARK — the atlas pass compares each
	// sector's stored payload fingerprint against the tiles loaded now, which also covers the loads that
	// raise no arrival at all (the collision builder, the PCG samplers and the editor bakers read
	// synchronously).
	if (FApp::CanEverRender() && Rail == EArcTerrainRail::Weight)
	{
		const int32 MinCellX = TileKey.X * Grid.CellsPerTileEdge;
		const int32 MinCellY = TileKey.Y * Grid.CellsPerTileEdge;
		for (int32 OffsetY = 0; OffsetY < Grid.CellsPerTileEdge; ++OffsetY)
		{
			for (int32 OffsetX = 0; OffsetX < Grid.CellsPerTileEdge; ++OffsetX)
			{
				// Without this an already-admitted group never revisits the region, so the tile that just
				// arrived would sit in memory while the slice keeps drawing the composite — or the clear —
				// it replaces. Ground with no cell is marked in the state's own set instead.
				const FArcTerrainCellKey CellKey(MinCellX + OffsetX, MinCellY + OffsetY);
				if (FArcTerrainCellState* Cell = State->Cells.Find(CellKey))
				{
					Cell->bAtlasRefillPending = true;
				}
				else
				{
					State->PendingRefillCells.Add(CellKey);
				}
			}
		}
	}

	if (Rail != EArcTerrainRail::Height)
	{
		return;
	}

	// A HEIGHT TILE ARRIVING IS THE COOKED RECOVERY PATH, and the only one: the baked-branch latch gates
	// a blocking read, so a patch that latched before the bytes existed would never look again. Clearing
	// it over the tile's area lets exactly those patches re-attempt, once.
	const double PatchSize = Terrain.CollisionPatchSize();
	if (PatchSize <= 0.0)
	{
		return;
	}
	const double TileSize = static_cast<double>(Grid.TileExtentM) * 100.0;
	const FBox TileBounds(
		FVector(TileKey.X * TileSize, TileKey.Y * TileSize, -ArcTerrain::GCellZHalfExtent),
		FVector((TileKey.X + 1) * TileSize, (TileKey.Y + 1) * TileSize, ArcTerrain::GCellZHalfExtent));
	TArray<FArcTerrainCollisionKey> CoveredPatches;
	ArcTerrain::CollisionPatchesIntersectingLocalBounds(TileBounds, PatchSize, CoveredPatches);
	for (FArcTerrainCollisionKey PatchKey : CoveredPatches)
	{
		if (FArcTerrainCollisionPatch* Patch = State->CollisionPatches.Find(PatchKey))
		{
			Patch->bCollisionHeightSourceMissing = false;
		}
	}
}
