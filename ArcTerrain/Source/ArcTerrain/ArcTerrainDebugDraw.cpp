// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainDebugDraw.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTileFormat.h"
#include "ArcTerrainTypes.h"
#include "Render/ArcTerrainDensityTiers.h"

#include "UObject/ObjectKey.h"

DECLARE_CYCLE_STAT(TEXT("Debug draw overlay"), STAT_ArcTerrainDebugDraw, STATGROUP_ArcTerrain);

namespace ArcTerrainDebugDrawPrivate_2b64
{
#if UE_DEBUG_RENDERING_ENABLED

	constexpr FColor TileResident(0, 255, 0);
	constexpr FColor TileInFlight(255, 255, 0);
	constexpr FColor TileUnusable(255, 0, 0);
	constexpr FColor TileInManifest(60, 110, 255);
	constexpr FColor TileAbsent(40, 40, 40);
	constexpr FColor TileUnknown(255, 0, 255);
	constexpr FColor TileWindowTruncated(255, 0, 120);
	constexpr FColor CellResident(0, 255, 255);
	constexpr FColor CellNotResident(0, 90, 90);
	constexpr FColor RingPreview(255, 255, 255);
	constexpr FColor RingUnload(160, 160, 160);
	constexpr FColor PatchBuilt(255, 150, 0);
	constexpr FColor PatchNoHeightSource(255, 60, 60);
	constexpr FColor ManifestHeightBaked(150, 60, 255);
	constexpr FColor ManifestHeightUnusable(70, 28, 120);
	constexpr FColor ManifestWeightBaked(0, 200, 120);
	constexpr FColor ManifestWeightUnusable(0, 80, 48);
	constexpr FColor ManifestDataBaked(255, 130, 170);
	constexpr FColor ManifestDataUnusable(120, 45, 70);

	/** Keys either side of an invoker the tile layer walks: the overlay's own draw budget, in TILES, not a
	 *  bound the stream phase shares. A stream radius reaching past it truncates the drawn window. */
	constexpr int32 MaxTileDrawSpan = 4;

	/** Union windows past this many keys fall back to the first invoker's window alone. */
	constexpr int32 MaxTileDrawKeys = 256;

	/** Inset of the second rect drawn inside every boundary key of a truncated window, as a fraction of the
	 *  tile edge. */
	constexpr double TruncatedWindowEdgeMarkInsetFraction = 0.12;

	constexpr int32 RingSides = 64;

	constexpr float LineThickness = 0.0f;

	/** Which tile rails a layer draws; bit i is EArcTerrainRail i. */
	struct FRailMask
	{
		uint8 Bits = 0;

		bool Has(int32 RailIndex) const { return (Bits & (1u << RailIndex)) != 0u; }

		bool IsEmpty() const { return Bits == 0; }
	};

	FRailMask MakeRailMask(bool bHeight, bool bWeight, bool bData)
	{
		FRailMask Mask;
		Mask.Bits = static_cast<uint8>((bHeight ? 1 : 0) | (bWeight ? 2 : 0) | (bData ? 4 : 0));
		return Mask;
	}

	struct FRailRectCounts
	{
		int32 Rects[ArcTerrain::NumTileRails] = {};
	};

	FColor ManifestColor(int32 RailIndex, bool bUnusable)
	{
		switch (static_cast<EArcTerrainRail>(RailIndex))
		{
		case EArcTerrainRail::Weight:
			return bUnusable ? ManifestWeightUnusable : ManifestWeightBaked;
		case EArcTerrainRail::Data:
			return bUnusable ? ManifestDataUnusable : ManifestDataBaked;
		default:
			return bUnusable ? ManifestHeightUnusable : ManifestHeightBaked;
		}
	}

	FColor TileColor(EArcTerrainTileResidency State)
	{
		switch (State)
		{
		case EArcTerrainTileResidency::Resident:
			return TileResident;
		case EArcTerrainTileResidency::InFlight:
			return TileInFlight;
		case EArcTerrainTileResidency::Unusable:
			return TileUnusable;
		case EArcTerrainTileResidency::InManifest:
			return TileInManifest;
		case EArcTerrainTileResidency::Absent:
			return TileAbsent;
		default:
			return TileUnknown;
		}
	}

	constexpr float TierBrightness[ArcTerrain::MaxTiers] = { 1.0f, 0.72f, 0.52f, 0.38f };

	FColor ApplyTierBrightness(FColor Base, int32 Tier)
	{
		const float Factor = TierBrightness[FMath::Clamp(Tier, 0, ArcTerrain::MaxTiers - 1)];
		if (Factor >= 1.0f)
		{
			return Base;
		}
		return FColor(static_cast<uint8>(Base.R * Factor), static_cast<uint8>(Base.G * Factor),
			static_cast<uint8>(Base.B * Factor), Base.A);
	}

	void AppendRect(TArray<UE::DebugDraw::FLineSubmission>& OutLines, const FTransform& TerrainTransform,
		double MinX, double MinY, double MaxX, double MaxY, double LocalZ, const FColor& Color)
	{
		const FVector Corners[4] = {
			TerrainTransform.TransformPosition(FVector(MinX, MinY, LocalZ)),
			TerrainTransform.TransformPosition(FVector(MaxX, MinY, LocalZ)),
			TerrainTransform.TransformPosition(FVector(MaxX, MaxY, LocalZ)),
			TerrainTransform.TransformPosition(FVector(MinX, MaxY, LocalZ)),
		};
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			UE::DebugDraw::FLineSubmission& Line = OutLines.AddDefaulted_GetRef();
			Line.Start = Corners[Corner];
			Line.End = Corners[(Corner + 1) & 3];
			Line.Color = Color;
			Line.Thickness = LineThickness;
			Line.Depth = UE::DebugDraw::EDepthTest::AlwaysOnTop;
		}
	}

	FVector RectCentre(const FTransform& TerrainTransform, double MinX, double MinY, double MaxX,
		double MaxY, double LocalZ)
	{
		return TerrainTransform.TransformPosition(
			FVector((MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5, LocalZ));
	}

	void DrawTilesOneTier(const FSceneInterface* Scene, const AArcTerrain& Terrain,
		const FTransform& TerrainTransform, double LocalZ,
		EArcTerrainRail Rail, int32 Tier, double StreamWindowRadius, TConstArrayView<FVector> InvokerPositions,
		bool bLabels, TArray<UE::DebugDraw::FLineSubmission>& OutLines, int32& OutRectCount,
		int32& OutLabelsSubmitted)
	{
		using ArcTerrainSubsystemPrivate::ComputeSectorWindow;
		using ArcTerrainSubsystemPrivate::FSectorWindow;

		FArcTerrainTileGrid Grid;
		if (!ArcTerrainTileAccess::ResolveGrid(Terrain, Rail, Tier, Grid))
		{
			return;
		}
		const int32 RailLinesBefore = OutLines.Num();
		const double TileSize = static_cast<double>(Grid.TileExtentM) * 100.0;

		FSectorWindow Window;
		FSectorWindow FirstWindow;
		bool bHasWindow = false;
		bool bWindowTruncated = false;
		for (const FVector& Invoker : InvokerPositions)
		{
			const FSectorWindow Other = ComputeSectorWindow(Invoker, StreamWindowRadius, 0, TileSize,
				TerrainTransform, static_cast<double>(MaxTileDrawSpan));
			if (Other.MaxX < Other.MinX || Other.MaxY < Other.MinY)
			{
				continue;
			}
			bWindowTruncated |= Other.bSpanClamped;
			if (!bHasWindow)
			{
				bHasWindow = true;
				FirstWindow = Other;
				Window = Other;
				continue;
			}
			Window.MinX = FMath::Min(Window.MinX, Other.MinX);
			Window.MinY = FMath::Min(Window.MinY, Other.MinY);
			Window.MaxX = FMath::Max(Window.MaxX, Other.MaxX);
			Window.MaxY = FMath::Max(Window.MaxY, Other.MaxY);
		}
		if (!bHasWindow)
		{
			return;
		}
		const int64 Keys = (static_cast<int64>(Window.MaxX) - Window.MinX + 1)
			* (static_cast<int64>(Window.MaxY) - Window.MinY + 1);
		if (Keys > static_cast<int64>(MaxTileDrawKeys))
		{
			// Invokers far enough apart to blow the budget: the first one's own window, not a union
			// spanning the empty ground between them.
			Window = FirstWindow;
			bWindowTruncated = true;
		}

		TArray<EArcTerrainTileResidency> States;
		ArcTerrainTileAccess::GatherTileResidency(Terrain, Rail, Tier, FIntPoint(Window.MinX, Window.MinY),
			FIntPoint(Window.MaxX, Window.MaxY), States);
		if (States.IsEmpty())
		{
			return;
		}

		const int32 Width = Window.MaxX - Window.MinX + 1;
		for (int32 Y = Window.MinY; Y <= Window.MaxY; ++Y)
		{
			for (int32 X = Window.MinX; X <= Window.MaxX; ++X)
			{
				const int32 Slot = (Y - Window.MinY) * Width + (X - Window.MinX);
				const FColor Color = ApplyTierBrightness(TileColor(States[Slot]), Tier);
				const double MinX = static_cast<double>(X) * TileSize;
				const double MinY = static_cast<double>(Y) * TileSize;
				const double MaxX = static_cast<double>(X + 1) * TileSize;
				const double MaxY = static_cast<double>(Y + 1) * TileSize;
				AppendRect(OutLines, TerrainTransform, MinX, MinY, MaxX, MaxY, LocalZ, Color);
				if (bWindowTruncated
					&& (X == Window.MinX || X == Window.MaxX || Y == Window.MinY || Y == Window.MaxY))
				{
					const double Mark = TileSize * TruncatedWindowEdgeMarkInsetFraction;
					AppendRect(OutLines, TerrainTransform, MinX + Mark, MinY + Mark, MaxX - Mark,
						MaxY - Mark, LocalZ, TileWindowTruncated);
				}
				if (bLabels)
				{
					const FString Label = Tier == 0
						? FString::Printf(TEXT("%s %d,%d"), ArcTerrainDiag::RailName(Rail), X, Y)
						: FString::Printf(TEXT("%s t%d %d,%d"), ArcTerrainDiag::RailName(Rail), Tier, X, Y);
					UE::DebugDraw::Text(Scene, RectCentre(TerrainTransform, MinX, MinY, MaxX, MaxY, LocalZ), Label,
						Color);
					++OutLabelsSubmitted;
				}
			}
		}
		OutRectCount += (OutLines.Num() - RailLinesBefore) / 4;
	}

	void DrawTiles(const FSceneInterface* Scene, const AArcTerrain& Terrain,
		const FTransform& TerrainTransform, double LocalZ,
		const FArcTerrainTierLadder& Ladder, TConstArrayView<FVector> InvokerPositions, FRailMask Rails,
		bool bLabels, TArray<UE::DebugDraw::FLineSubmission>& OutLines, FRailRectCounts& OutRects,
		int32& OutLabelsSubmitted)
	{
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			if (!Rails.Has(RailIndex))
			{
				continue;
			}
			const EArcTerrainRail Rail = static_cast<EArcTerrainRail>(RailIndex);
			const int32 NumTiers = Rail == EArcTerrainRail::Height ? FMath::Max(Ladder.Tiers.Num(), 1) : 1;
			for (int32 Tier = 0; Tier < NumTiers; ++Tier)
			{
				const double StreamWindowRadius = Ladder.Tiers.IsValidIndex(Tier)
					? Ladder.Tiers[Tier].StreamUnloadRadiusCm
					: 0.0;
				DrawTilesOneTier(Scene, Terrain, TerrainTransform, LocalZ, Rail, Tier, StreamWindowRadius,
					InvokerPositions, bLabels, OutLines, OutRects.Rects[RailIndex], OutLabelsSubmitted);
			}
		}
	}

	void DrawCells(const FSceneInterface* Scene, const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FTransform& TerrainTransform, double LocalZ, bool bLabels,
		TArray<UE::DebugDraw::FLineSubmission>& OutLines, int32& OutLabelsSubmitted)
	{
		for (const FArcTerrainCellHot& Hot : State.HotCells)
		{
			const FBox Bounds = ArcTerrain::CellLocalBounds(Hot.Key, Terrain.Config);
			if (!Bounds.IsValid)
			{
				continue;
			}
			const FColor Color = Hot.bResident != 0 ? CellResident : CellNotResident;
			AppendRect(OutLines, TerrainTransform, Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y,
				LocalZ, Color);
			if (bLabels)
			{
				UE::DebugDraw::Text(Scene,
					RectCentre(TerrainTransform, Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y,
						LocalZ),
					FString::Printf(TEXT("c %d,%d"), Hot.Key.X, Hot.Key.Y), Color);
				++OutLabelsSubmitted;
			}
		}
	}

	void DrawCollision(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FTransform& TerrainTransform, double LocalZ,
		TArray<UE::DebugDraw::FLineSubmission>& OutLines)
	{
		const double PatchSize = Terrain.CollisionPatchSize();
		if (PatchSize <= 0.0)
		{
			return;
		}
		for (const TPair<FArcTerrainCollisionKey, FArcTerrainCollisionPatch>& PatchPair :
			State.CollisionPatches)
		{
			const FBox Bounds = ArcTerrain::CollisionPatchLocalBounds(PatchPair.Key, PatchSize);
			if (!Bounds.IsValid)
			{
				continue;
			}
			const FColor Color = PatchPair.Value.bCollisionHeightSourceMissing
				? PatchNoHeightSource
				: PatchBuilt;
			AppendRect(OutLines, TerrainTransform, Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y,
				LocalZ, Color);
		}
	}

	int32 RailTileExtentM(const FArcTerrainConfig& Config, EArcTerrainRail Rail)
	{
		return static_cast<int32>(
			static_cast<uint32>(ArcTerrainTileFormat::RailTileExtent(Config, Rail)));
	}

	struct FCoverageCache
	{
		TArray<UE::DebugDraw::FLineSubmission> Lines;
		FTransform Transform;
		FRailMask Rails;
		FRailRectCounts Counts;
		double LocalZ = 0.0;
		int32 TileExtentM[ArcTerrain::NumTileRails] = {};
		int32 HeightTierExtentM[ArcTerrain::MaxTiers] = {};
		int32 NumHeightTiers = 1;
		uint32 Epoch = 0;
		int32 Entries = 0;
		int32 Dropped = 0;
		bool bBuilt = false;
	};

	TMap<FObjectKey, FCoverageCache> GCoverageCache;

	TMap<FObjectKey, double> GLastDiagLogSeconds;

	void BuildManifestCoverage(const AArcTerrain& Terrain, const FTransform& TerrainTransform, double LocalZ,
		FCoverageCache& Cache)
	{
		TArray<FArcTerrainManifestTile> Tiles;
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			if (!Cache.Rails.Has(RailIndex))
			{
				continue;
			}
			const EArcTerrainRail Rail = static_cast<EArcTerrainRail>(RailIndex);
			const int32 NumTiers = Rail == EArcTerrainRail::Height ? Cache.NumHeightTiers : 1;
			for (int32 Tier = 0; Tier < NumTiers; ++Tier)
			{
				const double TileSize = Tier == 0
					? static_cast<double>(Cache.TileExtentM[RailIndex]) * 100.0
					: static_cast<double>(Cache.HeightTierExtentM[Tier]) * 100.0;
				if (TileSize <= 0.0)
				{
					continue;
				}

				const int32 Total = ArcTerrainTileAccess::GatherManifestCoverage(Terrain, Rail, Tier, Tiles);
				if (Tier == 0)
				{
					Cache.Counts.Rects[RailIndex] = Tiles.Num();
				}
				else
				{
					Cache.Counts.Rects[RailIndex] += Tiles.Num();
				}
				Cache.Entries += Total;
				const int32 Dropped = Total - Tiles.Num();
				Cache.Dropped += Dropped;

				for (const FArcTerrainManifestTile& Tile : Tiles)
				{
					const double MinX = static_cast<double>(Tile.TileKey.X) * TileSize;
					const double MinY = static_cast<double>(Tile.TileKey.Y) * TileSize;
					const double MaxX = static_cast<double>(Tile.TileKey.X + 1) * TileSize;
					const double MaxY = static_cast<double>(Tile.TileKey.Y + 1) * TileSize;
					AppendRect(Cache.Lines, TerrainTransform, MinX, MinY, MaxX, MaxY, LocalZ,
						ApplyTierBrightness(ManifestColor(RailIndex, Tile.bUnusable), Tier));
				}
			}
		}
	}

	/** A terrain destroyed with the layer on leaves its geometry behind, and a rebuild is the only moment
	 *  that costs nothing to walk. */
	void PruneCoverageCaches()
	{
		for (TMap<FObjectKey, FCoverageCache>::TIterator It(GCoverageCache); It; ++It)
		{
			if (It.Key().ResolveObjectPtr() == nullptr)
			{
				It.RemoveCurrent();
			}
		}
		for (TMap<FObjectKey, double>::TIterator It(GLastDiagLogSeconds); It; ++It)
		{
			if (It.Key().ResolveObjectPtr() == nullptr)
			{
				It.RemoveCurrent();
			}
		}
	}

	/** Manifest coverage does not change per frame, so the lines are built once and re-submitted. The
	 *  epoch covers every store-side change; the transform, Z and per-rail tile extents cover the geometry
	 *  they are baked into — all read without touching the tile store. The enabled-rail set is part of the
	 *  key because switching a rail on moves none of the others: a hit would re-submit a line set that
	 *  never contained that rail. */
	const FCoverageCache& UpdateCoverageCache(const AArcTerrain& Terrain, const FTransform& TerrainTransform,
		double LocalZ, FRailMask Rails)
	{
		const uint32 Epoch = ArcTerrainTileAccess::GetManifestCoverageEpoch();
		int32 Extents[ArcTerrain::NumTileRails] = {};
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			Extents[RailIndex] =
				RailTileExtentM(Terrain.Config, static_cast<EArcTerrainRail>(RailIndex));
		}

		int32 TierExtents[ArcTerrain::MaxTiers] = {};
		int32 NumHeightTiers = 1;
		for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			FArcTerrainTileGrid TierGrid;
			if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, Tier, TierGrid))
			{
				break;
			}
			TierExtents[Tier] = TierGrid.TileExtentM;
			NumHeightTiers = Tier + 1;
		}

		const FObjectKey TerrainKey(&Terrain);
		if (const FCoverageCache* Existing = GCoverageCache.Find(TerrainKey))
		{
			bool bSameExtents = Existing->NumHeightTiers == NumHeightTiers;
			for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
			{
				bSameExtents = bSameExtents && Existing->TileExtentM[RailIndex] == Extents[RailIndex];
			}
			for (int32 Tier = 1; Tier < NumHeightTiers; ++Tier)
			{
				bSameExtents = bSameExtents && Existing->HeightTierExtentM[Tier] == TierExtents[Tier];
			}
			if (Existing->bBuilt && Existing->Epoch == Epoch && Existing->LocalZ == LocalZ && bSameExtents
				&& Existing->Rails.Bits == Rails.Bits && Existing->Transform.Equals(TerrainTransform))
			{
				return *Existing;
			}
		}

		PruneCoverageCaches();

		FCoverageCache& Cache = GCoverageCache.FindOrAdd(TerrainKey);
		Cache.Lines.Reset();
		Cache.Entries = 0;
		Cache.Dropped = 0;
		Cache.Counts = FRailRectCounts();
		Cache.Rails = Rails;
		Cache.Transform = TerrainTransform;
		Cache.LocalZ = LocalZ;
		FMemory::Memcpy(Cache.TileExtentM, Extents, sizeof(Extents));
		FMemory::Memcpy(Cache.HeightTierExtentM, TierExtents, sizeof(TierExtents));
		Cache.NumHeightTiers = NumHeightTiers;
		Cache.Epoch = Epoch;
		Cache.bBuilt = true;
		BuildManifestCoverage(Terrain, TerrainTransform, LocalZ, Cache);
		return Cache;
	}

	void DrawRings(const FSceneInterface* Scene, const FTransform& TerrainTransform, double LocalZ,
		const FArcTerrainTierLadder& Ladder,
		double BaseUnloadRadiusCm, TConstArrayView<FVector> InvokerPositions)
	{
		// WORLD XY, not the terrain plane: the radii gate RectInvokerDistSq, which discards Z. A ring tilted
		// with a pitched terrain would draw an admission boundary the phases do not use.
		for (const FVector& Invoker : InvokerPositions)
		{
			FVector Local = TerrainTransform.InverseTransformPosition(Invoker);
			Local.Z = LocalZ;
			const FVector GroundUnderInvoker = TerrainTransform.TransformPosition(Local);
			const FVector Centre(Invoker.X, Invoker.Y, GroundUnderInvoker.Z);
			for (int32 Tier = 0; Tier < Ladder.Tiers.Num(); ++Tier)
			{
				const FResolvedTier& TierEntry = Ladder.Tiers[Tier];
				const bool bLastTier = Tier == Ladder.Tiers.Num() - 1;
				// The gates from ArcTerrainSubsystem's residency walk, term for term.
				const double AdmitRadius = TierEntry.BandOuterRadiusCm;
				const double UnloadRadius = !bLastTier
					? AdmitRadius
					: (Tier == 0 ? BaseUnloadRadiusCm : TierEntry.BandUnloadRadiusCm);
				const FColor PreviewColor = ApplyTierBrightness(RingPreview, Tier);
				const FColor UnloadColor = ApplyTierBrightness(RingUnload, Tier);
				UE::DebugDraw::Circle(Scene, Centre, FVector::UpVector, static_cast<float>(AdmitRadius), RingSides,
					PreviewColor, LineThickness);
				if (UnloadRadius > AdmitRadius)
				{
					UE::DebugDraw::Circle(Scene, Centre, FVector::UpVector, static_cast<float>(UnloadRadius),
						RingSides, UnloadColor, LineThickness);
				}
			}
		}
	}

#endif // UE_DEBUG_RENDERING_ENABLED
}

namespace ArcTerrainDebugDraw
{
	void DrawTerrain(const AArcTerrain& Terrain, const FArcTerrainState& State,
		TConstArrayView<FVector> InvokerPositions)
	{
#if UE_DEBUG_RENDERING_ENABLED
		using namespace ArcTerrainDebugDrawPrivate_2b64;

		if (!Terrain.HasDebugOverlayEnabled())
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_ArcTerrainDebugDraw);

		const FTransform TerrainTransform = Terrain.GetTerrainTransform();
		const double LocalZ = static_cast<double>(Terrain.HeightOptions.BaseHeightCm);
		const bool bLabels = Terrain.bDrawKeyLabels;

		const FSceneInterface* Scene = UE::DebugDraw::GetSceneKey(Terrain.GetWorld());

		TArray<UE::DebugDraw::FLineSubmission> Lines;
		Lines.Reserve(1024);

		const FRailMask StreamingRails = MakeRailMask(Terrain.bDrawHeightTileResidency,
			Terrain.bDrawWeightTileResidency, Terrain.bDrawDataTileResidency);
		const FRailMask ManifestRails = MakeRailMask(Terrain.bDrawBakedHeightTiles,
			Terrain.bDrawBakedWeightTiles, Terrain.bDrawBakedDataTiles);

		int32 TileLabels = 0;
		int32 CellLabels = 0;
		FRailRectCounts TileRects;
		FRailRectCounts ManifestRects;
		int32 CellRects = 0;
		int32 PatchRects = 0;
		int32 ManifestEntries = 0;
		int32 ManifestDropped = 0;

		if (!StreamingRails.IsEmpty() && !InvokerPositions.IsEmpty())
		{
			const ArcTerrainSubsystemPrivate::FResolvedResidency Residency =
				ArcTerrainSubsystemPrivate::ResolveResidency(Terrain);
			const FArcTerrainTileAtlasLayout Layout =
				ArcTerrainSubsystemPrivate::ResolveTerrainAtlasLayout(Terrain, State, Residency);
			const FArcTerrainTierLadder Ladder =
				ArcTerrain::ResolveTierLadder(Terrain, Layout);
			DrawTiles(Scene, Terrain, TerrainTransform, LocalZ, Ladder, InvokerPositions, StreamingRails, bLabels,
				Lines, TileRects, TileLabels);
		}
		if (Terrain.bDrawCellResidency)
		{
			const int32 LinesBefore = Lines.Num();
			DrawCells(Scene, Terrain, State, TerrainTransform, LocalZ, bLabels, Lines, CellLabels);
			CellRects = (Lines.Num() - LinesBefore) / 4;
		}
		if (Terrain.bDrawCollisionPatches)
		{
			const int32 LinesBefore = Lines.Num();
			DrawCollision(Terrain, State, TerrainTransform, LocalZ, Lines);
			PatchRects = (Lines.Num() - LinesBefore) / 4;
		}
		if (!ManifestRails.IsEmpty())
		{
			const FCoverageCache& Coverage =
				UpdateCoverageCache(Terrain, TerrainTransform, LocalZ, ManifestRails);
			Lines.Append(Coverage.Lines);
			ManifestRects = Coverage.Counts;
			ManifestEntries = Coverage.Entries;
			ManifestDropped = Coverage.Dropped;
		}
		else
		{
			GCoverageCache.Remove(FObjectKey(&Terrain));
		}
		if (Terrain.bDrawResidencyRing)
		{
			const ArcTerrainSubsystemPrivate::FResolvedResidency Residency =
				ArcTerrainSubsystemPrivate::ResolveResidency(Terrain);
			const FArcTerrainTileAtlasLayout RingLayout =
				ArcTerrainSubsystemPrivate::ResolveTerrainAtlasLayout(Terrain, State, Residency);
			const FArcTerrainTierLadder RingLadder =
				ArcTerrain::ResolveTierLadder(Terrain, RingLayout);
			DrawRings(Scene, TerrainTransform, LocalZ, RingLadder, Residency.PreviewUnloadRadius, InvokerPositions);
		}

		const double Now = FPlatformTime::Seconds();
		double& LastLog = GLastDiagLogSeconds.FindOrAdd(FObjectKey(&Terrain), 0.0);
		if (Now - LastLog >= static_cast<double>(ArcTerrainDiag::GDiagIntervalSeconds))
		{
			LastLog = Now;
			UE_LOGF(LogArcTerrainDiag, Log,
				"[DebugDraw] %ls flags(tileH=%d tileW=%d tileD=%d Cell=%d Collision=%d Ring=%d "
				"Labels=%d bakedH=%d bakedW=%d bakedD=%d) "
				"HotCells=%d Cells=%d CollisionPatches=%d PatchSize=%.1f "
				"rects(tileH=%d tileW=%d tileD=%d cells=%d patches=%d bakedH=%d bakedW=%d bakedD=%d) "
				"manifest(entries=%d notDrawn=%d) labels(tiles=%d cells=%d) totalLines=%d",
				*Terrain.GetActorNameOrLabel(), Terrain.bDrawHeightTileResidency ? 1 : 0,
				Terrain.bDrawWeightTileResidency ? 1 : 0, Terrain.bDrawDataTileResidency ? 1 : 0,
				Terrain.bDrawCellResidency ? 1 : 0, Terrain.bDrawCollisionPatches ? 1 : 0,
				Terrain.bDrawResidencyRing ? 1 : 0, Terrain.bDrawKeyLabels ? 1 : 0,
				Terrain.bDrawBakedHeightTiles ? 1 : 0, Terrain.bDrawBakedWeightTiles ? 1 : 0,
				Terrain.bDrawBakedDataTiles ? 1 : 0, State.HotCells.Num(),
				State.Cells.Num(), State.CollisionPatches.Num(), Terrain.CollisionPatchSize(),
				TileRects.Rects[0], TileRects.Rects[1], TileRects.Rects[2], CellRects, PatchRects,
				ManifestRects.Rects[0], ManifestRects.Rects[1], ManifestRects.Rects[2], ManifestEntries,
				ManifestDropped, TileLabels, CellLabels, Lines.Num());
		}

		UE::DebugDraw::Lines(Scene, Lines);
#endif // UE_DEBUG_RENDERING_ENABLED
	}

	void EvictCoverageCache(const AArcTerrain& Terrain)
	{
#if UE_DEBUG_RENDERING_ENABLED
		using namespace ArcTerrainDebugDrawPrivate_2b64;

		GCoverageCache.Remove(FObjectKey(&Terrain));
		GLastDiagLogSeconds.Remove(FObjectKey(&Terrain));
#endif // UE_DEBUG_RENDERING_ENABLED
	}
}
