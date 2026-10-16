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
#include "Templates/Function.h"
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

namespace ArcTerrainSubsystemPrivate
{
	// One gathered mesh modifier of a cell, with its sort key snapshotted.
	struct FGatheredMeshModifier
	{
		UArcTerrainMeshModifier* Modifier = nullptr;
		FString PathName; // stable-across-sessions order (registration order is not)
	};

	void GatherSortedMeshModifiers(const FArcTerrainState& State, FArcTerrainCollisionKey Key,
		const FBox& CellBounds, TArray<UArcTerrainMeshModifier*>& OutModifiers)
	{
		TArray<FGatheredMeshModifier> Gathered;
		const TArray<TWeakObjectPtr<UArcTerrainMeshModifier>>* Indexed = State.CollisionMeshModifiers.Find(Key);
		if (Indexed != nullptr)
		{
			Gathered.Reserve(Indexed->Num());
			for (const TWeakObjectPtr<UArcTerrainMeshModifier>& WeakModifier : *Indexed)
			{
				UArcTerrainMeshModifier* Modifier = WeakModifier.Get();
				if (Modifier == nullptr || Modifier->bIsDisabled)
				{
					continue;
				}
				FBox ModifierBounds(ForceInit);
				const int32* StampIndex =
					State.StampByObject.Find(TObjectKey<UArcTerrainStampComponent>(Modifier));
				if (StampIndex != nullptr)
				{
					ModifierBounds = State.Stamps[*StampIndex].Bounds;
				}
				else
				{
					ModifierBounds = Modifier->ComputeBounds();
				}
				if (!ModifierBounds.IsValid || !ModifierBounds.Intersect(CellBounds))
				{
					continue;
				}
				FGatheredMeshModifier& GatheredModifier = Gathered.AddDefaulted_GetRef();
				GatheredModifier.Modifier = Modifier;
				GatheredModifier.PathName = Modifier->GetPathName();
			}
		}
		Gathered.Sort([](const FGatheredMeshModifier& A, const FGatheredMeshModifier& B) -> bool
		{
			return A.PathName < B.PathName;
		});

		OutModifiers.Reset(Gathered.Num());
		for (const FGatheredMeshModifier& GatheredModifier : Gathered)
		{
			OutModifiers.Add(GatheredModifier.Modifier);
		}
	}
	// Does Body already carry the modifier's MaterialOverride on every slot? Read against
	// OverrideMaterials — the array SetMaterial writes — so a cleared override (entries present, none
	// wanted) and a newly-set one (slots wanted, none present) are both detected. A mesh with zero
	// material slots matches either way, so it cannot churn a rebuild every tick.
	bool MeshModifierMaterialsMatch(const UStaticMeshComponent& Body, const UArcTerrainMeshModifier& Modifier)
	{
		if (Modifier.MaterialOverride == nullptr)
		{
			return Body.OverrideMaterials.Num() == 0;
		}
		if (Body.OverrideMaterials.Num() != Body.GetNumMaterials())
		{
			return false;
		}
		for (const TObjectPtr<UMaterialInterface>& Slot : Body.OverrideMaterials)
		{
			if (Slot != Modifier.MaterialOverride)
			{
				return false;
			}
		}
		return true;
	}
	int32 ResolveCollisionPatchQuads(const AArcTerrain& Terrain)
	{
		return FMath::Max(Terrain.CollisionPatchQuads, 8);
	}
#if WITH_EDITOR
	void GatherCollisionHeightStamps(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FBox& PatchBounds, TArray<UArcTerrainStampComponent*>& OutStamps)
	{
		struct FGatheredStamp
		{
			UArcTerrainStampComponent* Stamp = nullptr;
			int32 Priority = 0;
			FString PathName;
		};

		TArray<FArcTerrainBakeTileRef> TileRefs;
		GatherBakeTileRefsForBounds(Terrain, State, PatchBounds, TileRefs);
		TSet<UArcTerrainStampComponent*> UniqueStamps;
		for (const FArcTerrainBakeTileRef& Ref : TileRefs)
		{
			if (Ref.Rail != EArcTerrainRail::Height || Ref.Tier != 0)
			{
				continue;
			}
			const TArray<TWeakObjectPtr<UArcTerrainStampComponent>>* Contributors = State.Contributors.Find(Ref);
			if (Contributors == nullptr)
			{
				continue;
			}
			for (const TWeakObjectPtr<UArcTerrainStampComponent>& WeakStamp : *Contributors)
			{
				UArcTerrainStampComponent* Stamp = WeakStamp.Get();
				if (Stamp == nullptr || Stamp->bIsDisabled || UniqueStamps.Contains(Stamp))
				{
					continue;
				}
				FBox StampBounds(ForceInit);
				const int32* StampIndex = State.StampByObject.Find(TObjectKey<UArcTerrainStampComponent>(Stamp));
				if (StampIndex != nullptr)
				{
					StampBounds = State.Stamps[*StampIndex].Bounds;
				}
				else
				{
					StampBounds = Stamp->ComputeBounds();
				}
				if (!StampBounds.IsValid || !StampBounds.Intersect(PatchBounds))
				{
					continue;
				}
				UniqueStamps.Add(Stamp);
			}
		}

		TArray<FGatheredStamp> Gathered;
		Gathered.Reserve(UniqueStamps.Num());
		for (UArcTerrainStampComponent* Stamp : UniqueStamps)
		{
			FGatheredStamp& GatheredStamp = Gathered.AddDefaulted_GetRef();
			GatheredStamp.Stamp = Stamp;
			GatheredStamp.Priority = Stamp->Priority;
			GatheredStamp.PathName = Stamp->GetPathName();
		}
		Gathered.Sort([](const FGatheredStamp& A, const FGatheredStamp& B)
		{
			if (A.Priority != B.Priority)
			{
				return A.Priority < B.Priority;
			}
			return A.PathName < B.PathName;
		});
		OutStamps.Reset(Gathered.Num());
		for (const FGatheredStamp& GatheredStamp : Gathered)
		{
			OutStamps.Add(GatheredStamp.Stamp);
		}
	}

	// Collision recipe key of a patch, memoised per DirtySerial so the Blake3 field walk (GatherSortedStamps
	// plus the whole stamp/layer walk) runs at most once per dirtying edit and not every tick — and there
	// are 16 patches to a cell at the defaults.
	//
	// THE MEMO KEY IS THE DIRTY SERIAL AND MAY NEVER BE AppliedCollisionKeyHash. The hash only moves after
	// a build is applied, a build only runs when the key differs from it, and the key would come from this
	// memo: a stamp dragged over a built patch would then invalidate nothing at all, and its collision
	// would freeze on pre-edit ground precisely where the editing is happening.
	const FIoHash& GetOrComputeCollisionKey(FArcTerrainCollisionPatch& Patch,
		const FArcTerrainConfig& Config, float BaseHeightCm, FArcTerrainCollisionKey Key,
		const FTransform& TerrainTransform, TConstArrayView<UArcTerrainStampComponent*> SortedStamps)
	{
		if (Patch.CachedCollisionKeySerial != Patch.DirtySerial)
		{
			Patch.CachedCollisionKey = ArcTerrainCollision::ComputeCellCollisionKey(
				Config, BaseHeightCm, Key, TerrainTransform, SortedStamps);
			Patch.CachedCollisionKeySerial = Patch.DirtySerial;
		}
		return Patch.CachedCollisionKey;
	}
#endif // WITH_EDITOR
}

namespace ArcTerrainCollisionIndex_e7b2
{
	bool WindowContains(const ArcTerrainSubsystemPrivate::FSectorWindow& Window, FIntPoint Key)
	{
		return Key.X >= Window.MinX && Key.X <= Window.MaxX
			&& Key.Y >= Window.MinY && Key.Y <= Window.MaxY;
	}

	void AppendWindowKeys(const ArcTerrainSubsystemPrivate::FSectorWindow& Window, TSet<FIntPoint>& OutKeys)
	{
		if (Window.MaxX < Window.MinX || Window.MaxY < Window.MinY)
		{
			return;
		}
		for (int32 KeyY = Window.MinY; KeyY <= Window.MaxY; ++KeyY)
		{
			for (int32 KeyX = Window.MinX; KeyX <= Window.MaxX; ++KeyX)
			{
				OutKeys.Add(FIntPoint(KeyX, KeyY));
			}
		}
	}

	void DiffWindows(const ArcTerrainSubsystemPrivate::FSectorWindow& OldWindow,
		const ArcTerrainSubsystemPrivate::FSectorWindow& NewWindow, TArray<FIntPoint>& OutEnter,
		TArray<FIntPoint>& OutLeave)
	{
		TSet<FIntPoint> OldKeys;
		TSet<FIntPoint> NewKeys;
		AppendWindowKeys(OldWindow, OldKeys);
		AppendWindowKeys(NewWindow, NewKeys);
		for (const FIntPoint& Key : NewKeys)
		{
			if (!OldKeys.Contains(Key))
			{
				OutEnter.Add(Key);
			}
		}
		for (const FIntPoint& Key : OldKeys)
		{
			if (!NewKeys.Contains(Key))
			{
				OutLeave.Add(Key);
			}
		}
	}

	FIntPoint IntegerCentreOf(const FVector& WorldInvoker, double PatchSize, const FTransform& TerrainTransform)
	{
		const FVector Local = TerrainTransform.InverseTransformPosition(WorldInvoker);
		return FIntPoint(
			FMath::FloorToInt32(Local.X / PatchSize),
			FMath::FloorToInt32(Local.Y / PatchSize));
	}

	int32 Chebyshev(FIntPoint A, FIntPoint B)
	{
		return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
	}

	bool CentresMatch(TConstArrayView<FIntPoint> A, TConstArrayView<FIntPoint> B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index] != B[Index])
			{
				return false;
			}
		}
		return true;
	}

	double ReadyDistSq(const FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainCollisionKey Key)
	{
		return ArcTerrainSubsystemPrivate::PatchInvokerDistSq(
			Key, Terrain.CollisionPatchSize(), Terrain.GetTerrainTransform(),
			State.CollisionSelectionCache.QuantizedPositions);
	}

	bool KeyCoveredByOuters(TConstArrayView<FArcTerrainCachedInvokerWindow> Windows, FIntPoint Key)
	{
		for (const FArcTerrainCachedInvokerWindow& Window : Windows)
		{
			if (WindowContains(Window.OuterWindow, Key))
			{
				return true;
			}
		}
		return false;
	}

	void RemoveReady(FArcTerrainState& State, FArcTerrainCollisionKey Key)
	{
		if (State.CollisionReadySet.Remove(Key) > 0)
		{
			State.CollisionReadyOrdered.RemoveAll(
				[Key](const FArcTerrainCollisionReadyItem& Item)
				{
					return Item.Key == Key;
				});
		}
	}

	void RebuildStampIndex(FArcTerrainState& State)
	{
		State.StampByObject.Reset();
		State.StampByObject.Reserve(State.Stamps.Num());
		for (int32 Index = 0; Index < State.Stamps.Num(); ++Index)
		{
			if (UArcTerrainStampComponent* Stamp = State.Stamps[Index].Stamp.Get())
			{
				State.StampByObject.Add(TObjectKey<UArcTerrainStampComponent>(Stamp), Index);
			}
		}
	}

	void NoteStampInserted(FArcTerrainState& State, UArcTerrainStampComponent& Stamp, int32 StampIndex)
	{
		State.StampByObject.Add(TObjectKey<UArcTerrainStampComponent>(&Stamp), StampIndex);
	}

	void RemoveStampCollisionCoverage(FArcTerrainState& State, FArcTerrainStampEntry& Entry)
	{
		UArcTerrainMeshModifier* Modifier = Cast<UArcTerrainMeshModifier>(Entry.Stamp.Get());
		if (Modifier != nullptr)
		{
			for (const FArcTerrainCollisionKey& Key : Entry.CollisionCoveredPatches)
			{
				TArray<TWeakObjectPtr<UArcTerrainMeshModifier>>* List = State.CollisionMeshModifiers.Find(Key);
				if (List == nullptr)
				{
					continue;
				}
				List->RemoveAll(
					[Modifier](const TWeakObjectPtr<UArcTerrainMeshModifier>& Weak)
					{
						const UArcTerrainMeshModifier* Existing = Weak.Get();
						return Existing == nullptr || Existing == Modifier;
					});
				if (List->IsEmpty())
				{
					State.CollisionMeshModifiers.Remove(Key);
				}
			}
		}
		Entry.CollisionCoveredPatches.Reset();
	}

	void UpdateStampCollisionCoverage(const AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainStampEntry& Entry)
	{
		RemoveStampCollisionCoverage(State, Entry);
		if (!Entry.Bounds.IsValid)
		{
			return;
		}
		TArray<FArcTerrainCollisionKey> Keys;
		ArcTerrain::CollisionPatchesIntersectingLocalBounds(Entry.Bounds, Terrain.CollisionPatchSize(), Keys);
		Entry.CollisionCoveredPatches = Keys;
		UArcTerrainMeshModifier* Modifier = Cast<UArcTerrainMeshModifier>(Entry.Stamp.Get());
		if (Modifier == nullptr)
		{
			return;
		}
		for (const FArcTerrainCollisionKey& Key : Keys)
		{
			TArray<TWeakObjectPtr<UArcTerrainMeshModifier>>& List = State.CollisionMeshModifiers.FindOrAdd(Key);
			const bool bAlreadyPresent = List.ContainsByPredicate(
				[Modifier](const TWeakObjectPtr<UArcTerrainMeshModifier>& Weak)
				{
					return Weak.Get() == Modifier;
				});
			if (!bAlreadyPresent)
			{
				List.Add(Modifier);
			}
			if (State.CollisionWindowUnion.Contains(Key) || State.CollisionBootstrapKeys.Contains(Key))
			{
				EnqueueReady(State, Terrain, Key);
			}
		}
	}

	void RebuildCollisionMeshIndex(const AArcTerrain& Terrain, FArcTerrainState& State)
	{
		State.CollisionMeshModifiers.Reset();
		for (FArcTerrainStampEntry& Entry : State.Stamps)
		{
			Entry.CollisionCoveredPatches.Reset();
			UpdateStampCollisionCoverage(Terrain, State, Entry);
		}
	}

	void EnqueueReady(FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainCollisionKey Key)
	{
		const FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Key);
		if (Patch != nullptr && Patch->bCollisionHeightSourceMissing)
		{
			State.PipelineTelemetry.IncrementCollisionEnqueue(
				EArcTerrainPipelineCollisionEnqueue::HeightSourceMissing);
			return;
		}
		if (State.CollisionReadySet.Contains(Key))
		{
			State.PipelineTelemetry.IncrementCollisionEnqueue(
				EArcTerrainPipelineCollisionEnqueue::AlreadyQueued);
			return;
		}
		FArcTerrainCollisionReadyItem Item;
		Item.Key = Key;
		Item.DistSq = ReadyDistSq(State, Terrain, Key);
		State.CollisionReadySet.Add(Key);
		int32 InsertIndex = State.CollisionReadyOrdered.Num();
		while (InsertIndex > 0 && Item.DistSq < State.CollisionReadyOrdered[InsertIndex - 1].DistSq)
		{
			--InsertIndex;
		}
		State.CollisionReadyOrdered.Insert(Item, InsertIndex);
		State.PipelineTelemetry.IncrementCollisionEnqueue(EArcTerrainPipelineCollisionEnqueue::Enqueued);
	}

	FArcTerrainCollisionPatch& EnsurePatch(FArcTerrainState& State, FArcTerrainCollisionKey Key)
	{
		if (FArcTerrainCollisionPatch* Existing = State.CollisionPatches.Find(Key))
		{
			return *Existing;
		}
		FArcTerrainCollisionPatch& Patch = State.CollisionPatches.Add(Key);
		Patch.PatchGeneration = State.NextCollisionPatchGeneration++;
		return Patch;
	}

	void NoteBootstrapKey(FArcTerrainState& State, FArcTerrainCollisionKey Key)
	{
		State.CollisionBootstrapKeys.Add(Key);
		State.bCollisionBootstrapActive = true;
	}

	void ClearScheduling(FArcTerrainState& State)
	{
		State.CollisionReadySet.Reset();
		State.CollisionReadyOrdered.Reset();
		State.CollisionRetryHeap.Reset();
		State.PendingCollisionAuthorityEvents.Reset();
		State.CollisionWindowUnion.Reset();
		State.CollisionBootstrapKeys.Reset();
		State.CollisionSelectionCache = FArcTerrainCollisionSelectionCache();
		State.NextCollisionRetry = 0.0;
		State.bCollisionBootstrapActive = false;
		State.bCollisionRanksDirty = false;
	}

	void ForgetPatch(FArcTerrainState& State, FArcTerrainCollisionKey Key)
	{
		RemoveReady(State, Key);
		State.CollisionPatches.Remove(Key);
	}

	void PushRetry(FArcTerrainState& State, FArcTerrainCollisionKey Key, double RetryAfter)
	{
		FArcTerrainCollisionRetryItem Item;
		Item.RetryAfter = RetryAfter;
		Item.Key = Key;
		State.CollisionRetryHeap.HeapPush(Item,
			[](const FArcTerrainCollisionRetryItem& A, const FArcTerrainCollisionRetryItem& B)
			{
				return A.RetryAfter < B.RetryAfter;
			});
		if (State.NextCollisionRetry <= 0.0 || RetryAfter < State.NextCollisionRetry)
		{
			State.NextCollisionRetry = RetryAfter;
		}
	}

	void RebuildRanks(FArcTerrainState& State, const AArcTerrain& Terrain)
	{
		for (FArcTerrainCollisionReadyItem& Item : State.CollisionReadyOrdered)
		{
			Item.DistSq = ReadyDistSq(State, Terrain, Item.Key);
		}
		State.CollisionReadyOrdered.Sort(
			[](const FArcTerrainCollisionReadyItem& A, const FArcTerrainCollisionReadyItem& B)
			{
				return A.DistSq < B.DistSq;
			});
		State.bCollisionRanksDirty = false;
	}

	void RebuildInvokerCache(TConstArrayView<FVector> InvokerPositions, double PatchSize,
		const FTransform& TerrainTransform, double CollisionRadius, double CollisionUnloadRadius,
		FArcTerrainCollisionSelectionCache& Cache)
	{
		using namespace ArcTerrainSubsystemPrivate;
		Cache.Invokers.Reset();
		Cache.QuantizedPositions.Reset();
		Cache.Invokers.Reserve(InvokerPositions.Num());
		Cache.QuantizedPositions.Reserve(InvokerPositions.Num());
		TArray<TPair<FIntPoint, int32>> Sorted;
		Sorted.Reserve(InvokerPositions.Num());
		for (int32 Index = 0; Index < InvokerPositions.Num(); ++Index)
		{
			Sorted.Emplace(IntegerCentreOf(InvokerPositions[Index], PatchSize, TerrainTransform), Index);
		}
		Sorted.Sort([](const TPair<FIntPoint, int32>& A, const TPair<FIntPoint, int32>& B)
		{
			if (A.Key.X != B.Key.X)
			{
				return A.Key.X < B.Key.X;
			}
			return A.Key.Y < B.Key.Y;
		});
		for (const TPair<FIntPoint, int32>& Pair : Sorted)
		{
			FArcTerrainCachedInvokerWindow Window;
			Window.IntegerCentre = Pair.Key;
			Window.InnerWindow = ComputeSectorWindow(InvokerPositions[Pair.Value], CollisionRadius, 0,
				PatchSize, TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			Window.OuterWindow = ComputeSectorWindow(InvokerPositions[Pair.Value], CollisionUnloadRadius, 0,
				PatchSize, TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			Cache.Invokers.Add(Window);
			Cache.QuantizedPositions.Add(InvokerPositions[Pair.Value]);
		}
		Cache.CollisionRadius = CollisionRadius;
		Cache.CollisionUnloadRadius = CollisionUnloadRadius;
		Cache.PatchSize = PatchSize;
	}

	FBox TileLocalBounds(const FArcTerrainTileGrid& Grid, FIntPoint TileKey)
	{
		const double TileSize = static_cast<double>(Grid.TileExtentM) * 100.0;
		return FBox(
			FVector(static_cast<double>(TileKey.X) * TileSize, static_cast<double>(TileKey.Y) * TileSize,
				-ArcTerrain::GCellZHalfExtent),
			FVector(static_cast<double>(TileKey.X + 1) * TileSize, static_cast<double>(TileKey.Y + 1) * TileSize,
				ArcTerrain::GCellZHalfExtent));
	}
}

void UArcTerrainSubsystem::TickCollision(
	TConstArrayView<FVector> InvokerPositions,
	bool bInSyncWindow,
	const UArcTerrainSettings& Settings,
	int32& OutNumCollisionBuilds,
	int32& OutNumCollisionAttempts)
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainCollisionIndex_e7b2;

	const int32 MaxCollisionBuildsPerFrame = FMath::Max(Settings.MaxCompositesPerFrame, 1);
	const double Now = FPlatformTime::Seconds();
	OutNumCollisionBuilds = 0;
	OutNumCollisionAttempts = 0;

#if WITH_EDITOR
	const bool bHasCompletion =
		CollisionMailbox.IsValid()
		&& (CollisionMailbox->Pending.load(std::memory_order_acquire) > 0
			|| CollisionMailbox->bOverflowWake.load(std::memory_order_acquire));
#else
	const bool bHasCompletion = false;
#endif

	bool bHasBoundaryWork = false;
	bool bHasProducerWork = false;
	bool bHasDueRetry = false;
	bool bHasReadyWork = false;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			continue;
		}
		FArcTerrainState& State = TerrainPair.Value;
		if (State.CollisionTerrainId == 0)
		{
			State.CollisionTerrainId = NextCollisionTerrainId++;
			State.PipelineTelemetry.ConsumedCollisionCountSaturationSerial = CollisionCountSaturationSerial;
			State.PipelineTelemetry.ConsumedCollisionDeadlineSaturationSerial =
				CollisionDeadlineSaturationSerial;
		}
		AttributeCollisionSaturation(State);
		const FResolvedResidency CollisionResidency = ResolveResidency(*Terrain);
		const double PatchSize = Terrain->CollisionPatchSize();
		TArray<FIntPoint> NewCentres;
		NewCentres.Reserve(InvokerPositions.Num());
		if (PatchSize > 0.0)
		{
			const FTransform TerrainTransform = Terrain->GetTerrainTransform();
			for (const FVector& Invoker : InvokerPositions)
			{
				NewCentres.Add(IntegerCentreOf(Invoker, PatchSize, TerrainTransform));
			}
			NewCentres.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				if (A.X != B.X)
				{
					return A.X < B.X;
				}
				return A.Y < B.Y;
			});
		}
		TArray<FIntPoint> OldCentres;
		OldCentres.Reserve(State.CollisionSelectionCache.Invokers.Num());
		for (const FArcTerrainCachedInvokerWindow& Cached : State.CollisionSelectionCache.Invokers)
		{
			OldCentres.Add(Cached.IntegerCentre);
		}
		const bool bWindowUnchanged = CentresMatch(OldCentres, NewCentres)
			&& State.CollisionSelectionCache.CollisionRadius == CollisionResidency.CollisionRadius
			&& State.CollisionSelectionCache.CollisionUnloadRadius == CollisionResidency.CollisionUnloadRadius
			&& State.CollisionSelectionCache.PatchSize == PatchSize;
		const bool bBootstrapReconcile = State.bCollisionBootstrapActive && !bInSyncWindow;
		if (!bWindowUnchanged || bBootstrapReconcile)
		{
			bHasBoundaryWork = true;
		}
		if (State.PendingCollisionAuthorityEvents.Num() > 0)
		{
			bHasProducerWork = true;
		}
		if (State.NextCollisionRetry > 0.0 && Now >= State.NextCollisionRetry)
		{
			bHasDueRetry = true;
		}
		if (State.CollisionReadySet.Num() > 0)
		{
			bHasReadyWork = true;
		}
	}

	if (!bHasBoundaryWork && !bHasProducerWork && !bHasDueRetry && !bHasReadyWork && !bHasCompletion)
	{
		return;
	}

	TFunction<void(AArcTerrain&, FArcTerrainState&, FArcTerrainCollisionKey, double, double, bool)> VisitPatch =
		[this, bInSyncWindow](AArcTerrain& Terrain, FArcTerrainState& State,
			FArcTerrainCollisionKey Key, double CollisionRadius, double CollisionUnloadRadius,
			bool bCreate)
	{
		FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Key);
		if (Patch == nullptr)
		{
			if (!bCreate)
			{
				return;
			}
			Patch = &EnsurePatch(State, Key);
		}
		const double DistSq = ReadyDistSq(State, Terrain, Key);
		const bool bWantsBuild = UpdateCollisionPatch(Terrain, State, Key, *Patch, DistSq,
			CollisionRadius, CollisionUnloadRadius, bInSyncWindow);
		if (bWantsBuild)
		{
			EnqueueReady(State, Terrain, Key);
			return;
		}
		if (!Patch->CollisionComponent.IsValid()
			&& Patch->MeshModifierCollision.IsEmpty()
			&& !Patch->bCollisionHeightSourceMissing
			&& !State.CollisionBootstrapKeys.Contains(Key))
		{
			ForgetPatch(State, Key);
		}
	};

	TFunction<void(AArcTerrain&, FArcTerrainState&, FArcTerrainCollisionKey)> LeaveKey =
		[this, bInSyncWindow](AArcTerrain& Terrain, FArcTerrainState& State, FArcTerrainCollisionKey Key)
	{
		if (bInSyncWindow && State.CollisionBootstrapKeys.Contains(Key))
		{
			return;
		}
		if (KeyCoveredByOuters(State.CollisionSelectionCache.Invokers, Key))
		{
			return;
		}
		if (FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Key))
		{
			DestroyPatchCollision(*Patch);
		}
		ForgetPatch(State, Key);
	};

	TFunction<void(AArcTerrain&, FArcTerrainState&, TConstArrayView<FIntPoint>, double, double)> EnterInnerKeys =
		[&VisitPatch](AArcTerrain& Terrain, FArcTerrainState& State, TConstArrayView<FIntPoint> Keys,
			double CollisionRadius, double CollisionUnloadRadius)
	{
		for (const FIntPoint& Key : Keys)
		{
			State.CollisionWindowUnion.Add(Key);
			VisitPatch(Terrain, State, Key, CollisionRadius, CollisionUnloadRadius, true);
		}
	};

	TFunction<void(AArcTerrain&, FArcTerrainState&, const FArcTerrainCachedInvokerWindow&, double, double)> AddWindowMembership =
		[&VisitPatch](AArcTerrain& Terrain, FArcTerrainState& State, const FArcTerrainCachedInvokerWindow& Window,
			double CollisionRadius, double CollisionUnloadRadius)
	{
		TSet<FIntPoint> InnerKeys;
		TSet<FIntPoint> OuterKeys;
		AppendWindowKeys(Window.InnerWindow, InnerKeys);
		AppendWindowKeys(Window.OuterWindow, OuterKeys);
		for (const FIntPoint& Key : OuterKeys)
		{
			State.CollisionWindowUnion.Add(Key);
		}
		for (const FIntPoint& Key : InnerKeys)
		{
			VisitPatch(Terrain, State, Key, CollisionRadius, CollisionUnloadRadius, true);
		}
	};

	TFunction<void(AArcTerrain&, FArcTerrainState&, const FArcTerrainCachedInvokerWindow&)> RemoveWindowMembership =
		[&LeaveKey](AArcTerrain& Terrain, FArcTerrainState& State, const FArcTerrainCachedInvokerWindow& Window)
	{
		TSet<FIntPoint> OuterKeys;
		AppendWindowKeys(Window.OuterWindow, OuterKeys);
		for (const FIntPoint& Key : OuterKeys)
		{
			if (KeyCoveredByOuters(State.CollisionSelectionCache.Invokers, Key))
			{
				continue;
			}
			State.CollisionWindowUnion.Remove(Key);
			LeaveKey(Terrain, State, Key);
		}
	};

#if WITH_EDITOR
	TFunction<void(const ArcTerrainCollision::FArcTerrainCollisionCompletionToken&)> ApplyCompletionToken =
		[this](const ArcTerrainCollision::FArcTerrainCollisionCompletionToken& Token)
	{
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			AArcTerrain* Terrain = TerrainPair.Key.Get();
			if (Terrain == nullptr || TerrainPair.Value.CollisionTerrainId != Token.TerrainId)
			{
				continue;
			}
			FArcTerrainState& State = TerrainPair.Value;
			FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Token.PatchKey);
			if (Patch == nullptr
				|| Patch->PatchGeneration != Token.PatchGeneration
				|| Patch->DirtySerial != Token.DirtySerial
				|| Patch->CollisionProbeKey != Token.CollisionKey)
			{
				return;
			}
			if (!Patch->CollisionProbe.IsValid() || !ArcTerrainCollision::IsGetComplete(Patch->CollisionProbe))
			{
				EnqueueReady(State, *Terrain, Token.PatchKey);
				return;
			}
			EnqueueReady(State, *Terrain, Token.PatchKey);
			return;
		}
	};

	if (CollisionMailbox.IsValid())
	{
		int32 DrainBudget = MaxCollisionBuildsPerFrame;
		ArcTerrainCollision::FArcTerrainCollisionCompletionToken Token;
		while (DrainBudget > 0 && CollisionMailbox->Queue.Dequeue(Token))
		{
			CollisionMailbox->Pending.fetch_sub(1, std::memory_order_acq_rel);
			ApplyCompletionToken(Token);
			--DrainBudget;
		}
		const bool bOverflow = CollisionMailbox->bOverflowWake.exchange(false, std::memory_order_acq_rel);
		if (bOverflow)
		{
			for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
			{
				AArcTerrain* Terrain = TerrainPair.Key.Get();
				if (Terrain == nullptr)
				{
					continue;
				}
				for (TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair :
					TerrainPair.Value.CollisionPatches)
				{
					if (PatchPair.Value.CollisionProbe.IsValid()
						&& ArcTerrainCollision::IsGetComplete(PatchPair.Value.CollisionProbe))
					{
						EnqueueReady(TerrainPair.Value, *Terrain, PatchPair.Key);
					}
				}
			}
		}
	}
#endif

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			continue;
		}
		FArcTerrainState& State = TerrainPair.Value;
		const FResolvedResidency CollisionResidency = ResolveResidency(*Terrain);
		const double PatchSize = Terrain->CollisionPatchSize();
		const FTransform TerrainTransform = Terrain->GetTerrainTransform();

		TArray<FIntPoint> NewCentres;
		NewCentres.Reserve(InvokerPositions.Num());
		if (PatchSize > 0.0)
		{
			for (const FVector& Invoker : InvokerPositions)
			{
				NewCentres.Add(IntegerCentreOf(Invoker, PatchSize, TerrainTransform));
			}
			NewCentres.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				if (A.X != B.X)
				{
					return A.X < B.X;
				}
				return A.Y < B.Y;
			});
		}
		TArray<FIntPoint> OldCentres;
		OldCentres.Reserve(State.CollisionSelectionCache.Invokers.Num());
		for (const FArcTerrainCachedInvokerWindow& Cached : State.CollisionSelectionCache.Invokers)
		{
			OldCentres.Add(Cached.IntegerCentre);
		}
		const bool bWindowUnchanged = CentresMatch(OldCentres, NewCentres)
			&& State.CollisionSelectionCache.CollisionRadius == CollisionResidency.CollisionRadius
			&& State.CollisionSelectionCache.CollisionUnloadRadius == CollisionResidency.CollisionUnloadRadius
			&& State.CollisionSelectionCache.PatchSize == PatchSize;

		if (!bWindowUnchanged && PatchSize > 0.0)
		{
			FArcTerrainCollisionSelectionCache PreviousCache = State.CollisionSelectionCache;
			RebuildInvokerCache(InvokerPositions, PatchSize, TerrainTransform,
				CollisionResidency.CollisionRadius, CollisionResidency.CollisionUnloadRadius,
				State.CollisionSelectionCache);
			const bool bFullRebuild = PreviousCache.Invokers.IsEmpty()
				|| PreviousCache.CollisionRadius != CollisionResidency.CollisionRadius
				|| PreviousCache.CollisionUnloadRadius != CollisionResidency.CollisionUnloadRadius
				|| PreviousCache.PatchSize != PatchSize;
			if (bFullRebuild)
			{
				TSet<FIntPoint> PreviousUnion = State.CollisionWindowUnion;
				State.CollisionWindowUnion.Reset();
				for (const FArcTerrainCachedInvokerWindow& Window : State.CollisionSelectionCache.Invokers)
				{
					AddWindowMembership(*Terrain, State, Window, CollisionResidency.CollisionRadius,
						CollisionResidency.CollisionUnloadRadius);
				}
				for (const FIntPoint& OldKey : PreviousUnion)
				{
					if (!State.CollisionWindowUnion.Contains(OldKey))
					{
						LeaveKey(*Terrain, State, OldKey);
					}
				}
			}
			else
			{
				TArray<int32> OldUsed;
				OldUsed.Init(0, PreviousCache.Invokers.Num());
				TArray<int32> NewUsed;
				NewUsed.Init(0, State.CollisionSelectionCache.Invokers.Num());
				for (int32 NewIndex = 0; NewIndex < State.CollisionSelectionCache.Invokers.Num(); ++NewIndex)
				{
					for (int32 OldIndex = 0; OldIndex < PreviousCache.Invokers.Num(); ++OldIndex)
					{
						if (OldUsed[OldIndex] != 0)
						{
							continue;
						}
						if (PreviousCache.Invokers[OldIndex].IntegerCentre
							== State.CollisionSelectionCache.Invokers[NewIndex].IntegerCentre)
						{
							OldUsed[OldIndex] = 1;
							NewUsed[NewIndex] = 1;
							break;
						}
					}
				}
				for (int32 NewIndex = 0; NewIndex < State.CollisionSelectionCache.Invokers.Num(); ++NewIndex)
				{
					if (NewUsed[NewIndex] != 0)
					{
						continue;
					}
					int32 AdjacentOld = INDEX_NONE;
					int32 TeleportOld = INDEX_NONE;
					for (int32 OldIndex = 0; OldIndex < PreviousCache.Invokers.Num(); ++OldIndex)
					{
						if (OldUsed[OldIndex] != 0)
						{
							continue;
						}
						const int32 Distance = Chebyshev(PreviousCache.Invokers[OldIndex].IntegerCentre,
							State.CollisionSelectionCache.Invokers[NewIndex].IntegerCentre);
						if (Distance == 1 && AdjacentOld == INDEX_NONE)
						{
							AdjacentOld = OldIndex;
						}
						else if (Distance > 1 && TeleportOld == INDEX_NONE)
						{
							TeleportOld = OldIndex;
						}
					}
					if (AdjacentOld != INDEX_NONE)
					{
						OldUsed[AdjacentOld] = 1;
						NewUsed[NewIndex] = 1;
						TArray<FIntPoint> EnterKeys;
						TArray<FIntPoint> InnerLeaveKeys;
						DiffWindows(PreviousCache.Invokers[AdjacentOld].InnerWindow,
							State.CollisionSelectionCache.Invokers[NewIndex].InnerWindow, EnterKeys,
							InnerLeaveKeys);
						(void)InnerLeaveKeys;
						TArray<FIntPoint> OuterEnter;
						TArray<FIntPoint> OuterLeave;
						DiffWindows(PreviousCache.Invokers[AdjacentOld].OuterWindow,
							State.CollisionSelectionCache.Invokers[NewIndex].OuterWindow, OuterEnter, OuterLeave);
						for (const FIntPoint& Key : OuterEnter)
						{
							State.CollisionWindowUnion.Add(Key);
						}
						EnterInnerKeys(*Terrain, State, EnterKeys, CollisionResidency.CollisionRadius,
							CollisionResidency.CollisionUnloadRadius);
						for (const FIntPoint& Key : OuterLeave)
						{
							if (KeyCoveredByOuters(State.CollisionSelectionCache.Invokers, Key))
							{
								continue;
							}
							State.CollisionWindowUnion.Remove(Key);
							LeaveKey(*Terrain, State, Key);
						}
					}
					else if (TeleportOld != INDEX_NONE)
					{
						OldUsed[TeleportOld] = 1;
						NewUsed[NewIndex] = 1;
						RemoveWindowMembership(*Terrain, State, PreviousCache.Invokers[TeleportOld]);
						AddWindowMembership(*Terrain, State, State.CollisionSelectionCache.Invokers[NewIndex],
							CollisionResidency.CollisionRadius, CollisionResidency.CollisionUnloadRadius);
					}
					else
					{
						NewUsed[NewIndex] = 1;
						AddWindowMembership(*Terrain, State, State.CollisionSelectionCache.Invokers[NewIndex],
							CollisionResidency.CollisionRadius, CollisionResidency.CollisionUnloadRadius);
					}
				}
				for (int32 OldIndex = 0; OldIndex < PreviousCache.Invokers.Num(); ++OldIndex)
				{
					if (OldUsed[OldIndex] != 0)
					{
						continue;
					}
					RemoveWindowMembership(*Terrain, State, PreviousCache.Invokers[OldIndex]);
				}
			}
			State.bCollisionRanksDirty = true;
		}

		if (State.bCollisionBootstrapActive && !bInSyncWindow)
		{
			TArray<FArcTerrainCollisionKey> ObsoleteBootstrap;
			for (const FArcTerrainCollisionKey& Key : State.CollisionBootstrapKeys)
			{
				if (!State.CollisionWindowUnion.Contains(Key))
				{
					ObsoleteBootstrap.Add(Key);
				}
			}
			for (const FArcTerrainCollisionKey& Key : ObsoleteBootstrap)
			{
				if (FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Key))
				{
					DestroyPatchCollision(*Patch);
				}
				ForgetPatch(State, Key);
			}
			State.CollisionBootstrapKeys.Reset();
			State.bCollisionBootstrapActive = false;
		}

		if (State.PendingCollisionAuthorityEvents.Num() > 0)
		{
			TArray<FArcTerrainTileAuthorityEvent> Events = MoveTemp(State.PendingCollisionAuthorityEvents);
			State.PendingCollisionAuthorityEvents.Reset();
			for (const FArcTerrainTileAuthorityEvent& Event : Events)
			{
				if (Event.Reason == EArcTerrainTileAuthorityReason::LayoutChanged
					|| Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightReset)
				{
					for (const FArcTerrainCollisionKey& Key : State.CollisionWindowUnion)
					{
						if (FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Key))
						{
							Patch->bCollisionHeightSourceMissing = false;
						}
						EnqueueReady(State, *Terrain, Key);
					}
					continue;
				}
				if (Event.Rail != EArcTerrainRail::Height)
				{
					continue;
				}
				FArcTerrainTileGrid Grid;
				if (!ArcTerrainTileAccess::ResolveGrid(*Terrain, Event.Rail, Event.Tier, Grid) || !Grid.IsValid())
				{
					continue;
				}
				TArray<FArcTerrainCollisionKey> Covered;
				ArcTerrain::CollisionPatchesIntersectingLocalBounds(TileLocalBounds(Grid, Event.TileKey),
					PatchSize, Covered);
				for (const FArcTerrainCollisionKey& Key : Covered)
				{
					if (FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Key))
					{
						if (Event.Rail == EArcTerrainRail::Height && Event.Tier == 0
							&& (Event.Reason == EArcTerrainTileAuthorityReason::TileArrived
								|| Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightChanged))
						{
							Patch->bCollisionHeightSourceMissing = false;
						}
					}
					if (State.CollisionWindowUnion.Contains(Key) || State.CollisionBootstrapKeys.Contains(Key))
					{
						EnqueueReady(State, *Terrain, Key);
					}
				}
			}
		}

#if WITH_EDITOR
		if (State.NextCollisionRetry > 0.0 && Now >= State.NextCollisionRetry)
		{
			double NextRetry = 0.0;
			while (State.CollisionRetryHeap.Num() > 0)
			{
				const FArcTerrainCollisionRetryItem& Top = State.CollisionRetryHeap.HeapTop();
				if (Top.RetryAfter > Now)
				{
					NextRetry = Top.RetryAfter;
					break;
				}
				FArcTerrainCollisionRetryItem Due;
				State.CollisionRetryHeap.HeapPop(Due, [](const FArcTerrainCollisionRetryItem& A,
					const FArcTerrainCollisionRetryItem& B)
				{
					return A.RetryAfter < B.RetryAfter;
				}, EAllowShrinking::No);
				const FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(Due.Key);
				if (Patch != nullptr && Patch->bCollisionBuiltIncomplete && Now >= Patch->CollisionRetryAfter)
				{
					EnqueueReady(State, *Terrain, Due.Key);
				}
			}
			State.NextCollisionRetry = NextRetry;
		}
#endif

		if (State.bCollisionRanksDirty)
		{
			RebuildRanks(State, *Terrain);
		}
	}

	TArray<FCollisionBuildCandidate> CollisionCandidates;
	CollisionCandidates.Reserve(MaxCollisionBuildsPerFrame);
	int32 TotalCollisionReady = 0;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			continue;
		}
		FArcTerrainState& State = TerrainPair.Value;
		TotalCollisionReady += State.CollisionReadySet.Num();
		const int32 ReadyCount = FMath::Min(State.CollisionReadyOrdered.Num(), MaxCollisionBuildsPerFrame);
		for (int32 ReadyIndex = 0; ReadyIndex < ReadyCount; ++ReadyIndex)
		{
			const FArcTerrainCollisionReadyItem& Item = State.CollisionReadyOrdered[ReadyIndex];
			InsertBoundedCandidate(CollisionCandidates, MaxCollisionBuildsPerFrame,
				FCollisionBuildCandidate{ Terrain, &State, Item.Key, Item.DistSq },
				FCollisionCandidateOrder());
		}
	}

	const double CollisionDeadline =
		FPlatformTime::Seconds() + static_cast<double>(Settings.CompositeBudgetMs) / 1000.0;
	int32 NumProcessedCollisionCandidates = 0;
	bool bProcessedCandidateStillReady = false;
	bool bSaturationRecorded = false;
	for (const FCollisionBuildCandidate& Candidate : CollisionCandidates)
	{
		if (OutNumCollisionBuilds >= MaxCollisionBuildsPerFrame
			|| (OutNumCollisionAttempts > 0 && FPlatformTime::Seconds() >= CollisionDeadline))
		{
			if (OutNumCollisionBuilds >= MaxCollisionBuildsPerFrame)
			{
				++CollisionCountSaturationSerial;
			}
			else
			{
				++CollisionDeadlineSaturationSerial;
			}
			bSaturationRecorded = true;
			break;
		}
		++NumProcessedCollisionCandidates;
		Candidate.State->PipelineTelemetry.Increment(&FArcTerrainPipelineCounters::CollisionDequeued);
		FArcTerrainCollisionPatch* Patch = Candidate.State->CollisionPatches.Find(Candidate.Key);
		if (Patch == nullptr)
		{
			RemoveReady(*Candidate.State, Candidate.Key);
			continue;
		}
		RemoveReady(*Candidate.State, Candidate.Key);
		const FResolvedResidency CollisionResidency = ResolveResidency(*Candidate.Terrain);
		if (!UpdateCollisionPatch(*Candidate.Terrain, *Candidate.State, Candidate.Key, *Patch,
			Candidate.DistSq, CollisionResidency.CollisionRadius,
			CollisionResidency.CollisionUnloadRadius, bInSyncWindow))
		{
			continue;
		}
		++OutNumCollisionAttempts;
		Candidate.State->PipelineTelemetry.Increment(&FArcTerrainPipelineCounters::CollisionQueuedAttempts);
		if (BuildAndApplyPatchCollision(*Candidate.Terrain, *Candidate.State, Candidate.Key, *Patch,
			/*bAllowBlockingProbe =*/ false))
		{
			++OutNumCollisionBuilds;
		}
#if WITH_EDITOR
		else if (Patch->CollisionProbe.IsValid())
		{
		}
		else if (Patch->bCollisionBuiltIncomplete)
		{
			PushRetry(*Candidate.State, Candidate.Key, Patch->CollisionRetryAfter);
		}
		else
		{
			const bool bStillWants = !Patch->bCollisionHeightSourceMissing
				&& (Patch->AppliedDirtySerial != Patch->DirtySerial
					|| !Patch->CollisionComponent.IsValid());
			if (bStillWants)
			{
				EnqueueReady(*Candidate.State, *Candidate.Terrain, Candidate.Key);
			}
		}
#else
		if (!Patch->bCollisionHeightSourceMissing
			&& (Patch->AppliedDirtySerial != Patch->DirtySerial || !Patch->CollisionComponent.IsValid()))
		{
			EnqueueReady(*Candidate.State, *Candidate.Terrain, Candidate.Key);
		}
#endif
		bProcessedCandidateStillReady |= Candidate.State->CollisionReadySet.Contains(Candidate.Key);
	}

	const bool bHasRemainingReady = TotalCollisionReady > NumProcessedCollisionCandidates
		|| bProcessedCandidateStillReady;
	if (!bSaturationRecorded && bHasRemainingReady)
	{
		if (OutNumCollisionBuilds >= MaxCollisionBuildsPerFrame)
		{
			++CollisionCountSaturationSerial;
		}
		else if (OutNumCollisionAttempts > 0 && FPlatformTime::Seconds() >= CollisionDeadline)
		{
			++CollisionDeadlineSaturationSerial;
		}
	}
}

void UArcTerrainSubsystem::AttributeCollisionSaturation(FArcTerrainState& State)
{
	const uint64 CountDelta = CollisionCountSaturationSerial
		- State.PipelineTelemetry.ConsumedCollisionCountSaturationSerial;
	const uint64 DeadlineDelta = CollisionDeadlineSaturationSerial
		- State.PipelineTelemetry.ConsumedCollisionDeadlineSaturationSerial;
	if (State.CollisionReadySet.Num() > 0)
	{
		State.PipelineTelemetry.IncrementBy(
			&FArcTerrainPipelineCounters::CollisionCountSaturated, CountDelta);
		State.PipelineTelemetry.IncrementBy(
			&FArcTerrainPipelineCounters::CollisionDeadlineSaturated, DeadlineDelta);
	}
	State.PipelineTelemetry.ConsumedCollisionCountSaturationSerial = CollisionCountSaturationSerial;
	State.PipelineTelemetry.ConsumedCollisionDeadlineSaturationSerial = CollisionDeadlineSaturationSerial;
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style): deliberately NOT ArcTerrainSubsystemPrivate,
// which is declared in ArcTerrainSubsystem.h and linked against from other translation units
// (other translation units call its CompositeQuads) — everything here is private to this
// file and must not gain external linkage.
namespace ArcTerrainCollisionBuild
{
	//
	// THE BLOCKING LOAD IS WHY CALLERS MUST CHECK Patch.bCollisionHeightSourceMissing FIRST. It is a
	// whole-file read on a per-patch, per-tick path; a failing read retried every tick is an unbounded
	// stall, so a failure latches and the latch gates the next ATTEMPT, not merely its log.
	bool LoadPatchBakedHeights(AArcTerrain& Terrain, FArcTerrainCollisionKey Key,
		TArray<float>& OutHeights, int32& OutResolution)
	{
		const double LoadStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		int32 LoadedTileCount = 0;
		const FArcTerrainConfig& Config = Terrain.Config;
		const int32 PatchQuads = Config.TileQuads(Terrain.CollisionPatchExtent);
		if (PatchQuads <= 0 || PatchQuads > ArcTerrain::MaxTileQuads)
		{
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionHeightOpGather,
				LoadStartSeconds);
			return false;
		}

		FArcTerrainTileGrid Grid;
		if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, 0, Grid)
			|| Grid.TexelSizeCm != Config.TexelSizeCm())
		{
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionHeightOpGather,
				LoadStartSeconds);
			return false;
		}

		// Global height-texel index space, the one every rail addresses in: the patch's samples run
		// [Key * PatchQuads, Key * PatchQuads + PatchQuads] inclusive, and tile T's payload texel (0,0)
		// is global (T * TileQuads - 1) — the apron convention the writers share.
		const int32 TileQuads = Grid.TileQuads;
		const int32 TileRes = Grid.Resolution();
		const int64 MinX = static_cast<int64>(Key.X) * PatchQuads;
		const int64 MinY = static_cast<int64>(Key.Y) * PatchQuads;
		const int64 MaxX = MinX + PatchQuads;
		const int64 MaxY = MinY + PatchQuads;

		const int32 Resolution = PatchQuads + 1;
		OutHeights.SetNumUninitialized(static_cast<int64>(Resolution) * Resolution);

		const int32 MinTileX = static_cast<int32>(ArcTerrain::FloorDiv(MinX, TileQuads));
		const int32 MaxTileX = static_cast<int32>(ArcTerrain::FloorDiv(MaxX, TileQuads));
		const int32 MinTileY = static_cast<int32>(ArcTerrain::FloorDiv(MinY, TileQuads));
		const int32 MaxTileY = static_cast<int32>(ArcTerrain::FloorDiv(MaxY, TileQuads));

		for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
		{
			for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
			{
				const FIntPoint TileKey(TileX, TileY);
				const FArcTerrainTilePayloadPtr Payload =
					ArcTerrainTileAccess::LoadTileSync(Terrain, EArcTerrainRail::Height, 0, TileKey);
				++LoadedTileCount;
				if (!Payload.IsValid() || Payload->Resolution() != TileRes
					|| Payload->Floats.Num()
						< static_cast<int64>(TileRes) * TileRes * ArcTerrainTileFormat::HeightPlaneCount)
				{
					if (ArcTerrainDiag::IsVerbose())
					{
						const double LoadSeconds = FPlatformTime::Seconds() - LoadStartSeconds;
						ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionHeightOpGather,
							LoadStartSeconds);
						UE_LOGF(LogArcTerrainDiag, Log,"[CollisionTelemetry] '%ls' patch (%d,%d) synchronous height loads=%d duration=%.2f ms (failed).",
							*Terrain.GetName(), Key.X, Key.Y, LoadedTileCount,
							LoadSeconds * 1000.0);
					}
					else
					{
						ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionHeightOpGather,
							LoadStartSeconds);
					}
					return false;
				}

				const int64 TileOriginX = static_cast<int64>(TileX) * TileQuads;
				const int64 TileOriginY = static_cast<int64>(TileY) * TileQuads;
				const int64 CopyMinX = FMath::Max(MinX, TileOriginX);
				const int64 CopyMaxX = FMath::Min(MaxX, TileOriginX + TileQuads - 1);
				const int64 CopyMinY = FMath::Max(MinY, TileOriginY);
				const int64 CopyMaxY = FMath::Min(MaxY, TileOriginY + TileQuads - 1);
				if (CopyMaxX < CopyMinX || CopyMaxY < CopyMinY)
				{
					continue;
				}

				const int32 RunTexels = static_cast<int32>(CopyMaxX - CopyMinX + 1);
				const float* const Plane = Payload->Floats.GetData();
				for (int64 GlobalY = CopyMinY; GlobalY <= CopyMaxY; ++GlobalY)
				{
					// Payload index = global index - (TileKey * TileQuads - 1).
					const int64 SourceRow = GlobalY - TileOriginY + 1;
					const int64 SourceCol = CopyMinX - TileOriginX + 1;
					FMemory::Memcpy(
						OutHeights.GetData() + (GlobalY - MinY) * Resolution + (CopyMinX - MinX),
						Plane + SourceRow * TileRes + SourceCol,
						static_cast<int64>(RunTexels) * sizeof(float));
				}
			}
		}

		OutResolution = Resolution;
		const double LoadSeconds = FPlatformTime::Seconds() - LoadStartSeconds;
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionHeightOpGather,
			LoadStartSeconds);
		if (ArcTerrainDiag::IsVerbose())
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[CollisionTelemetry] '%ls' patch (%d,%d) synchronous height loads=%d duration=%.2f ms.",
				*Terrain.GetName(), Key.X, Key.Y, LoadedTileCount,
				LoadSeconds * 1000.0);
		}
		return true;
	}

	TRefCountPtr<FArcTerrainHeightfieldRef> BuildFromBakedTiles(AArcTerrain& Terrain,
		FArcTerrainCollisionKey Key)
	{
		TArray<float> Heights;
		int32 Resolution = 0;
		if (!LoadPatchBakedHeights(Terrain, Key, Heights, Resolution) || Resolution < 2)
		{
			return nullptr;
		}

		const double TexelToLocal = static_cast<double>(Terrain.Config.TexelSizeCm());
		const double BuildStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		TRefCountPtr<FArcTerrainHeightfieldRef> Heightfield = ArcTerrainCollision::BuildHeightfield(
			Heights, Resolution, FVector(TexelToLocal, TexelToLocal, 1.0));
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionHeightfieldBuild,
			BuildStartSeconds);
		if (ArcTerrainDiag::IsVerbose())
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[CollisionTelemetry] '%ls' patch (%d,%d) BuildHeightfield duration=%.2f ms.",
				*Terrain.GetName(), Key.X, Key.Y,
				(FPlatformTime::Seconds() - BuildStartSeconds) * 1000.0);
		}
		return Heightfield;
	}

	void ReportMissingHeightSource(const AArcTerrain& Terrain, FArcTerrainCollisionKey Key,
		FArcTerrainCollisionPatch& Patch)
	{
		Patch.bCollisionHeightSourceMissing = true;
		UE_LOGF(LogArcTerrain, Warning,
			"[TerrainCollision] Collision patch (%d,%d) on '%ls' has NO COLLISION: required height "
			"tiles are absent, unreadable, incompatible with the collision lattice, or could not "
			"produce a heightfield. Rebuild them.",
			Key.X, Key.Y, *Terrain.GetName());
	}

#if WITH_EDITOR
	void ScheduleIncompleteRetry(FArcTerrainCollisionPatch& Patch)
	{
		Patch.bCollisionBuiltIncomplete = true;
		Patch.CollisionRetryAfter = ArcTerrainSubsystemPrivate::ScheduleExponentialRetry(
			Patch.CollisionRetryCount, 0.5, FPlatformTime::Seconds());
	}

	void ClearIncompleteRetry(FArcTerrainCollisionPatch& Patch)
	{
		Patch.bCollisionBuiltIncomplete = false;
		Patch.CollisionRetryCount = 0;
		Patch.CollisionRetryAfter = 0.0;
	}
#endif

	bool IsIncompleteRetryDue(const FArcTerrainCollisionPatch& Patch)
	{
#if WITH_EDITOR
		return Patch.bCollisionBuiltIncomplete && FPlatformTime::Seconds() >= Patch.CollisionRetryAfter;
#else
		return false;
#endif
	}
}

bool UArcTerrainSubsystem::BuildAndApplyPatchCollision(AArcTerrain& Terrain,
	FArcTerrainState& State, FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch,
	bool bAllowBlockingProbe)
{
	(void)State;
	(void)bAllowBlockingProbe;

	if (Terrain.CollisionPatchSize() <= 0.0)
	{
		return false;
	}

	const uint64 BuiltSerial = Patch.DirtySerial;
	if (!Patch.bCollisionHeightSourceMissing)
	{
		TRefCountPtr<FArcTerrainHeightfieldRef> Baked =
			ArcTerrainCollisionBuild::BuildFromBakedTiles(Terrain, Key);
		if (Baked.IsValid())
		{
			ArcTerrainDiag::NoteCollisionBuild(Key, /*bFromBakedTiles=*/true);
			ApplyPatchCollision(Terrain, Key, Patch, Baked);
			Patch.AppliedDirtySerial = BuiltSerial;
			Patch.bCollisionHeightSourceMissing = false;
			return true;
		}
		ArcTerrainCollisionBuild::ReportMissingHeightSource(Terrain, Key, Patch);
	}
	return false;
}

bool UArcTerrainSubsystem::UpdateCollisionPatch(AArcTerrain& Terrain, FArcTerrainState& State,
	FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch, double InvokerDistSq,
	double CollisionRadius, double CollisionUnloadRadius, bool bInSyncWindow)
{
	const double GateRadius = Patch.CollisionComponent.IsValid() ? CollisionUnloadRadius : CollisionRadius;
	if (InvokerDistSq > GateRadius * GateRadius)
	{
		// During the game-entry sync window keep entry-built collision alive even though the invokers
		// haven't stabilized yet (the very first frames may see none at all).
		if (!bInSyncWindow)
		{
			DestroyPatchCollision(Patch);
		}
		return false;
	}

	// BEFORE every early-out below: a modifier's UStaticMesh needs nothing from the heightfield, so its
	// body exists on the collision ring alone. Behind the build it would be silently absent for the
	// whole residency of a patch whose baked payload is missing.
	SyncPatchMeshModifierCollision(Terrain, State, Key, Patch);

	if (Patch.bCollisionHeightSourceMissing)
	{
		return false;
	}

	if (Patch.AppliedDirtySerial == Patch.DirtySerial && Patch.CollisionComponent.IsValid()
		&& !ArcTerrainCollisionBuild::IsIncompleteRetryDue(Patch))
	{
		return false;
	}

	return true;
}

void UArcTerrainSubsystem::ApplyPatchCollision(AArcTerrain& Terrain, FArcTerrainCollisionKey Key,
	FArcTerrainCollisionPatch& Patch, const TRefCountPtr<FArcTerrainHeightfieldRef>& Heightfield)
{
	const double ApplyStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	ArcTerrainDiag::NoteCollisionApplied(Key);

	UArcTerrainCollisionComponent* Component = Patch.CollisionComponent.Get();
	bool bRegistered = false;
	double RegisterSeconds = 0.0;
	if (Component == nullptr)
	{
		Component = NewObject<UArcTerrainCollisionComponent>(&Terrain, NAME_None, RF_Transient);
		Component->SetupAttachment(Terrain.GetRootComponent());
		const double RegisterStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		Component->RegisterComponent();
		RegisterSeconds = FPlatformTime::Seconds() - RegisterStartSeconds;
		bRegistered = true;
		const double PatchSize = Terrain.CollisionPatchSize();
		Component->SetRelativeLocation(FVector(
			static_cast<double>(Key.X) * PatchSize,
			static_cast<double>(Key.Y) * PatchSize,
			0.0));
		Patch.CollisionComponent = Component;
	}
	// Double-buffered swap + RecreatePhysicsState inside.
	const double SetHeightfieldStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	Component->SetHeightfield(Heightfield);
	const double SetHeightfieldSeconds = FPlatformTime::Seconds() - SetHeightfieldStartSeconds;
	if (ArcTerrainDiag::IsVerbose() && bRegistered)
	{
		TArray<UArcTerrainCollisionComponent*> CollisionComponents;
		Terrain.GetComponents<UArcTerrainCollisionComponent>(CollisionComponents);
		UE_LOGF(LogArcTerrainDiag, Log,"[CollisionTelemetry] '%ls' collision components=%d patch quads effective=%d configured=%d "
				"register=%.2f ms SetHeightfield/RecreatePhysicsState=%.2f ms.",
			*Terrain.GetName(), CollisionComponents.Num(), Terrain.Config.TileQuads(Terrain.CollisionPatchExtent),
			Terrain.CollisionPatchQuads, RegisterSeconds * 1000.0, SetHeightfieldSeconds * 1000.0);
	}
	ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionApply, ApplyStartSeconds);
}

void UArcTerrainSubsystem::SyncPatchMeshModifierCollision(AArcTerrain& Terrain,
	const FArcTerrainState& State, FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch)
{
	const double SyncStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	bool bCachedSetValid = Patch.SyncedMeshModifierRevision == State.MeshModifierRevision
		&& Patch.SyncedMeshModifiers.Num() == Patch.MeshModifierCollision.Num();
	for (int32 Index = 0; bCachedSetValid && Index < Patch.SyncedMeshModifiers.Num(); ++Index)
	{
		UArcTerrainMeshModifier* Modifier = Patch.SyncedMeshModifiers[Index].Get();
		UStaticMeshComponent* Body = Patch.MeshModifierCollision[Index].Get();
		bCachedSetValid = Modifier != nullptr && Body != nullptr;
	}

	if (bCachedSetValid)
	{
		for (int32 Index = 0; Index < Patch.SyncedMeshModifiers.Num(); ++Index)
		{
			UArcTerrainMeshModifier* Modifier = Patch.SyncedMeshModifiers[Index].Get();
			UStaticMeshComponent* Body = Patch.MeshModifierCollision[Index].Get();
			const FTransform& Pose = Modifier->GetComponentTransform();
			if (!Body->GetComponentTransform().Equals(Pose))
			{
				Body->SetWorldTransform(Pose);
			}
		}
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionModifierSync,
			SyncStartSeconds);
		return;
	}

	TArray<UArcTerrainMeshModifier*> SortedModifiers;
	ArcTerrainSubsystemPrivate::GatherSortedMeshModifiers(State, Key,
		ArcTerrain::CollisionPatchLocalBounds(Key, Terrain.CollisionPatchSize()), SortedModifiers);

	// A gathered modifier always has a mesh (a null one yields an invalid ComputeBounds box), but the
	// pointer is re-checked below because it decides whether a component is created at all.
	bool bSetMatches = Patch.MeshModifierCollision.Num() == SortedModifiers.Num();
	for (int32 Index = 0; bSetMatches && Index < SortedModifiers.Num(); ++Index)
	{
		const UStaticMeshComponent* Existing = Patch.MeshModifierCollision[Index].Get();
		const UArcTerrainMeshModifier* Modifier = SortedModifiers[Index];
		bSetMatches = Existing != nullptr && Modifier != nullptr && Modifier->Mesh != nullptr
			&& Existing->GetStaticMesh() == Modifier->Mesh
			&& ArcTerrainSubsystemPrivate::MeshModifierMaterialsMatch(*Existing, *Modifier);
	}

	// A POSE-ONLY delta is applied in place. An interactive gizmo drag re-dirties the patch every frame,
	// so destroying and recreating N physics states here would be a per-frame hitch.
	if (bSetMatches)
	{
		for (int32 Index = 0; Index < SortedModifiers.Num(); ++Index)
		{
			UStaticMeshComponent* Existing = Patch.MeshModifierCollision[Index].Get();
			const FTransform& Pose = SortedModifiers[Index]->GetComponentTransform();
			if (!Existing->GetComponentTransform().Equals(Pose))
			{
				Existing->SetWorldTransform(Pose);
			}
		}
		Patch.SyncedMeshModifiers.Reset(SortedModifiers.Num());
		for (UArcTerrainMeshModifier* Modifier : SortedModifiers)
		{
			Patch.SyncedMeshModifiers.Add(Modifier);
		}
		Patch.SyncedMeshModifierRevision = State.MeshModifierRevision;
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionModifierSync,
			SyncStartSeconds);
		return;
	}

	DestroyPatchMeshModifierCollision(Patch);

	Patch.MeshModifierCollision.Reserve(SortedModifiers.Num());
	for (UArcTerrainMeshModifier* Modifier : SortedModifiers)
	{
		if (Modifier == nullptr || Modifier->Mesh == nullptr)
		{
			continue;
		}

		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(&Terrain, NAME_None, RF_Transient);
		Body->SetupAttachment(Terrain.GetRootComponent());
		// Never drawn — the modifier's own mesh renders it. The
		// hidden FStaticMeshSceneProxy is still registered; CastShadow keeps it out of shadow and
		// distance-field gathers.
		Body->SetVisibility(false);
		Body->CastShadow = false;
		Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Body->SetGenerateOverlapEvents(false);
		Body->SetStaticMesh(Modifier->Mesh);
		// Complex collision derives its physical materials from the COMPONENT's materials
		// (FBodyInstance::GetComplexPhysicalMaterials), so the override the merged section renders with
		// has to ride on the body or every trace reports the mesh's own LOD0 surface.
		if (Modifier->MaterialOverride != nullptr)
		{
			const int32 NumMaterials = Body->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
			{
				Body->SetMaterial(SlotIndex, Modifier->MaterialOverride);
			}
		}
		Body->RegisterComponent();
		Body->SetWorldTransform(Modifier->GetComponentTransform());
		Patch.MeshModifierCollision.Add(Body);
	}
	Patch.SyncedMeshModifiers.Reset(SortedModifiers.Num());
	for (UArcTerrainMeshModifier* Modifier : SortedModifiers)
	{
		Patch.SyncedMeshModifiers.Add(Modifier);
	}
	Patch.SyncedMeshModifierRevision = State.MeshModifierRevision;
	ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::CollisionModifierSync,
		SyncStartSeconds);
}

void UArcTerrainSubsystem::DestroyPatchMeshModifierCollision(FArcTerrainCollisionPatch& Patch)
{
	for (TWeakObjectPtr<UStaticMeshComponent>& BodyPtr : Patch.MeshModifierCollision)
	{
		if (UStaticMeshComponent* Body = BodyPtr.Get())
		{
			Body->DestroyComponent();
		}
	}
	Patch.MeshModifierCollision.Reset();
	Patch.SyncedMeshModifiers.Reset();
	Patch.SyncedMeshModifierRevision = 0;
}

void UArcTerrainSubsystem::DestroyPatchCollision(FArcTerrainCollisionPatch& Patch)
{
	DestroyPatchMeshModifierCollision(Patch);
	if (UArcTerrainCollisionComponent* Component = Patch.CollisionComponent.Get())
	{
		Component->DestroyComponent(); // destroys the physics state
	}
	Patch.CollisionComponent = nullptr;
	Patch.AppliedDirtySerial = 0;
	// Ring exit -> ring re-entry is a fresh attempt, and the tiles may have loaded meanwhile: let the
	// baked-branch attempt (and its diagnosis) run once more rather than staying latched.
	Patch.bCollisionHeightSourceMissing = false;
#if WITH_EDITOR
	Patch.AppliedCollisionKeyHash = FIoHash();
	Patch.CollisionProbe.Reset();
	Patch.CollisionProbeKey = FIoHash();
	ArcTerrainCollisionBuild::ClearIncompleteRetry(Patch);
#endif
}

void UArcTerrainSubsystem::SyncBuildPatchCollision(AArcTerrain& Terrain, FArcTerrainState& State,
	FArcTerrainCollisionKey Key, FArcTerrainCollisionPatch& Patch)
{
	if (Terrain.CollisionPatchSize() <= 0.0)
	{
		return;
	}

	// This path exists so a pawn has ground the MOMENT it spawns, and a modifier's body is ground too —
	// sync it ahead of the early-out below, exactly as the tick path does at its ring gate.
	SyncPatchMeshModifierCollision(Terrain, State, Key, Patch);

#if !WITH_EDITOR
	if (Patch.bCollisionHeightSourceMissing)
	{
		return;
	}
#endif

	if (Patch.AppliedDirtySerial == Patch.DirtySerial && Patch.CollisionComponent.IsValid()
		&& !ArcTerrainCollisionBuild::IsIncompleteRetryDue(Patch))
	{
		return; // already up to date
	}

	State.PipelineTelemetry.Increment(&FArcTerrainPipelineCounters::CollisionSyncAttempts);
	BuildAndApplyPatchCollision(Terrain, State, Key, Patch, /*bAllowBlockingProbe =*/ true);
}

void UArcTerrainSubsystem::SyncBuildCollisionAroundAnchor(AArcTerrain& Terrain, FArcTerrainState& State,
	const FVector& WorldAnchor, double Radius)
{
	const double PatchSize = Terrain.CollisionPatchSize();
	if (PatchSize <= 0.0)
	{
		return;
	}

	// Radius applied in TERRAIN-LOCAL units (terrain scale ~1 assumed — see GPawnCellProbeExtent):
	// a scaled terrain only trims/pads the OUTER ring of sync-built patches; the anchor's own patch is
	// always included, and the tick-side collision ring (world-space rect test) takes over after
	// the sync window.
	const FVector Local = Terrain.WorldToTerrainLocal(WorldAnchor);
	const FBox AnchorBounds(
		Local - FVector(Radius, Radius, 0.0),
		Local + FVector(Radius, Radius, 0.0));

	TArray<FArcTerrainCollisionKey> AnchorPatches;
	ArcTerrain::CollisionPatchesIntersectingLocalBounds(AnchorBounds, PatchSize, AnchorPatches);
	for (FArcTerrainCollisionKey Key : AnchorPatches)
	{
		FArcTerrainCollisionPatch& Patch = ArcTerrainCollisionIndex_e7b2::EnsurePatch(State, Key);
		ArcTerrainCollisionIndex_e7b2::NoteBootstrapKey(State, Key);
		SyncBuildPatchCollision(Terrain, State, Key, Patch);
	}
}
