// Copyright Lukasz Baran. All Rights Reserved.

#include "Site/ArcTownSiteSampler.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"
#include "Components/BoxComponent.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Surface/ArcTerrainSurfaceSample.h"
#include "Town/ArcTownSiteSnapshot.h"
#include "Town/ArcTownVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownSite, Log, All);

namespace ArcTownSiteSamplerPrivate
{
	/** Cosine tolerance for "the volume's local +Z is the terrain's local +Z". */
	constexpr double VerticalAlignmentTolerance = 1.0e-4;

	/** Per-axis cell span above which a terrain is rejected as a sampling candidate. */
	constexpr int64 MaxTerrainCellAxisSpan = 1000000000;

	/** Registry lookups the built-cell ranking scan may spend per axis; a wider rectangle is probed on a stride. */
	constexpr int64 MaxTerrainScanSamplesPerAxis = 256;

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsFiniteVector3f(const FVector3f& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		UE_LOGF(LogArcTownSite, Error,"%ls", *Message);
		return false;
	}

	/** True when a vertical line in volume-local space is also vertical in terrain-local space. */
	bool IsVerticalAxisAligned(const FTransform& ActorTransform, const FTransform& TerrainTransform)
	{
		const FVector VolumeUpWorld = ActorTransform.TransformVectorNoScale(FVector::UpVector);
		const FVector VolumeUpLocal = TerrainTransform.InverseTransformVectorNoScale(VolumeUpWorld).GetSafeNormal();
		return VolumeUpLocal.Z >= 1.0 - VerticalAlignmentTolerance;
	}

	/** Inclusive terrain-cell rectangle covering a world AABB. */
	bool TryComputeTerrainCellRect(const FBox& WorldBounds, const FTransform& TerrainTransform, const double CellSize,
		FIntPoint& OutMinCell, FIntPoint& OutMaxCell)
	{
		const FBox LocalBounds = WorldBounds.InverseTransformBy(TerrainTransform);
		if (!LocalBounds.IsValid || !IsFiniteVector(LocalBounds.Min) || !IsFiniteVector(LocalBounds.Max))
		{
			return false;
		}

		const double MinRatioX = FMath::Floor(LocalBounds.Min.X / CellSize);
		const double MaxRatioX = FMath::Floor(LocalBounds.Max.X / CellSize);
		const double MinRatioY = FMath::Floor(LocalBounds.Min.Y / CellSize);
		const double MaxRatioY = FMath::Floor(LocalBounds.Max.Y / CellSize);
		if (!FMath::IsFinite(MinRatioX) || !FMath::IsFinite(MaxRatioX)
			|| !FMath::IsFinite(MinRatioY) || !FMath::IsFinite(MaxRatioY)
			|| FMath::Abs(MinRatioX) > static_cast<double>(MAX_int32) || FMath::Abs(MaxRatioX) > static_cast<double>(MAX_int32)
			|| FMath::Abs(MinRatioY) > static_cast<double>(MAX_int32) || FMath::Abs(MaxRatioY) > static_cast<double>(MAX_int32))
		{
			return false;
		}

		OutMinCell = FIntPoint(static_cast<int32>(MinRatioX), static_cast<int32>(MinRatioY));
		OutMaxCell = FIntPoint(static_cast<int32>(MaxRatioX), static_cast<int32>(MaxRatioY));

		const int64 SpanX = static_cast<int64>(OutMaxCell.X) - static_cast<int64>(OutMinCell.X) + 1;
		const int64 SpanY = static_cast<int64>(OutMaxCell.Y) - static_cast<int64>(OutMinCell.Y) + 1;
		return SpanX > 0 && SpanY > 0 && SpanX <= MaxTerrainCellAxisSpan && SpanY <= MaxTerrainCellAxisSpan;
	}

	/** Product of two non-negative counts, clamped instead of overflowing. */
	int64 SaturatingProduct(const int64 A, const int64 B)
	{
		if (A == 0 || B == 0)
		{
			return 0;
		}
		return A > TNumericLimits<int64>::Max() / B ? TNumericLimits<int64>::Max() : A * B;
	}

	/**
	 * Cells inside an inclusive rectangle that carry a built section. A rectangle wider than the sample
	 * budget on either axis is probed on a stride and the hits are scaled back to cell units, which
	 * bounds the registry lookups at the cost of an estimate.
	 */
	int64 CountBuiltCells(const AArcTerrain& Terrain, const FIntPoint& MinCell, const FIntPoint& MaxCell)
	{
		const int64 SpanX = static_cast<int64>(MaxCell.X) - static_cast<int64>(MinCell.X) + 1;
		const int64 SpanY = static_cast<int64>(MaxCell.Y) - static_cast<int64>(MinCell.Y) + 1;
		if (SpanX <= 0 || SpanY <= 0)
		{
			return 0;
		}

		const int64 StrideX = FMath::DivideAndRoundUp(SpanX, MaxTerrainScanSamplesPerAxis);
		const int64 StrideY = FMath::DivideAndRoundUp(SpanY, MaxTerrainScanSamplesPerAxis);

		int64 HitCount = 0;
		for (int64 CellY = MinCell.Y; CellY <= static_cast<int64>(MaxCell.Y); CellY += StrideY)
		{
			for (int64 CellX = MinCell.X; CellX <= static_cast<int64>(MaxCell.X); CellX += StrideX)
			{
				const FArcTerrainCellKey Key(static_cast<int32>(CellX), static_cast<int32>(CellY));
				FIntPoint TileKey = FIntPoint::ZeroValue;
				if (ArcTerrainTileAccess::TileKeyFromCell(Terrain, EArcTerrainRail::Height, 0, Key, TileKey)
					&& ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Height, 0, TileKey))
				{
					++HitCount;
				}
			}
		}

		return SaturatingProduct(HitCount, SaturatingProduct(StrideX, StrideY));
	}

	/**
	 * Terrain the samples come from. Terrains are ranked by how many built cells they contribute under
	 * the volume, ties broken on path name, so a world holding several overlapping terrains always
	 * samples against the same one.
	 */
	AArcTerrain* SelectTerrain(UWorld& World, const FBox& WorldVolumeBounds, const FTransform& ActorTransform, const FString& VolumeName)
	{
		AArcTerrain* Selected = nullptr;
		int64 SelectedBuiltCellCount = 0;
		FString SelectedPathName;

		for (TActorIterator<AArcTerrain> TerrainIt(&World); TerrainIt; ++TerrainIt)
		{
			AArcTerrain* Terrain = *TerrainIt;
			if (!IsValid(Terrain))
			{
				continue;
			}

			const FArcTerrainConfig& Config = Terrain->Config;
			if (!FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
			{
				UE_LOGF(LogArcTownSite, Warning,"ArcTown site sampling for '%ls' rejected terrain '%ls': CellSize %.3f is not finite and positive.",
					*VolumeName, *Terrain->GetName(), Config.CellSize());
				continue;
			}

			const FTransform TerrainTransform = Terrain->GetTerrainTransform();
			if (!IsVerticalAxisAligned(ActorTransform, TerrainTransform))
			{
				UE_LOGF(LogArcTownSite, Warning,"ArcTown site sampling for '%ls' rejected terrain '%ls': its vertical axis does not align with the volume's.",
					*VolumeName, *Terrain->GetName());
				continue;
			}

			FIntPoint MinCell = FIntPoint::ZeroValue;
			FIntPoint MaxCell = FIntPoint::ZeroValue;
			if (!TryComputeTerrainCellRect(WorldVolumeBounds, TerrainTransform, Config.CellSize(), MinCell, MaxCell))
			{
				continue;
			}

			const int64 BuiltCellCount = CountBuiltCells(*Terrain, MinCell, MaxCell);
			if (BuiltCellCount <= 0)
			{
				UE_LOGF(LogArcTownSite, Warning,"ArcTown site sampling for '%ls' rejected terrain '%ls': zero built cells under the volume.",
					*VolumeName, *Terrain->GetName());
				continue;
			}

			const FString PathName = Terrain->GetPathName();
			const bool bBetter = Selected == nullptr
				|| BuiltCellCount > SelectedBuiltCellCount
				|| (BuiltCellCount == SelectedBuiltCellCount && PathName.Compare(SelectedPathName, ESearchCase::CaseSensitive) < 0);
			if (bBetter)
			{
				Selected = Terrain;
				SelectedBuiltCellCount = BuiltCellCount;
				SelectedPathName = PathName;
			}
		}

		return Selected;
	}

	/** Dequantized baked height tiles, one entry per terrain cell the sample pass has touched. */
	struct FTerrainSurfaceSampler
	{
		const AArcTerrain* Terrain = nullptr;
		const FArcTerrainConfig* Config = nullptr;
		TArray<FArcTerrainSurfaceCellCache> Cells;
		/** Every key is probed once: a readable payload gets its own entry, a cell nothing shaped shares
		 *  FlatCellIndex, and INDEX_NONE records a registered payload that could not be read. */
		TMap<FIntPoint, int32> KeyToIndex;
		/** Entry shared by every unshaped cell, built on first demand. */
		int32 FlatCellIndex = INDEX_NONE;
	};

	int32 GetOrBuildCellCache(FTerrainSurfaceSampler& Sampler, const FArcTerrainCellKey Key)
	{
		if (const int32* ExistingIndex = Sampler.KeyToIndex.Find(Key))
		{
			return *ExistingIndex;
		}

		// Nothing shaped this cell, so its ground is flat at BaseHeightCm; one entry backs every such key.
		// The FLAT ENTRY STAYS CELL-SIZED even though the payload is a tile: an 8 km tile of flat ground
		// would allocate 32771^2 floats to say one number.
		FIntPoint TileKey = FIntPoint::ZeroValue;
		const bool bHasTile =
			ArcTerrainTileAccess::TileKeyFromCell(*Sampler.Terrain, EArcTerrainRail::Height, 0, Key, TileKey)
			&& ArcTerrainTileAccess::HasTile(*Sampler.Terrain, EArcTerrainRail::Height, 0, TileKey);
		if (!bHasTile)
		{
			if (Sampler.FlatCellIndex == INDEX_NONE)
			{
				Sampler.FlatCellIndex = Sampler.Cells.Num();
				Sampler.Cells.Add(ArcTerrainSurface::BuildFlatCellCache(
					Sampler.Terrain->HeightOptions.BaseHeightCm,
					Sampler.Config->QuadsPerCell(),
					Sampler.Config->CellSize()));
			}
			Sampler.KeyToIndex.Add(Key, Sampler.FlatCellIndex);
			return Sampler.FlatCellIndex;
		}

		// LoadTileSync, not FindLoadedTile: an editor site bake asks about cells no invoker radius will
		// ever pull in.
		int32 CellIndex = INDEX_NONE;
		TArray<float> CellTexels;
		int32 CellResolution = 0;
		if (ArcTerrainTileAccess::LoadCellHeightsSync(*Sampler.Terrain, Key, CellTexels, CellResolution)
			&& CellResolution >= 4)
		{
			// The whole apron-inclusive cell tile is what BuildCellCache expects, in cm as stored.
			CellIndex = Sampler.Cells.Num();
			Sampler.Cells.Add(ArcTerrainSurface::BuildCellCache(Key, CellTexels, CellResolution - 3,
				Sampler.Config->CellSize()));
		}

		// The manifest knows the tile but it could not be read. A cell under a stamp is NOT flat, so this
		// stays a hole.
		Sampler.KeyToIndex.Add(Key, CellIndex);
		return CellIndex;
	}

	/** A built cell cache plus the fractional cell coordinate to sample it at. */
	struct FCellTap
	{
		FArcTerrainCellKey Key = FArcTerrainCellKey(0, 0);
		float FracX = 0.0f;
		float FracY = 0.0f;
		int32 CellIndex = INDEX_NONE;
	};

	/**
	 * Cell the terrain-local position is sampled from, falling back to the lower neighbour on an exact
	 * cell boundary. That fallback is an identity, not an approximation: ArcTerrainSurface::ResolveTaps
	 * maps frac 1 in cell K-1 to interior vertex Quads and frac 0 in cell K to interior vertex 0, both
	 * with zero weight on the apron tap, and the baked payload is edge-inclusive, so the two cells
	 * carry the same value at their shared border vertex. CellIndex stays INDEX_NONE only when every
	 * candidate carries a registered height section that could not be read.
	 */
	FCellTap ResolveCellTap(FTerrainSurfaceSampler& Sampler, const double LocalX, const double LocalY, const double CellSize)
	{
		FCellTap Tap;
		Tap.Key = ArcTerrain::CellKeyFromLocal(FVector(LocalX, LocalY, 0.0), *Sampler.Config);
		Tap.FracX = static_cast<float>(FMath::Clamp((LocalX - static_cast<double>(Tap.Key.X) * CellSize) / CellSize, 0.0, 1.0));
		Tap.FracY = static_cast<float>(FMath::Clamp((LocalY - static_cast<double>(Tap.Key.Y) * CellSize) / CellSize, 0.0, 1.0));

		Tap.CellIndex = GetOrBuildCellCache(Sampler, Tap.Key);
		if (Tap.CellIndex != INDEX_NONE)
		{
			return Tap;
		}

		const bool bCanShiftX = Tap.FracX == 0.0f && Tap.Key.X > MIN_int32;
		const bool bCanShiftY = Tap.FracY == 0.0f && Tap.Key.Y > MIN_int32;
		const FIntPoint Shifts[] = { FIntPoint(-1, 0), FIntPoint(0, -1), FIntPoint(-1, -1) };
		for (const FIntPoint& Shift : Shifts)
		{
			if ((Shift.X != 0 && !bCanShiftX) || (Shift.Y != 0 && !bCanShiftY))
			{
				continue;
			}

			FCellTap Neighbor;
			Neighbor.Key = FArcTerrainCellKey(Tap.Key.X + Shift.X, Tap.Key.Y + Shift.Y);
			Neighbor.FracX = Shift.X != 0 ? 1.0f : Tap.FracX;
			Neighbor.FracY = Shift.Y != 0 ? 1.0f : Tap.FracY;
			Neighbor.CellIndex = GetOrBuildCellCache(Sampler, Neighbor.Key);
			if (Neighbor.CellIndex != INDEX_NONE)
			{
				return Neighbor;
			}
		}

		return Tap;
	}

	enum class EDataTileState : uint8
	{
		Resident,
		OutsideLattice,
		NotBaked,
		Unreadable,
	};

	struct FDataTileLoader
	{
		const AArcTerrain* Terrain = nullptr;
		FArcTerrainTileGrid Grid;
		TMap<FIntPoint, EDataTileState> KeyToState;
	};

	EDataTileState EnsureDataTileLoaded(FDataTileLoader& Loader, const FVector2D& LocalXY, FIntPoint& OutTileKey)
	{
		OutTileKey = FIntPoint::ZeroValue;
		if (!ArcTerrainTileAccess::TileKeyFromLocal(Loader.Grid, LocalXY, OutTileKey))
		{
			return EDataTileState::OutsideLattice;
		}

		if (const EDataTileState* ExistingState = Loader.KeyToState.Find(OutTileKey))
		{
			return *ExistingState;
		}

		EDataTileState State = EDataTileState::NotBaked;
		if (ArcTerrainTileAccess::HasTile(*Loader.Terrain, EArcTerrainRail::Data, 0, OutTileKey, &Loader.Grid))
		{
			const FArcTerrainTilePayloadPtr Payload =
				ArcTerrainTileAccess::LoadTileSync(*Loader.Terrain, EArcTerrainRail::Data, 0, OutTileKey);
			State = Payload.IsValid() ? EDataTileState::Resident : EDataTileState::Unreadable;
		}

		Loader.KeyToState.Add(OutTileKey, State);
		return State;
	}
}

bool ArcTownSiteSampler::SampleSite(AArcTownVolume& Volume, FArcTownSiteSnapshot& OutSnapshot, FText& OutError)
{
	using namespace ArcTownSiteSamplerPrivate;
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_SampleSite);
	const double StartSeconds = FPlatformTime::Seconds();

	if (!IsInGameThread())
	{
		return SetError(OutError, TEXT("ArcTown site sampling must run on the game thread."));
	}

	UWorld* const World = Volume.GetWorld();
	const UBoxComponent* const VolumeBounds = Volume.VolumeBounds;
	if (World == nullptr || VolumeBounds == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown site sampling requires a world and a VolumeBounds component."));
	}

	const FTransform ActorTransform = Volume.GetActorTransform();
	const FVector ActorScale = ActorTransform.GetScale3D();
	if (!IsFiniteVector(ActorTransform.GetLocation()) || !ActorTransform.GetRotation().IsNormalized()
		|| !IsFiniteVector(ActorScale) || ActorScale.X <= 0.0 || ActorScale.Y <= 0.0 || ActorScale.Z <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown site sampling requires a finite volume transform with positive scale."));
	}

	const FVector Extent = VolumeBounds->GetUnscaledBoxExtent();
	if (!IsFiniteVector(Extent) || Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown site sampling requires finite, positive VolumeBounds extents."));
	}

	const double SampleSpacing = Volume.SampleSpacing;
	if (!FMath::IsFinite(SampleSpacing) || SampleSpacing <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown site sampling requires a finite SampleSpacing greater than zero."));
	}

	const int32 MarginCells = Volume.SiteMarginCells;
	if (MarginCells < FArcTownSiteSnapshot::MinMarginCells
		|| static_cast<int64>(MarginCells) > FArcTownSiteSnapshot::MaxSampleCount)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site sampling requires SiteMarginCells of at least %d, and volume '%s' carries %d. The hydrology pass reads terrain outside the site, so the margin cannot be dropped; the usable ceiling is whatever keeps the margined grid inside its %lld sample budget, which is a few hundred cells even for a one-sample site."),
			FArcTownSiteSnapshot::MinMarginCells,
			*Volume.GetName(),
			MarginCells,
			FArcTownSiteSnapshot::MaxSampleCount));
	}

	const float RiversChannelThreshold = Volume.RiversChannelThreshold;
	if (!FMath::IsFinite(RiversChannelThreshold))
	{
		return SetError(OutError, TEXT("ArcTown site sampling requires a finite RiversChannelThreshold."));
	}

	UArcTerrainSubsystem* const TerrainSubsystem = World->GetSubsystem<UArcTerrainSubsystem>();
	if (TerrainSubsystem == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown site sampling requires an ArcTerrain subsystem to read the Rivers data map."));
	}

	const FBox WorldVolumeBounds = FBox(-Extent, Extent).TransformBy(ActorTransform);
	AArcTerrain* const Terrain = SelectTerrain(*World, WorldVolumeBounds, ActorTransform, Volume.GetName());
	if (Terrain == nullptr)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site sampling found no vertically aligned ArcTerrain with built cells under volume '%s'."),
			*Volume.GetName()));
	}

	const FGuid TerrainGuid = Terrain->GetActorGuid();
	if (!TerrainGuid.IsValid())
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site sampling needs a valid actor guid on terrain '%s' to record which terrain the samples came from."),
			*Terrain->GetName()));
	}

	const FArcTerrainConfig& Config = Terrain->Config;
	const FTransform TerrainTransform = Terrain->GetTerrainTransform();
	const FBox LocalVolumeBounds = WorldVolumeBounds.InverseTransformBy(TerrainTransform);
	if (!LocalVolumeBounds.IsValid || !IsFiniteVector(LocalVolumeBounds.Min) || !IsFiniteVector(LocalVolumeBounds.Max))
	{
		return SetError(OutError, TEXT("ArcTown site sampling could not express the volume bounds in the terrain's local frame."));
	}

	const double SpanX = LocalVolumeBounds.Max.X - LocalVolumeBounds.Min.X;
	const double SpanY = LocalVolumeBounds.Max.Y - LocalVolumeBounds.Min.Y;
	const double RatioX = SpanX / SampleSpacing;
	const double RatioY = SpanY / SampleSpacing;
	if (!FMath::IsFinite(RatioX) || !FMath::IsFinite(RatioY) || RatioX < 0.0 || RatioY < 0.0
		|| RatioX > static_cast<double>(FArcTownSiteSnapshot::MaxSampleCount)
		|| RatioY > static_cast<double>(FArcTownSiteSnapshot::MaxSampleCount))
	{
		return SetError(OutError, TEXT("ArcTown site grid exceeds the supported sample allocation limit."));
	}

	// The lattice runs past the max edge rather than stopping short of it, so every point inside the
	// volume falls between four samples.
	const int64 SiteWidth = FMath::CeilToInt64(RatioX) + 1;
	const int64 SiteHeight = FMath::CeilToInt64(RatioY) + 1;
	const int64 GridWidth = SiteWidth + 2 * static_cast<int64>(MarginCells);
	const int64 GridHeight = SiteHeight + 2 * static_cast<int64>(MarginCells);
	if (GridWidth > FArcTownSiteSnapshot::MaxSampleCount / GridHeight)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site grid %lldx%lld, a %lldx%lld site plus a %d cell margin, holds %lld samples, above the maximum of %lld. Raise SampleSpacing, shrink VolumeBounds or lower SiteMarginCells."),
			GridWidth,
			GridHeight,
			SiteWidth,
			SiteHeight,
			MarginCells,
			GridWidth * GridHeight,
			FArcTownSiteSnapshot::MaxSampleCount));
	}
	const int32 SampleCount = static_cast<int32>(GridWidth * GridHeight);

	FTerrainSurfaceSampler Sampler;
	Sampler.Terrain = Terrain;
	Sampler.Config = &Config;

	FDataTileLoader DataLoader;
	DataLoader.Terrain = Terrain;
	if (!ArcTerrainTileAccess::ResolveGrid(*Terrain, EArcTerrainRail::Data, 0, DataLoader.Grid)
		|| !DataLoader.Grid.IsValid())
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site sampling could not resolve the data tile lattice of terrain '%s', so its Rivers map cannot be read. That terrain's tile configuration does not produce a usable data rail."),
			*Terrain->GetName()));
	}

	FArcTownSiteSnapshot Candidate;
	Candidate.TerrainGuid = TerrainGuid;
	Candidate.TerrainToWorld = TerrainTransform;
	Candidate.GridDims = FIntPoint(static_cast<int32>(GridWidth), static_cast<int32>(GridHeight));
	Candidate.SampleSpacing = SampleSpacing;
	Candidate.MarginCells = MarginCells;
	Candidate.RiversChannelThreshold = RiversChannelThreshold;
	const double MarginOffset = static_cast<double>(MarginCells) * SampleSpacing;
	Candidate.OriginLocal =
		FVector2D(LocalVolumeBounds.Min.X - MarginOffset, LocalVolumeBounds.Min.Y - MarginOffset);
	Candidate.Heights.SetNumUninitialized(SampleCount);
	Candidate.Normals.SetNumUninitialized(SampleCount);
	Candidate.ChannelMask.SetNumUninitialized(SampleCount);

	int32 ChannelCellCount = 0;
	bool bAnyRiversReading = false;
	float HighestRiversReading = 0.0f;

	const double CellSize = Config.CellSize();
	for (int32 SampleY = 0; SampleY < Candidate.GridDims.Y; ++SampleY)
	{
		const double LocalY = Candidate.OriginLocal.Y + static_cast<double>(SampleY) * SampleSpacing;
		for (int32 SampleX = 0; SampleX < Candidate.GridDims.X; ++SampleX)
		{
			const double LocalX = Candidate.OriginLocal.X + static_cast<double>(SampleX) * SampleSpacing;
			const FCellTap Tap = ResolveCellTap(Sampler, LocalX, LocalY, CellSize);
			if (Tap.CellIndex == INDEX_NONE)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling could not read terrain cell (%d, %d) of '%s': it carries a registered height section whose baked payload is missing or mis-sized. Re-bake that terrain cell."),
					Tap.Key.X,
					Tap.Key.Y,
					*Terrain->GetName()));
			}

			// The flat substitute is a synthetic entry at BaseHeightCm, so accepting it would hand the
			// hydrology pass a fabricated plain that reads exactly like measured ground.
			if (Tap.CellIndex == Sampler.FlatCellIndex)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling reached terrain cell (%d, %d) of '%s', which carries no baked height tile and would read as flat ground at that terrain's base height. The site and its %d cell margin must both stand on baked terrain: bake that cell, or shrink VolumeBounds."),
					Tap.Key.X,
					Tap.Key.Y,
					*Terrain->GetName(),
					MarginCells));
			}

			const FArcTerrainSurfaceCellCache& Cell = Sampler.Cells[Tap.CellIndex];
			const float HeightCm = ArcTerrainSurface::BilinearApron(Cell.HeightsWithApron, Cell.RenderQuads, Tap.FracX, Tap.FracY);
			const FVector3f Normal = ArcTerrainSurface::BilinearApronVec(Cell.NormalsWithApron, Cell.RenderQuads, Tap.FracX, Tap.FracY).GetSafeNormal();
			if (!FMath::IsFinite(HeightCm) || !IsFiniteVector3f(Normal) || Normal.IsNearlyZero())
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling read a non-finite surface at terrain-local (%.1f, %.1f) in cell (%d, %d) of '%s'."),
					LocalX,
					LocalY,
					Tap.Key.X,
					Tap.Key.Y,
					*Terrain->GetName()));
			}

			// SampleDataMap is GAME THREAD ONLY and takes an AArcTerrain&, so the Rivers mask can only be
			// read here, never from the simulation worker that consumes it.
			const FVector WorldPosition =
				TerrainTransform.TransformPosition(FVector(LocalX, LocalY, static_cast<double>(HeightCm)));

			const FVector DataLocal = Terrain->WorldToTerrainLocal(WorldPosition);
			const FVector2D DataLocalXY(DataLocal.X, DataLocal.Y);
			FIntPoint DataTileKey = FIntPoint::ZeroValue;
			const EDataTileState TileState = EnsureDataTileLoaded(DataLoader, DataLocalXY, DataTileKey);
			if (TileState == EDataTileState::OutsideLattice)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling reached terrain-local (%.1f, %.1f) on '%s', which falls outside that terrain's data tile lattice, so no Rivers reading can exist there. Move the volume onto the terrain, or shrink VolumeBounds."),
					LocalX,
					LocalY,
					*Terrain->GetName()));
			}
			if (TileState == EDataTileState::NotBaked)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling reached terrain-local (%.1f, %.1f) on '%s', whose Rivers reading comes from data tile (%d, %d), and that terrain's manifest does not name it. The site and its %d cell margin must both stand on baked data tiles: bake the data maps over that area, or shrink VolumeBounds."),
					LocalX,
					LocalY,
					*Terrain->GetName(),
					DataTileKey.X,
					DataTileKey.Y,
					MarginCells));
			}
			if (TileState == EDataTileState::Unreadable)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling could not read data tile (%d, %d) of '%s', named by that terrain's manifest, for terrain-local (%.1f, %.1f): its baked payload is missing or does not match the terrain's current tile lattice. Re-bake that terrain's data maps."),
					DataTileKey.X,
					DataTileKey.Y,
					*Terrain->GetName(),
					LocalX,
					LocalY));
			}

			float RiversReading = 0.0f;
			const bool bHasReading =
				TerrainSubsystem->SampleDataMap(*Terrain, EArcTerrainDataMap::Rivers, WorldPosition, RiversReading)
				&& FMath::IsFinite(RiversReading);
			if (!bHasReading)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown site sampling read no Rivers value at terrain-local (%.1f, %.1f) on '%s' from loaded data tile (%d, %d): that tile carries no readable Rivers plane. Bake the Rivers map over that area."),
					LocalX,
					LocalY,
					*Terrain->GetName(),
					DataTileKey.X,
					DataTileKey.Y));
			}

			HighestRiversReading = bAnyRiversReading
				? FMath::Max(HighestRiversReading, RiversReading)
				: RiversReading;
			bAnyRiversReading = true;

			const bool bIsChannel = bHasReading && RiversReading >= RiversChannelThreshold;
			ChannelCellCount += bIsChannel ? 1 : 0;

			const int32 SampleIndex = SampleX + SampleY * Candidate.GridDims.X;
			Candidate.Heights[SampleIndex] = HeightCm;
			Candidate.Normals[SampleIndex] = Normal;
			Candidate.ChannelMask[SampleIndex] = bIsChannel ? 1 : 0;
		}
	}

	if (!bAnyRiversReading)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site sampling read no Rivers value anywhere under volume '%s' on terrain '%s'. The Rivers data map carries the water the town has to stand clear of, so it must be baked before the site is sampled."),
			*Volume.GetName(),
			*Terrain->GetName()));
	}

	if (ChannelCellCount == 0)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site sampling found no Rivers reading at or above RiversChannelThreshold (%.3f) under volume '%s' on terrain '%s'; the highest reading was %.3f. Lower the threshold to at most that reading, or sample a site the water network reaches."),
			RiversChannelThreshold,
			*Volume.GetName(),
			*Terrain->GetName(),
			HighestRiversReading));
	}

	Candidate.SnapshotHash = Candidate.ComputeHash();

	if (!Candidate.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown site sampling produced a snapshot that failed its own validation."));
	}

	UE_LOGF(LogArcTownSite, Log,"ArcTown sampled site for '%ls' against terrain '%ls': %d x %d grid including a %d cell margin, %.1f cm spacing, %d channel cell(s), %d terrain cell(s) covered, %.1f ms.",
		*Volume.GetName(),
		*Terrain->GetName(),
		Candidate.GridDims.X,
		Candidate.GridDims.Y,
		MarginCells,
		SampleSpacing,
		ChannelCellCount,
		Sampler.Cells.Num(),
		(FPlatformTime::Seconds() - StartSeconds) * 1000.0);

	OutSnapshot = MoveTemp(Candidate);
	return true;
}
