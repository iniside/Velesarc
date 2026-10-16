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

namespace ArcTerrainSubsystemPrivate
{
	// Refreshes InvokerDistSq on each listed cell and returns the smallest — the sector's rank when the
	// list is a sector's cells. Ranking on the sector RECT instead would be a strict lower bound: cells
	// exist only where something shaped them, so the live cells do not tile the rect, and a sector whose
	// only cell sits in its far corner would outrank a fully populated nearer one at the cap.
	double UpdateCellDistances(TArray<FArcTerrainCellHot>& HotCells, TConstArrayView<int32> CellIndices,
		double CellSize, const FTransform& TerrainTransform, TConstArrayView<FVector> WorldInvokers)
	{
		double BestDistSq = TNumericLimits<double>::Max();
		for (const int32 HotIndex : CellIndices)
		{
			FArcTerrainCellHot& Hot = HotCells[HotIndex];
			Hot.InvokerDistSq = CellInvokerDistSq(Hot.Key, CellSize, TerrainTransform, WorldInvokers);
			BestDistSq = FMath::Min(BestDistSq, Hot.InvokerDistSq);
		}
		return BestDistSq;
	}

	// Reset the atlas-publication tags of a cell that is about to be destroyed or re-admitted, so a cell
	// returning to residency republishes into whatever slice it is then handed.
	void ResetCellAtlasPublication(FArcTerrainCellState& Cell)
	{
		Cell.bAtlasRefillPending = false;
		Cell.AtlasHeightVersion = 0;
		Cell.HeightTileSerial = 0;
		Cell.AtlasHeightTileSerial = 0;
		Cell.AtlasWeightTileSerial = 0;
	}

	FResolvedResidency ResolveResidency(const AArcTerrain& Terrain)
	{
		FResolvedResidency Resolved;
		Resolved.PreviewRadius = static_cast<double>(Terrain.PreviewRadius);
		Resolved.PreviewUnloadRadius = Resolved.PreviewRadius
			* FMath::Max(static_cast<double>(Terrain.PreviewRadiusHysteresis), 1.0);
		Resolved.CollisionRadius =
			FMath::Min(static_cast<double>(Terrain.CollisionRadius), Resolved.PreviewRadius);
		// Same rule as PreviewUnloadRadius. At 1.0 this is CollisionRadius bit-for-bit (R * 1.0 == R in
		// IEEE-754 for every finite R), so the split gate collapses to the single-threshold predicate.
		Resolved.CollisionUnloadRadius = Resolved.CollisionRadius
			* FMath::Max(static_cast<double>(Terrain.CollisionRadiusHysteresis), 1.0);
		Resolved.MaxResidentSectors = ArcTerrain::SumAuthoredResidentSectors(Terrain);
		Resolved.MaxResidentWeightCells = FMath::Max(Terrain.MaxResidentWeightCells, 1);
		return Resolved;
	}

	// THE ONE ATLAS-LAYOUT AUTHORITY. Phase 1 (which sector a cell belongs to) and phase 2b (which slice
	// that sector holds) MUST agree on SectorShift, and ResolveTileAtlasLayout may LOWER the authored
	// shift to fit the device limit — so both phases resolve through here rather than reading the
	// actor's property directly.
	FArcTerrainTileAtlasLayout ResolveTerrainAtlasLayout(const AArcTerrain& Terrain,
		const FArcTerrainState& State, const FResolvedResidency& Residency)
	{
		const FArcTerrainConfig& Config = Terrain.Config;
		const int32 NumChannels = Config.ChannelSet != nullptr ? Config.ChannelSet->Channels.Num() : 0;
		return ArcTerrainAtlas::ResolveTileAtlasLayout(CompositeQuads(Config), ResolveWeightQuads(Config, State),
			NumChannels, Residency.MaxResidentSectors, Residency.MaxResidentWeightCells, Terrain.SectorShift,
			Terrain.WeightSliceShift, Terrain.HeightOptions.BaseHeightCm);
	}
	struct FGatheredStamp
	{
		UArcTerrainStampComponent* Stamp = nullptr;
	};

	bool IsStampOrderedBefore(const UArcTerrainStampComponent& A, const UArcTerrainStampComponent& B)
	{
		const UArcTerrainStampComponent* AnchorA = A.GetCompositeOrderAnchor();
		const UArcTerrainStampComponent* AnchorB = B.GetCompositeOrderAnchor();
		AnchorA = AnchorA != nullptr ? AnchorA : &A;
		AnchorB = AnchorB != nullptr ? AnchorB : &B;
		if (AnchorA->Priority != AnchorB->Priority)
		{
			return AnchorA->Priority < AnchorB->Priority;
		}
		if (AnchorA->GetPathName() != AnchorB->GetPathName())
		{
			return AnchorA->GetPathName() < AnchorB->GetPathName();
		}
		return A.GetPathName() < B.GetPathName();
	}

	// Declared in ArcTerrainSubsystem.h (the composite order AND the DDC-key stamp order).
	void GatherSortedStamps(const FArcTerrainState& State, const FBox& CellBounds,
		TArray<UArcTerrainStampComponent*>& OutStamps)
	{
		TArray<FGatheredStamp> Gathered;
		Gathered.Reserve(State.Stamps.Num());
		for (const FArcTerrainStampEntry& Entry : State.Stamps)
		{
			UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
			if (Stamp == nullptr || Stamp->bIsDisabled)
			{
				continue;
			}
			const FBox& StampBounds = Entry.Bounds;
			if (!StampBounds.IsValid || !StampBounds.Intersect(CellBounds))
			{
				continue;
			}
			FGatheredStamp& GatheredStamp = Gathered.AddDefaulted_GetRef();
			GatheredStamp.Stamp = Stamp;
		}
		Gathered.Sort([](const FGatheredStamp& A, const FGatheredStamp& B)
		{
			return IsStampOrderedBefore(*A.Stamp, *B.Stamp);
		});

		OutStamps.Reset(Gathered.Num());
		for (const FGatheredStamp& GatheredStamp : Gathered)
		{
			OutStamps.Add(GatheredStamp.Stamp);
		}
	}
	FSectorWindow ComputeSectorWindow(const FVector& WorldInvoker, double WorldRadius, int32 SectorShift,
		double CellSize, const FTransform& TerrainTransform, double MaxSpan)
	{
		FSectorWindow Window;
		const double SectorSize = CellSize * static_cast<double>(static_cast<int64>(1) << SectorShift);
		if (SectorSize <= 0.0 || WorldRadius < 0.0)
		{
			return Window;
		}

		const FVector Scale = TerrainTransform.GetScale3D();
		const double MinAxisScale = FMath::Max(
			FMath::Min(FMath::Abs(Scale.X), FMath::Abs(Scale.Y)), UE_DOUBLE_SMALL_NUMBER);
		const double AskedSpan = FMath::CeilToDouble(WorldRadius / MinAxisScale / SectorSize);
		const double SpanSectors = FMath::Min(AskedSpan, FMath::Max(MaxSpan, 0.0));
		Window.bSpanClamped = SpanSectors < AskedSpan;

		const FVector Local = TerrainTransform.InverseTransformPosition(WorldInvoker);
		constexpr double KeyLimit = 1.0e9;
		const double CentreX = FMath::Clamp(FMath::Floor(Local.X / SectorSize), -KeyLimit, KeyLimit);
		const double CentreY = FMath::Clamp(FMath::Floor(Local.Y / SectorSize), -KeyLimit, KeyLimit);
		Window.MinX = static_cast<int32>(FMath::Max(CentreX - SpanSectors, -KeyLimit));
		Window.MinY = static_cast<int32>(FMath::Max(CentreY - SpanSectors, -KeyLimit));
		Window.MaxX = static_cast<int32>(FMath::Min(CentreX + SpanSectors, KeyLimit));
		Window.MaxY = static_cast<int32>(FMath::Min(CentreY + SpanSectors, KeyLimit));
		return Window;
	}
	constexpr int32 GMaxWeightGroupWindowSpan = 32;
	FWantedWeightGroup& FindOrAddWantedWeightGroup(TArray<FWantedWeightGroup>& Wanted,
		TMap<FArcTerrainWeightGroupId, int32>& IndexById, FArcTerrainWeightGroupId Id)
	{
		const int32* Existing = IndexById.Find(Id);
		if (Existing != nullptr)
		{
			return Wanted[*Existing];
		}
		FWantedWeightGroup& Group = Wanted[IndexById.Add(Id, Wanted.AddDefaulted())];
		Group.Id = Id;
		return Group;
	}

	void GatherWantedWeightGroups(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainWeightLadder& WeightLadder,
		const FTransform& TerrainTransform, TConstArrayView<FVector> InvokerPositions,
		TConstArrayView<FContentCellRange> ContentRanges, double Now,
		TArray<FWantedWeightGroup>& OutGroups)
	{
		OutGroups.Reset();

		if (WeightLadder.Tiers.IsEmpty())
		{
			return;
		}

		const FArcTerrainConfig& Config = Terrain.Config;
		const int32 GroupShift = Layout.WeightSliceShift;
		const int32 GroupCellSpan = 1 << GroupShift;
		const double CellSize = Config.CellSize();
		const FResolvedWeightTier& BaseTier = WeightLadder.Tiers[0];
		const double ResidencyRadius = BaseTier.BandOuterRadiusCm;
		const double UnloadRadius =
			WeightLadder.Tiers.Num() > 1 ? BaseTier.BandOuterRadiusCm : BaseTier.BandUnloadRadiusCm;
		const double ResidencyRadiusSq = ResidencyRadius * ResidencyRadius;
		const double UnloadRadiusSq = UnloadRadius * UnloadRadius;

		// Read before the caller overwrites State.ResidentWeightGroups from OutGroups — the hysteresis
		// boundary is the set granted LAST tick.
		TSet<FArcTerrainWeightGroupId> PreviousResidentGroups;
		PreviousResidentGroups.Reserve(State.ResidentWeightGroups.Num());
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			PreviousResidentGroups.Add(FArcTerrainWeightGroupId{ Group.Tier, Group.Key });
		}

		TArray<FSectorWindow> InvokerWindows;
		InvokerWindows.Reserve(InvokerPositions.Num());
		for (const FVector& Invoker : InvokerPositions)
		{
			InvokerWindows.Add(ComputeSectorWindow(Invoker, UnloadRadius,
				GroupShift, CellSize, TerrainTransform,
				static_cast<double>(GMaxWeightGroupWindowSpan)));
		}

		TMap<FIntPoint, double> ContentGroupDistSq;
		for (const FContentCellRange& Range : ContentRanges)
		{
			for (const FSectorWindow& Window : InvokerWindows)
			{
				const int32 MinGroupX = FMath::Max(Range.Min.X >> GroupShift, Window.MinX);
				const int32 MaxGroupX = FMath::Min(Range.Max.X >> GroupShift, Window.MaxX);
				const int32 MinGroupY = FMath::Max(Range.Min.Y >> GroupShift, Window.MinY);
				const int32 MaxGroupY = FMath::Min(Range.Max.Y >> GroupShift, Window.MaxY);
				for (int32 GroupY = MinGroupY; GroupY <= MaxGroupY; ++GroupY)
				{
					for (int32 GroupX = MinGroupX; GroupX <= MaxGroupX; ++GroupX)
					{
						const int32 GroupMinCellX = GroupX * GroupCellSpan;
						const int32 GroupMinCellY = GroupY * GroupCellSpan;
						const int32 ClipMinX = FMath::Max(Range.Min.X, GroupMinCellX);
						const int32 ClipMaxX =
							FMath::Min(Range.Max.X, GroupMinCellX + GroupCellSpan - 1);
						const int32 ClipMinY = FMath::Max(Range.Min.Y, GroupMinCellY);
						const int32 ClipMaxY =
							FMath::Min(Range.Max.Y, GroupMinCellY + GroupCellSpan - 1);
						const double DistSq = RectInvokerDistSq(
							static_cast<double>(ClipMinX) * CellSize,
							static_cast<double>(ClipMinY) * CellSize,
							static_cast<double>(ClipMaxX + 1) * CellSize,
							static_cast<double>(ClipMaxY + 1) * CellSize,
							TerrainTransform, InvokerPositions);
						const FIntPoint ContentGroupKey(GroupX, GroupY);
						double* Best = ContentGroupDistSq.Find(ContentGroupKey);
						if (Best != nullptr)
						{
							*Best = FMath::Min(*Best, DistSq);
						}
						else
						{
							ContentGroupDistSq.Add(ContentGroupKey, DistSq);
						}
					}
				}
			}
		}

		int32 NumGroupsNoContent = 0;
		int32 NumGroupsOutOfRadius = 0;
		int32 NumWindowSpanClamped = 0;
		double NearestRejectedDistSq = TNumericLimits<double>::Max();

		TMap<FArcTerrainWeightGroupId, int32> GroupIndexById;
		for (const FSectorWindow& Window : InvokerWindows)
		{
			NumWindowSpanClamped += Window.bSpanClamped ? 1 : 0;
			for (int32 GroupY = Window.MinY; GroupY <= Window.MaxY; ++GroupY)
			{
				for (int32 GroupX = Window.MinX; GroupX <= Window.MaxX; ++GroupX)
				{
					const FIntPoint GroupKey(GroupX, GroupY);
					const FArcTerrainWeightGroupId GroupId{ 0, GroupKey };
					if (GroupIndexById.Contains(GroupId))
					{
						continue;
					}
					// Authored content ALONE, where the sector test also takes live cells: the ground a
					// group covers can be answered for by a current height tile, and then no cell record
					// for it exists to be found.
					const double* ContentDistSq = ContentGroupDistSq.Find(GroupKey);
					if (ContentDistSq == nullptr)
					{
						++NumGroupsNoContent;
						continue;
					}
					const double DistSq = *ContentDistSq;
					const bool bWasResident = PreviousResidentGroups.Contains(GroupId);
					if (DistSq > (bWasResident ? UnloadRadiusSq : ResidencyRadiusSq))
					{
						++NumGroupsOutOfRadius;
						NearestRejectedDistSq = FMath::Min(NearestRejectedDistSq, DistSq);
						continue;
					}
					FindOrAddWantedWeightGroup(OutGroups, GroupIndexById, GroupId).DistSq = DistSq;
				}
			}
		}

		for (const FArcTerrainCellKey& PinnedKey : State.PinnedCells)
		{
			const FArcTerrainCellState* PinnedCell = State.Cells.Find(PinnedKey);
			if (PinnedCell == nullptr || State.Hot(*PinnedCell).PinnedUntil <= Now)
			{
				continue;
			}
			const FIntPoint PinnedGroupKey = ArcTerrainAtlas::GroupKeyFromCell(PinnedKey, GroupShift);
			const FArcTerrainWeightGroupId PinnedGroupId{ 0, PinnedGroupKey };
			const bool bFirstReach = !GroupIndexById.Contains(PinnedGroupId);
			FWantedWeightGroup& Group =
				FindOrAddWantedWeightGroup(OutGroups, GroupIndexById, PinnedGroupId);
			Group.bPinned = true;
			if (bFirstReach)
			{
				const double GroupMinX =
					static_cast<double>(PinnedGroupKey.X * GroupCellSpan) * CellSize;
				const double GroupMinY =
					static_cast<double>(PinnedGroupKey.Y * GroupCellSpan) * CellSize;
				Group.DistSq = RectInvokerDistSq(GroupMinX, GroupMinY,
					GroupMinX + static_cast<double>(GroupCellSpan) * CellSize,
					GroupMinY + static_cast<double>(GroupCellSpan) * CellSize,
					TerrainTransform, InvokerPositions);
			}
		}

		// A coarse group has no cells under it and never will, so the authored-content test above does not
		// apply to it: the one thing that has to exist is the baked tile. There is deliberately NO INNER
		// RADIUS TEST — bands overlap and the ring walk stops at the first finer hit.
		int32 NumCoarseGroupsNoTile = 0;
		int32 NumCoarseGroupsOutOfRadius = 0;
		for (int32 Tier = 1; Tier < WeightLadder.Tiers.Num(); ++Tier)
		{
			const FResolvedWeightTier& TierEntry = WeightLadder.Tiers[Tier];
			// Only the last tier's outer boundary meets the fill slice: an inner band is covered by the tier
			// outside it, so hysteresis there funds slices nothing samples.
			const bool bLastTier = Tier == WeightLadder.Tiers.Num() - 1;
			const double OuterRadius = TierEntry.BandOuterRadiusCm;
			const double OuterUnloadRadius = bLastTier ? TierEntry.BandUnloadRadiusCm : OuterRadius;
			const double OuterRadiusSq = OuterRadius * OuterRadius;
			const double OuterUnloadRadiusSq = OuterUnloadRadius * OuterUnloadRadius;

			const FArcTerrainTileGrid TierGrid = ArcTerrainTileFormat::WeightTileGrid(Config,
				TierEntry.CellWeightQuads, TierEntry.CumulativeShift);
			if (!TierGrid.IsValid())
			{
				continue;
			}

			// CELLS, not texels: ComputeSectorWindow sizes its unit as CellSize << Shift.
			const int32 SliceShift = TierEntry.SliceShift;
			const double GroupSize =
				CellSize * static_cast<double>(static_cast<int64>(1) << SliceShift);
			for (const FVector& Invoker : InvokerPositions)
			{
				const FSectorWindow Window = ComputeSectorWindow(Invoker, OuterUnloadRadius, SliceShift,
					CellSize, TerrainTransform, static_cast<double>(GMaxWeightGroupWindowSpan));
				NumWindowSpanClamped += Window.bSpanClamped ? 1 : 0;
				for (int32 GroupY = Window.MinY; GroupY <= Window.MaxY; ++GroupY)
				{
					for (int32 GroupX = Window.MinX; GroupX <= Window.MaxX; ++GroupX)
					{
						const FArcTerrainWeightGroupId GroupId{ Tier, FIntPoint(GroupX, GroupY) };
						if (GroupIndexById.Contains(GroupId))
						{
							continue;
						}
						const FIntPoint TileKey =
							CoarseWeightGroupTileKey(TierGrid, GroupId.Key, SliceShift);
						if (!ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Weight, Tier, TileKey,
							&TierGrid))
						{
							++NumCoarseGroupsNoTile;
							continue;
						}
						const double MinX = static_cast<double>(GroupX) * GroupSize;
						const double MinY = static_cast<double>(GroupY) * GroupSize;
						const double DistSq = RectInvokerDistSq(MinX, MinY, MinX + GroupSize,
							MinY + GroupSize, TerrainTransform, InvokerPositions);
						const bool bWasResident = PreviousResidentGroups.Contains(GroupId);
						if (DistSq > (bWasResident ? OuterUnloadRadiusSq : OuterRadiusSq))
						{
							++NumCoarseGroupsOutOfRadius;
							continue;
						}
						FindOrAddWantedWeightGroup(OutGroups, GroupIndexById, GroupId).DistSq = DistSq;
					}
				}
			}
		}

		if (ArcTerrainDiag::IsWeightCompositeVerbose())
		{
			UE_LOGF(LogArcTerrain, Log,
				"[WeightAdmit] select: tiers %d | tier0 radius %.0f m unload %.0f m | tier0 windows %d "
				"(spanclamped %d across all tiers) | rejected: nocontent %d outofradius %d "
				"(nearest rejected %.1f m) "
				"| coarse rejected: notile %d outofradius %d | wantedgroups %d",
				WeightLadder.Tiers.Num(), ResidencyRadius * 0.01, UnloadRadius * 0.01,
				InvokerWindows.Num(), NumWindowSpanClamped, NumGroupsNoContent, NumGroupsOutOfRadius,
				NumGroupsOutOfRadius > 0 ? FMath::Sqrt(NearestRejectedDistSq) * 0.01 : -1.0,
				NumCoarseGroupsNoTile, NumCoarseGroupsOutOfRadius,
				OutGroups.Num());
		}
	}
	// Global texel origin (integer-valued) of a raster's bottom-left APRON texel, at a given
	// QuadsPerSide density: Key * QuadsPerSide - 1 (inner texel 0 lands at global Key * QuadsPerSide).
	// Shared by BOTH the height (Quads) and weight (WeightQuads) tiles below — a PURE function of Key
	// and QuadsPerSide, so two adjacent cells resolve the SAME global index for their shared border
	// texel at a uniform QuadsPerSide, the bit-identical-seam guarantee (plan I1). Extracted (weightmap-
	// decouple Stage 2 Step 8) as a deterministic test seam — QuadsPerSide need NOT be power-of-two.
	FIntPoint ComputeGlobalTexelOrigin(FArcTerrainCellKey Key, int32 QuadsPerSide)
	{
		// Widen the multiply to int64 BEFORE narrowing to the int32 FIntPoint: Key.X * QuadsPerSide
		// overflows int32 past ~+-8.4M cells at 256 quads, silently wrapping the shared global texel
		// index and reopening the seam. Computing in 64-bit removes the *multiply* overflow; the int32
		// *result* range (the cell-index ceiling) is the accepted cap (plan Step 1 / seam-precision
		// principle). The exact integer origin is transported downstream as FIntPoint (no float cast),
		// preserving the invariant-integer sum both neighbours canonicalize on for the bit-identical seam.
		const int64 OriginX = static_cast<int64>(Key.X) * QuadsPerSide - 1;
		const int64 OriginY = static_cast<int64>(Key.Y) * QuadsPerSide - 1;
		return FIntPoint(static_cast<int32>(OriginX), static_cast<int32>(OriginY));
	}

	// The terrain-local XY box a raster's samples cover, apron included — (Resolution - 1) texels from the
	// raster's global origin. The same rule ArcTerrainTileBake::TileGatherBounds applies on the tile
	// lattice, and the one a stamp gather has to use: the rectangle of the unit is not what gets written.
	FBox RasterGatherBounds(FIntPoint TexelOrigin, int32 Resolution, double TexelToLocal)
	{
		const double MinX = static_cast<double>(TexelOrigin.X) * TexelToLocal;
		const double MinY = static_cast<double>(TexelOrigin.Y) * TexelToLocal;
		const double Span = static_cast<double>(FMath::Max(Resolution - 1, 0)) * TexelToLocal;
		return FBox(FVector(MinX, MinY, -ArcTerrain::GCellZHalfExtent),
			FVector(MinX + Span, MinY + Span, ArcTerrain::GCellZHalfExtent));
	}

	// Half-texel map from footprint UV [0,1] to the texel centers of a Dims raster (vertex-centered
	// grid; same map as MeshPartitionMakeSectionChannels.usf's InUVScale/InUVBias). The map is
	// reconstruction-agnostic: the WEIGHT path samples it bilinearly (= TSampledScalarField2
	// BilinearSampleClamped), the HEIGHT path runs bicubic Catmull-Rom over the same texel centers
	// (see ArcTerrainComposite.usf ComputeStamp).
	void ComputeSampleMap(FIntPoint Dims, FVector2f& OutUVScale, FVector2f& OutUVBias)
	{
		const float DimX = static_cast<float>(FMath::Max(Dims.X, 1));
		const float DimY = static_cast<float>(FMath::Max(Dims.Y, 1));
		OutUVScale = FVector2f((DimX - 1.0f) / DimX, (DimY - 1.0f) / DimY);
		OutUVBias = FVector2f(0.5f / DimX, 0.5f / DimY);
	}

	// THE COMPOSITE RASTER AUTHORITY: quads per side of
	// EVERY cell's composited height tile (tile texels = this + 3, with the 1-texel apron per side).
	//
	// CAMERA-INDEPENDENT BY CONSTRUCTION — a pure function of the terrain Config. Nothing about where
	// an invoker stands, which cell is pinned, or how a cell renders may enter here. That is the whole
	// point of the step: the raster is the SHARED height source for the CBT preview, for collision AND
	// for the baked height section, so if it moved with the camera then the bake would be
	// against a moving input and its DDC key would have to fold the camera state to stay sound.
	//
	// The plan calls this "content/config-derived". Today it is config-derived ONLY: CellExtent over
	// TexelSize is the per-cell resolution and no content input exists yet (CBT-rebuild Phase 2's finer
	// per-cell detail composite would be the first — it would raise this above base, which every
	// consumer below is already written to survive: the section BuildKey folds the tile resolution, and
	// collision does not read this raster at all).
	int32 CompositeQuads(const FArcTerrainConfig& Config)
	{
		return Config.QuadsPerCell();
	}
	// THE WEIGHT-COMPOSITE RASTER AUTHORITY (weightmap-decouple Stage 1 Step 1): weight texels per side
	// of ONE CELL, DECOUPLED from CompositeQuads (the height/mesh raster). A high-res source weightmap is
	// composited at its own texel density regardless of vertex count:
	//   WeightQuads = clamp( round(CellSize / WeightTexelSizeWorld), HeightQuads, MaxWeightSliceSide - 3 )
	//               then capped at the largest native weight-source dimension present.
	// HeightQuads is the floor so weights are never COARSER than the mesh raster; 4093 = 4096 - 3, the
	// apron budget of the widest weight slice. AArcTerrain::ValidateGridInvariants reports a config over
	// that ceiling but corrects nothing, so a violating config still reaches the composite — the ceiling
	// stays here as the last place that keeps the composite inside a slice the atlas can hold.
	int32 ResolveWeightQuads(const FArcTerrainConfig& Config, const FArcTerrainState& State)
	{
		(void)Config;
		return State.ResolvedWeightQuads;
	}

#if WITH_EDITOR
	// Present-channel set (weightmap-decouple Stage 2 Step 3): every weight-layer ChannelIndex that a
	// covering stamp resolves against this terrain's channel set, de-duped. Pure Desc.Layers — the
	// texturing-only replacement for the deleted PretendLayers/metric machinery. Same
	// GatherSortedStamps + FindChannelIndex walk BuildCompositeJob uses to decide which weight passes to
	// emit, so the present set is exactly the channels the weight composite actually writes.
	void GatherPresentWeightChannels(const FArcTerrainConfig& Config,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		TArray<int32>& OutPresentChannels)
	{
		OutPresentChannels.Reset();
		if (Config.ChannelSet == nullptr)
		{
			return;
		}

		int32 StampsWithSourceDescriptor = 0;
		for (UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			FArcStampSourceDesc Desc;
			if (!Stamp->GetSourceDescriptor(Desc))
			{
				continue;
			}
			++StampsWithSourceDescriptor;
			for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
			{
				const int32 ChannelIndex = Config.ChannelSet->FindChannelIndex(LayerPair.Key);
				if (ChannelIndex != INDEX_NONE)
				{
					OutPresentChannels.AddUnique(ChannelIndex);
				}
			}
		}
		if (OutPresentChannels.Num() > 0 && Config.ChannelSet->Channels.IsValidIndex(ArcTerrain::GBaseChannelIndex))
		{
			OutPresentChannels.AddUnique(ArcTerrain::GBaseChannelIndex);
		}
	}

	// Present weight-layer SOURCE IDENTITIES (weightmap-decouple Stage 2 Step 5): one entry per (covering
	// stamp, Desc.Layers pair) that resolves to a valid terrain channel — the texturing-only weight-content
	// inputs ComputeCellWeightKey hashes (CompositeInputKey EXCLUDES weight-only layers). Same
	// GatherSortedStamps + FindChannelIndex walk as GatherPresentWeightChannels, but carrying each layer's
	// full decode identity, NOT de-duped (each layer is a distinct compositing source). Deterministic key
	// order: a STABLE sort by CHANNEL-NAME HASH, so reordering the channel set does not permute the hashed
	// sequence, while composite (z-order) order among layers sharing a channel is preserved and a z-order
	// swap that changes the composited result still changes the key.
	void GatherPresentWeightLayerIdentities(const FArcTerrainConfig& Config,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		TArray<ArcTerrainWeights::FArcTerrainWeightLayerIdentity>& OutIdentities)
	{
		OutIdentities.Reset();
		if (Config.ChannelSet == nullptr)
		{
			return;
		}

		for (UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			FArcStampSourceDesc Desc;
			if (!Stamp->GetSourceDescriptor(Desc))
			{
				continue;
			}
			for (const TPair<FName, FArcStampLayerDesc>& LayerPair : Desc.Layers)
			{
				const int32 ChannelIndex = Config.ChannelSet->FindChannelIndex(LayerPair.Key);
				if (ChannelIndex == INDEX_NONE)
				{
					continue;
				}
				const FArcStampLayerDesc& Layer = LayerPair.Value;
				ArcTerrainWeights::FArcTerrainWeightLayerIdentity& Identity = OutIdentities.AddDefaulted_GetRef();
				Identity.ChannelNameHash = ArcTerrain::ChannelNameHash(LayerPair.Key);
				Identity.ContentId = Layer.ContentId;
				Identity.SourceChannel = static_cast<uint8>(Layer.SourceChannel);
				Identity.EncodingScale = Layer.EncodingScale;
				Identity.ZeroInEncoding = Layer.ZeroInEncoding;
				Identity.SourceDims = Layer.SourceDims;
				Identity.Kind = Layer.Kind;
				Identity.MaskKind = Desc.MaskKind;
				Identity.MaskContentId = Desc.MaskContentId;
				Identity.MaskSourceDims = Desc.MaskSourceDims;
			}
		}
		OutIdentities.StableSort([](const ArcTerrainWeights::FArcTerrainWeightLayerIdentity& A,
			const ArcTerrainWeights::FArcTerrainWeightLayerIdentity& B)
		{
			return A.ChannelNameHash < B.ChannelNameHash;
		});
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
namespace ArcTerrainTileWrite_8a3f
{
	bool DropTile(const AArcTerrain& Terrain, const FString& TileDir, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey);
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainTileSeedLocal_c9f2
{
	void SaturatingAddCount(uint32& Value, uint32 Addend)
	{
		if (Addend == 0)
		{
			return;
		}
		if (Value > TNumericLimits<uint32>::Max() - Addend)
		{
			Value = TNumericLimits<uint32>::Max();
			return;
		}
		Value += Addend;
	}

	void AccumulateTileInvalidation(FArcTerrainState& State, const FArcTerrainBakeTileRef& TileRef,
		uint32 Count = 1)
	{
		uint32& NotificationCount = State.PendingTileInvalidations.FindOrAdd(TileRef);
		SaturatingAddCount(NotificationCount, Count);
	}

	void AccumulateAffectedTiles(FArcTerrainState& State, TConstArrayView<FArcTerrainBakeTileRef> Affected)
	{
		for (const FArcTerrainBakeTileRef& Ref : Affected)
		{
			AccumulateTileInvalidation(State, Ref);
		}
	}

	void FlushTileInvalidations(AArcTerrain& Terrain, FArcTerrainState& State)
	{
		TMap<FArcTerrainBakeTileRef, uint32> Invalidations = MoveTemp(State.PendingTileInvalidations);
		State.PendingTileInvalidations.Reset();

		bool bDataMayChange = false;
		for (const TPair<FArcTerrainBakeTileRef, uint32>& Pair : Invalidations)
		{
			const int32 RailIndex = static_cast<int32>(Pair.Key.Rail);
			if (Pair.Key.Rail == EArcTerrainRail::Data
				|| RailIndex < 0 || RailIndex >= ArcTerrain::NumTileRails)
			{
				bDataMayChange = true;
				break;
			}
		}

		if (bDataMayChange)
		{
			const bool bHadFingerprint =
				!State.DataMaskWindowFingerprint.IsZero() || State.DataWindowRevision != 0;
			double MaskWindowGatherSeconds = 0.0;
			const bool bFingerprintChanged =
				ArcTerrainDataMaps::RefreshDataMaskWindows(State, MaskWindowGatherSeconds);
			(void)MaskWindowGatherSeconds;
			if (bFingerprintChanged && bHadFingerprint)
			{
				++State.DataWindowRevision;

				TArray<FIntPoint> ManifestKeys;
				ArcTerrainTileAccess::GatherManifestTileKeys(Terrain, EArcTerrainRail::Data, 0, ManifestKeys);
				for (const FIntPoint& TileKey : ManifestKeys)
				{
					SaturatingAddCount(
						Invalidations.FindOrAdd(FArcTerrainBakeTileRef{EArcTerrainRail::Data, 0, TileKey}), 1);
				}
				for (const TPair<FArcTerrainBakeTileRef, TArray<TWeakObjectPtr<UArcTerrainStampComponent>>>& Pair
					: State.Contributors)
				{
					if (Pair.Key.Rail == EArcTerrainRail::Data && Pair.Key.Tier == 0)
					{
						SaturatingAddCount(Invalidations.FindOrAdd(Pair.Key), 1);
					}
				}
			}
		}

		for (const TPair<FArcTerrainBakeTileRef, uint32>& Pair : Invalidations)
		{
			const FArcTerrainBakeTileRef& TileRef = Pair.Key;
			const int32 RailIndex = static_cast<int32>(TileRef.Rail);
			if (RailIndex < 0 || RailIndex >= ArcTerrain::NumTileRails
				|| !ArcTerrainTileAccess::IsValidTier(TileRef.Tier))
			{
				continue;
			}

			FArcTerrainTileRailState& RailState = State.TileRails[RailIndex][TileRef.Tier];
			FArcTerrainTileDecision& Decision = RailState.Decisions.FindOrAdd(TileRef.Key);
			TSet<FIntPoint>& PendingKeys = State.PendingTiles(TileRef.Rail, TileRef.Tier);

			uint32 CapturedSerial = 0;
			bool bActiveWork = false;
			if (const FArcTerrainPendingTileBake* PendingBake = RailState.Pending.Find(TileRef.Key))
			{
				bActiveWork = true;
				CapturedSerial = PendingBake->DirtySerial;
			}
			else if (const FArcTerrainTileRailState::FWriteReservation* Reservation =
				RailState.WriteReservations.Find(TileRef.Key))
			{
				bActiveWork = true;
				CapturedSerial = Reservation->DirtySerial;
			}

			const bool bArmSuccessor =
				Decision.DecidedSerial == Decision.DirtySerial
				|| (bActiveWork && CapturedSerial == Decision.DirtySerial);
			if (bArmSuccessor)
			{
				if (bActiveWork)
				{
					Decision.DirtySerial = FMath::Max(Decision.DirtySerial, CapturedSerial) + 1;
				}
				else
				{
					++Decision.DirtySerial;
				}
			}

			RailState.ClearRetryDeferred(TileRef.Key, Decision);
			Decision.ReadyAfterBakePass = State.BakePass
				+ static_cast<uint64>(FMath::Max(State.BakeSettleTicks, 1));
			const bool bNewPending = !PendingKeys.Contains(TileRef.Key);
			PendingKeys.Add(TileRef.Key);
			if (bNewPending && TileRef.Rail == EArcTerrainRail::Height && TileRef.Tier == 0)
			{
				ArcTerrainResidencyIndex_c4e8::NotePendingHeightChanged(State, TileRef.Key);
			}
			if (!bActiveWork)
			{
				RailState.EnqueueReady(TileRef.Key);
			}
			SaturatingAddCount(Decision.CoalescedInvalidations, Pair.Value);
		}
	}

	void RebuildStampTileGraphFromCachedBounds(AArcTerrain& Terrain, FArcTerrainState& State);

	void SeedAllTileRails(const AArcTerrain& Terrain, FArcTerrainState& State)
	{
		const int32 WeightQuads = ArcTerrainSubsystemPrivate::ResolveWeightQuads(Terrain.Config, State);

		TArray<int32> HeightShifts;
		ArcTerrain::ResolveAuthoredHeightTierShifts(Terrain, HeightShifts);
		TArray<int32> WeightShifts;
		ArcTerrain::ResolveAuthoredWeightTierShifts(Terrain, WeightQuads, WeightShifts);

		const EArcTerrainRail Rails[3] = {
			EArcTerrainRail::Height,
			EArcTerrainRail::Weight,
			EArcTerrainRail::Data
		};
		const int32 TierCounts[3] = {
			HeightShifts.Num(),
			WeightShifts.Num(),
			1
		};

		TArray<FIntPoint> ManifestKeys;
		for (int32 RailIndex = 0; RailIndex < 3; ++RailIndex)
		{
			const EArcTerrainRail Rail = Rails[RailIndex];
			const int32 NumTiers = TierCounts[RailIndex];
			for (int32 Tier = 0; Tier < NumTiers; ++Tier)
			{
				FArcTerrainTileGrid Grid;
				if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, Rail, Tier, WeightQuads, Grid))
				{
					continue;
				}

				TSet<FIntPoint> TileKeys;
				ArcTerrainTileAccess::GatherManifestTileKeys(Terrain, Rail, Tier, ManifestKeys);
				TileKeys.Reserve(ManifestKeys.Num() + State.Contributors.Num());
				for (const FIntPoint& TileKey : ManifestKeys)
				{
					TileKeys.Add(TileKey);
				}
				for (const TPair<FArcTerrainBakeTileRef, TArray<TWeakObjectPtr<UArcTerrainStampComponent>>>& Pair
					: State.Contributors)
				{
					if (Pair.Key.Rail == Rail && Pair.Key.Tier == Tier)
					{
						TileKeys.Add(Pair.Key.Key);
					}
				}
				for (const FIntPoint& TileKey : TileKeys)
				{
					AccumulateTileInvalidation(State, FArcTerrainBakeTileRef{Rail, Tier, TileKey});
				}
			}
		}
	}

	void RebuildAndSeedAllTileRails(AArcTerrain& Terrain, FArcTerrainState& State)
	{
		RebuildStampTileGraphFromCachedBounds(Terrain, State);
		SeedAllTileRails(Terrain, State);
	}

	void DropRemovedAuthoredTiers(AArcTerrain& Terrain, FArcTerrainState& State, EArcTerrainRail Rail)
	{
		const int32 WeightQuads = ArcTerrainSubsystemPrivate::ResolveWeightQuads(Terrain.Config, State);
		const FString TileDir = ArcTerrainTileNames::MakeTileDirForWrite(Terrain);

		TArray<FIntPoint> ManifestKeys;
		TArray<FArcTerrainBakeTileRef> ContributorRefsToRemove;
		for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			FArcTerrainTileGrid Grid;
			if (ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, Rail, Tier, WeightQuads, Grid))
			{
				continue;
			}

			ArcTerrainTileAccess::GatherManifestTileKeys(Terrain, Rail, Tier, ManifestKeys);
			if (!TileDir.IsEmpty())
			{
				for (const FIntPoint& TileKey : ManifestKeys)
				{
					ArcTerrainTileWrite_8a3f::DropTile(Terrain, TileDir, Rail, Tier, TileKey);
				}
			}

			ContributorRefsToRemove.Reset();
			for (const TPair<FArcTerrainBakeTileRef, TArray<TWeakObjectPtr<UArcTerrainStampComponent>>>& Pair
				: State.Contributors)
			{
				if (Pair.Key.Rail == Rail && Pair.Key.Tier == Tier)
				{
					ContributorRefsToRemove.Add(Pair.Key);
				}
			}
			for (const FArcTerrainBakeTileRef& Ref : ContributorRefsToRemove)
			{
				State.Contributors.Remove(Ref);
			}
		}
	}
}
#endif
#if WITH_EDITOR
namespace ArcTerrainStampGraph_8b21
{
	void AddStampDependency(
		FArcTerrainState& State,
		const FArcTerrainBakeTileRef& Ref,
		UArcTerrainStampComponent& Stamp)
	{
		TArray<TWeakObjectPtr<UArcTerrainStampComponent>>& Contributors = State.Contributors.FindOrAdd(Ref);
		const bool bAlreadyPresent = Contributors.ContainsByPredicate(
			[&Stamp](const TWeakObjectPtr<UArcTerrainStampComponent>& Weak)
			{
				return Weak.Get() == &Stamp;
			});
		if (!bAlreadyPresent)
		{
			Contributors.Add(&Stamp);
		}
	}

	void RemoveStampDependency(
		FArcTerrainState& State,
		const FArcTerrainBakeTileRef& Ref,
		UArcTerrainStampComponent& Stamp)
	{
		TArray<TWeakObjectPtr<UArcTerrainStampComponent>>* Contributors = State.Contributors.Find(Ref);
		if (Contributors == nullptr)
		{
			return;
		}

		Contributors->RemoveAll(
			[&Stamp](const TWeakObjectPtr<UArcTerrainStampComponent>& Weak)
			{
				const UArcTerrainStampComponent* Existing = Weak.Get();
				return Existing == nullptr || Existing == &Stamp;
			});
		if (Contributors->IsEmpty())
		{
			State.Contributors.Remove(Ref);
		}
	}

	void UpdateStampTileDependencies(
		FArcTerrainState& State,
		AArcTerrain& Terrain,
		FArcTerrainStampEntry& Entry,
		TConstArrayView<FArcTerrainBakeTileRef> OldCoverage,
		TConstArrayView<FArcTerrainBakeTileRef> NewCoverage,
		TArray<FArcTerrainBakeTileRef>& OutAffected)
	{
		(void)Terrain;

		UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
		if (Stamp != nullptr)
		{
			TSet<FArcTerrainBakeTileRef> NewSet;
			NewSet.Reserve(NewCoverage.Num());
			for (const FArcTerrainBakeTileRef& Ref : NewCoverage)
			{
				NewSet.Add(Ref);
			}

			TSet<FArcTerrainBakeTileRef> OldSet;
			OldSet.Reserve(OldCoverage.Num());
			for (const FArcTerrainBakeTileRef& Ref : OldCoverage)
			{
				OldSet.Add(Ref);
			}

			for (const FArcTerrainBakeTileRef& Ref : OldCoverage)
			{
				if (!NewSet.Contains(Ref))
				{
					RemoveStampDependency(State, Ref, *Stamp);
				}
			}
			for (const FArcTerrainBakeTileRef& Ref : NewCoverage)
			{
				if (!OldSet.Contains(Ref))
				{
					AddStampDependency(State, Ref, *Stamp);
				}
			}
		}

		Entry.CoveredTiles.Reset(NewCoverage.Num());
		Entry.CoveredTiles.Append(NewCoverage.GetData(), NewCoverage.Num());

		OutAffected.Reset();
		TSet<FArcTerrainBakeTileRef> AffectedSet;
		AffectedSet.Reserve(OldCoverage.Num() + NewCoverage.Num());
		for (const FArcTerrainBakeTileRef& Ref : OldCoverage)
		{
			bool bAlready = false;
			AffectedSet.Add(Ref, &bAlready);
			if (!bAlready)
			{
				OutAffected.Add(Ref);
			}
		}
		for (const FArcTerrainBakeTileRef& Ref : NewCoverage)
		{
			bool bAlready = false;
			AffectedSet.Add(Ref, &bAlready);
			if (!bAlready)
			{
				OutAffected.Add(Ref);
			}
		}
	}
}

namespace ArcTerrainTileSeedLocal_c9f2
{
	void RebuildStampTileGraphFromCachedBounds(AArcTerrain& Terrain, FArcTerrainState& State)
	{
		State.Contributors.Reset();
		TArray<FArcTerrainBakeTileRef> UnusedAffected;
		for (FArcTerrainStampEntry& Entry : State.Stamps)
		{
			UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
			Entry.CoveredTiles.Reset();
			if (Stamp == nullptr)
			{
				continue;
			}

			TArray<FArcTerrainBakeTileRef> NewCoverage;
			ArcTerrainSubsystemPrivate::GatherBakeTileRefsForBounds(Terrain, State, Entry.Bounds, NewCoverage);
			ArcTerrainStampGraph_8b21::UpdateStampTileDependencies(
				State, Terrain, Entry, TConstArrayView<FArcTerrainBakeTileRef>(), NewCoverage, UnusedAffected);
		}
	}
}
#endif
