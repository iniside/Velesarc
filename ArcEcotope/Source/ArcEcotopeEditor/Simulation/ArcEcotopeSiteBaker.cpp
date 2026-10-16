// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSiteBaker.h"

#include "ArcEcotopeModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"
#include "Climate/ArcClimateProduct.h"
#include "Climate/ArcClimateVolume.h"
#include "Components/BoxComponent.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fields/ArcTerrainMaskDistance.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/ArcEcotopeModificationMask.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Simulation/ArcEcotopeSiteFields.h"
#include "Simulation/ArcEcotopeSiteFieldSettings.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcEcotopeTerrainDataSettings.h"
#include "Simulation/ArcEcotopeTerrainSelection.h"
#include "Surface/ArcTerrainSurfaceSample.h"

namespace ArcEcotopeSiteBakerPrivate
{
	/** First large tiled gate: 4 km at the authored two-metre cell size. */
	constexpr int64 MaxTiledBakedCells = 4000000;
	constexpr int64 MaxTiledMarginedCells = 4024036; // 2006 * 2006, including the three-cell margin.

	TAutoConsoleVariable<int32> CVarMaxProjectedSiteBakePeakMiB(
		TEXT("arcecotope.SiteBake.MaxProjectedPeakMiB"),
		4096,
		TEXT("Reject a tiled ArcEcotope site bake before large allocations when its conservative projected peak exceeds this MiB budget."),
		ECVF_Default);

	struct FSiteBakeMemoryProjection
	{
		uint64 WriterAndPublicationBytes = 0;
		uint64 GlobalGeneratorBytes = 0;
		uint64 TerrainCacheBytes = 0;
		uint64 FixedAllowanceBytes = 0;

		uint64 GetTotalBytes() const
		{
			return WriterAndPublicationBytes + GlobalGeneratorBytes + TerrainCacheBytes
				+ FixedAllowanceBytes;
		}
	};

	bool CheckedMultiply(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Left != 0 && Right > MAX_uint64 / Left)
		{
			OutValue = 0;
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool ProjectTiledBakePeak(
		const int64 BaseCellCount,
		const int64 MarginedCellCount,
		const int64 TerrainSpannedCellCount,
		const int32 TerrainQuadsPerCell,
		FSiteBakeMemoryProjection& OutProjection)
	{
		OutProjection = FSiteBakeMemoryProjection();
		if (BaseCellCount < 0 || MarginedCellCount < 0 || TerrainSpannedCellCount < 0)
		{
			return false;
		}
		if (!CheckedMultiply(static_cast<uint64>(BaseCellCount), 239ull, OutProjection.WriterAndPublicationBytes))
		{
			return false;
		}
		if (!CheckedMultiply(static_cast<uint64>(MarginedCellCount), 160ull, OutProjection.GlobalGeneratorBytes))
		{
			return false;
		}
		if (TerrainSpannedCellCount > 0)
		{
			if (TerrainQuadsPerCell <= 0)
			{
				return false;
			}
			const uint64 TileEdge = static_cast<uint64>(TerrainQuadsPerCell) + 3ull;
			uint64 TileTexels = 0;
			uint64 PerTerrainCellBytes = 0;
			uint64 TexelBytes = 0;
			// At cache construction peak each apron texel can coexist as uint16 source, temporary float
			// dequantization, cached float height and cached FVector3f normal.  Round 22 up to 24 bytes,
			// then add fixed slack for the cell object and map entry.
			if (!CheckedMultiply(TileEdge, TileEdge, TileTexels)
				|| !CheckedMultiply(TileTexels, 24ull, TexelBytes)
				|| TexelBytes > MAX_uint64 - 512ull)
			{
				return false;
			}
			PerTerrainCellBytes = TexelBytes + 512ull;
			if (!CheckedMultiply(
					static_cast<uint64>(TerrainSpannedCellCount),
					PerTerrainCellBytes,
					OutProjection.TerrainCacheBytes))
			{
				return false;
			}
		}
		OutProjection.FixedAllowanceBytes = 256ull * 1024ull * 1024ull;
		const uint64 Terms[3] =
		{
			OutProjection.WriterAndPublicationBytes,
			OutProjection.GlobalGeneratorBytes,
			OutProjection.TerrainCacheBytes,
		};
		uint64 Running = OutProjection.FixedAllowanceBytes;
		for (const uint64 Term : Terms)
		{
			if (Term > MAX_uint64 - Running)
			{
				return false;
			}
			Running += Term;
		}
		return true;
	}

	/** Halo of terrain sampled outside the volume on every side: one cell closes every neighbourhood stencil the generators use, three is the cushion, and the flow and horizon fields truncate at the margined edge. */
	constexpr int64 SiteMarginCells = 3;

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool DoesClimateCoverSite(
		const FArcClimateProductMetadata& ClimateMetadata,
		const FTransform& SiteTransform,
		const FBox& SiteLocalBounds)
	{
		const FVector SiteCorners[4] =
		{
			FVector(SiteLocalBounds.Min.X, SiteLocalBounds.Min.Y, 0.0),
			FVector(SiteLocalBounds.Max.X, SiteLocalBounds.Min.Y, 0.0),
			FVector(SiteLocalBounds.Min.X, SiteLocalBounds.Max.Y, 0.0),
			FVector(SiteLocalBounds.Max.X, SiteLocalBounds.Max.Y, 0.0),
		};
		for (const FVector& SiteCorner : SiteCorners)
		{
			const FVector ClimateLocal = ClimateMetadata.BakeTransform.InverseTransformPosition(
				SiteTransform.TransformPosition(SiteCorner));
			if (!IsFiniteVector(ClimateLocal)
				|| ClimateLocal.X < ClimateMetadata.LocalBounds.Min.X
				|| ClimateLocal.Y < ClimateMetadata.LocalBounds.Min.Y
				|| ClimateLocal.X > ClimateMetadata.LocalBounds.Max.X
				|| ClimateLocal.Y > ClimateMetadata.LocalBounds.Max.Y)
			{
				return false;
			}
		}
		return true;
	}

	double EvaluateEdgeMask(const AArcEcotopeVolume& Volume, const FVector& WorldPosition)
	{
		const FVector Local = Volume.GetActorTransform().InverseTransformPosition(WorldPosition);
		const FVector Half = Volume.VolumeBounds->GetUnscaledBoxExtent();
		if (!IsFiniteVector(Local) || !IsFiniteVector(Half))
		{
			return 0.0;
		}
		const double Outside = FMath::Max(FMath::Abs(Local.X) - Half.X, FMath::Abs(Local.Y) - Half.Y);
		if (Outside <= 0.0)
		{
			return 1.0;
		}
		if (!(Volume.EdgeGradientBandCm > 0.0))
		{
			return 0.0;
		}
		const double Ramp = FMath::Clamp(1.0 - Outside / Volume.EdgeGradientBandCm, 0.0, 1.0);
		return FMath::Pow(Ramp, FMath::Max(Volume.EdgeGradientSharpness, 1.0e-4));
	}

	void SummarizeStoredField(const TArray<float>& Field, double& OutMin, double& OutMean, double& OutMax)
	{
		OutMin = 0.0;
		OutMean = 0.0;
		OutMax = 0.0;
		if (Field.IsEmpty())
		{
			return;
		}

		double Sum = 0.0;
		OutMin = static_cast<double>(Field[0]);
		OutMax = static_cast<double>(Field[0]);
		for (const float Value : Field)
		{
			OutMin = FMath::Min(OutMin, static_cast<double>(Value));
			OutMax = FMath::Max(OutMax, static_cast<double>(Value));
			Sum += static_cast<double>(Value);
		}
		OutMean = Sum / static_cast<double>(Field.Num());
	}

	bool SetError(FText& OutError, const TCHAR* Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	/** Dequantized baked height tiles, one entry per terrain cell the bake has touched. */
	struct FTerrainSurfaceSampler
	{
		const AArcTerrain* Terrain = nullptr;
		const FArcTerrainConfig* Config = nullptr;
		FTransform TerrainTransform = FTransform::Identity;
		TArray<FArcTerrainSurfaceCellCache> Cells;
		/** Every key is probed once: a readable payload gets its own entry, a cell nothing shaped shares
		 *  FlatCellIndex, and INDEX_NONE records a registered payload that could not be read. */
		TMap<FArcTerrainCellKey, int32> KeyToIndex;
		/** Entry shared by every unshaped cell, built on first demand. */
		int32 FlatCellIndex = INDEX_NONE;
		int32 SectionsResolved = 0;
		int32 SectionsLoadedSync = 0;
		int32 SectionsUnusable = 0;
		double SectionLoadMilliseconds = 0.0;
		uint64 SourceHeightBytesTouched = 0;
		uint64 SourceHeightBytesLoadedSync = 0;
	};

	/** One aggregated line for the cells the bake could not read; each one bakes as a hole. */
	void ReportUnusableSections(UE::ArcForest::FDiagnosticScopeReport& Report, const FTerrainSurfaceSampler& Sampler,
		const FString& VolumeName)
	{
		Report.AddDetail(FString::Printf(TEXT("terrainSectionsUnusable=%d"), Sampler.SectionsUnusable));
		if (Sampler.SectionsUnusable > 0)
		{
			UE_LOGF(LogArcEcotope, Warning,"ArcEcotope site bake of %ls: %d terrain cell(s) carry a registered height section that could not be read; those cells bake as holes and fall back to line traces. Re-bake the terrain's sections.",
				*VolumeName,
				Sampler.SectionsUnusable);
		}
	}

	uint64 GetTerrainSamplerAllocatedBytes(const FTerrainSurfaceSampler& Sampler)
	{
		uint64 Bytes = Sampler.Cells.GetAllocatedSize() + Sampler.KeyToIndex.GetAllocatedSize()
			+ Sampler.SourceHeightBytesLoadedSync;
		for (const FArcTerrainSurfaceCellCache& Cell : Sampler.Cells)
		{
			Bytes += Cell.HeightsWithApron.GetAllocatedSize() + Cell.NormalsWithApron.GetAllocatedSize();
		}
		return Bytes;
	}

	/** Cell count for the report: the axis-span saturation sentinel and a strided estimate each read as what they are. */
	FString FormatCellCount(const int64 CellCount, const bool bSampled)
	{
		if (CellCount == TNumericLimits<int64>::Max())
		{
			return FString(TEXT("unbounded"));
		}
		const FString Formatted = FString::Printf(TEXT("%lld"), CellCount);
		return bSampled ? FString(TEXT("~")) + Formatted : Formatted;
	}

	/** Terrain chosen for the bake, plus the cell counts the report quotes. */
	struct FTerrainSelection
	{
		AArcTerrain* Terrain = nullptr;
		/** |Z| of the volume centre in the selected terrain's local frame, in cm. */
		double VolumeCenterHeightCm = 0.0;
		int64 VolumeBuiltCellCount = 0;
		int64 MarginedBuiltCellCount = 0;
		int64 MarginedSpannedCellCount = 0;
		bool bVolumeCountSampled = false;
		bool bMarginedCountSampled = false;
	};

	FTerrainSelection SelectTerrain(UWorld& World, const FBox& WorldVolumeBounds, const FBox& WorldSampleBounds,
		const FTransform& ActorTransform)
	{
		const UE::ArcEcotope::FArcEcotopeTerrainChoice Choice =
			UE::ArcEcotope::SelectTerrainForVolume(World, WorldVolumeBounds, WorldSampleBounds, ActorTransform);

		FTerrainSelection Selection;
		Selection.Terrain = Choice.Terrain;
		Selection.VolumeCenterHeightCm = Choice.VolumeCenterHeightCm;
		Selection.VolumeBuiltCellCount = Choice.VolumeBuiltCellCount;
		Selection.bVolumeCountSampled = Choice.bVolumeCountSampled;
		if (Selection.Terrain == nullptr)
		{
			return Selection;
		}

		FIntPoint MarginedMinCell = FIntPoint::ZeroValue;
		FIntPoint MarginedMaxCell = FIntPoint::ZeroValue;
		int64 MarginedCellCount = 0;
		if (UE::ArcEcotope::TryComputeTerrainCellRect(WorldSampleBounds, Selection.Terrain->GetTerrainTransform(),
			Selection.Terrain->Config.CellSize(), MarginedMinCell, MarginedMaxCell, MarginedCellCount))
		{
			Selection.MarginedSpannedCellCount = MarginedCellCount;
			Selection.MarginedBuiltCellCount = UE::ArcEcotope::CountBuiltTerrainCells(
				*Selection.Terrain, MarginedMinCell, MarginedMaxCell, Selection.bMarginedCountSampled);
		}

		return Selection;
	}

	int32 GetOrBuildCellCache(FTerrainSurfaceSampler& Sampler, FArcTerrainCellKey Key)
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

		int32 CellIndex = INDEX_NONE;
		const bool bAlreadyLoaded =
			ArcTerrainTileAccess::FindLoadedTile(*Sampler.Terrain, EArcTerrainRail::Height, 0, TileKey).IsValid();

		// LoadTileSync, not FindLoadedTile: an editor volume bake asks about cells no invoker radius will
		// ever pull in, and a miss there would fall back to a line trace against terrain that has none.
		TArray<float> CellTexels;
		int32 CellResolution = 0;
		bool bRead = false;
		{
			const UE::ArcForest::FDiagnosticScopeTimer LoadTimer(Sampler.SectionLoadMilliseconds);
			bRead = ArcTerrainTileAccess::LoadCellHeightsSync(*Sampler.Terrain, Key, CellTexels,
				CellResolution);
		}
		if (!bAlreadyLoaded && bRead)
		{
			++Sampler.SectionsLoadedSync;
			Sampler.SourceHeightBytesLoadedSync += CellTexels.GetAllocatedSize();
		}

		if (!bRead || CellResolution < 4)
		{
			// The manifest knows the tile but it could not be read. A cell under a stamp is NOT flat, so
			// this stays a hole.
			++Sampler.SectionsUnusable;
			Sampler.KeyToIndex.Add(Key, CellIndex);
			return CellIndex;
		}

		++Sampler.SectionsResolved;
		Sampler.SourceHeightBytesTouched += CellTexels.GetAllocatedSize();

		// The whole apron-inclusive cell tile is what BuildCellCache expects, in cm as stored.
		CellIndex = Sampler.Cells.Num();
		Sampler.Cells.Add(ArcTerrainSurface::BuildCellCache(Key, CellTexels, CellResolution - 3,
			Sampler.Config->CellSize()));

		Sampler.KeyToIndex.Add(Key, CellIndex);
		return CellIndex;
	}

	/** Baked surface point and normal under a world position's XY, in world space. */
	bool TrySampleTerrainSurface(FTerrainSurfaceSampler& Sampler, const FVector& WorldPosition,
		FVector& OutWorldPosition, FVector& OutWorldNormal)
	{
		const FVector LocalPosition = Sampler.TerrainTransform.InverseTransformPosition(WorldPosition);
		if (!IsFiniteVector(LocalPosition))
		{
			return false;
		}

		const FArcTerrainCellKey Key = ArcTerrain::CellKeyFromLocal(LocalPosition, *Sampler.Config);
		const int32 CellIndex = GetOrBuildCellCache(Sampler, Key);
		if (CellIndex == INDEX_NONE)
		{
			return false;
		}

		const FArcTerrainSurfaceCellCache& Cell = Sampler.Cells[CellIndex];
		const double CellSize = Sampler.Config->CellSize();
		const float FracX = static_cast<float>(FMath::Clamp(
			(LocalPosition.X - static_cast<double>(Key.X) * CellSize) / CellSize, 0.0, 1.0));
		const float FracY = static_cast<float>(FMath::Clamp(
			(LocalPosition.Y - static_cast<double>(Key.Y) * CellSize) / CellSize, 0.0, 1.0));

		const float HeightCm = ArcTerrainSurface::BilinearApron(Cell.HeightsWithApron, Cell.RenderQuads, FracX, FracY);
		if (!FMath::IsFinite(HeightCm))
		{
			return false;
		}

		const FVector3f LocalNormal = ArcTerrainSurface::BilinearApronVec(Cell.NormalsWithApron, Cell.RenderQuads, FracX, FracY);
		OutWorldPosition = Sampler.TerrainTransform.TransformPosition(
			FVector(LocalPosition.X, LocalPosition.Y, static_cast<double>(HeightCm)));
		OutWorldNormal = Sampler.TerrainTransform.TransformVectorNoScale(FVector(LocalNormal)).GetSafeNormal();
		return true;
	}

	/**
	 * Height grid lattice centre. Site coordinates run negative and past GridDimensions inside the margin,
	 * and the trailing site cell is not clamped to LocalBounds.Max the way its snapshot centre is.
	 */
	FVector2D GetUniformCellCenterLocal(const FArcEcotopeSiteMetadata& Metadata, const int32 SiteX, const int32 SiteY)
	{
		const double CellSize = static_cast<double>(Metadata.CellSizeCm);
		return FVector2D(
			Metadata.LocalBounds.Min.X + (static_cast<double>(SiteX) + 0.5) * CellSize,
			Metadata.LocalBounds.Min.Y + (static_cast<double>(SiteY) + 0.5) * CellSize);
	}
	/** Baked terrain height under a lattice centre, in volume-local centimetres, with no Z-span test. */
	bool TryGetLocalSurfaceZCm(FTerrainSurfaceSampler& Sampler, const FTransform& ActorTransform,
		const FVector2D& LocalCenter, const double LocalQueryZCm, double& OutLocalSurfaceZCm)
	{
		if (Sampler.Terrain == nullptr)
		{
			return false;
		}

		const FVector WorldQuery = ActorTransform.TransformPosition(FVector(LocalCenter.X, LocalCenter.Y, LocalQueryZCm));
		FVector WorldSurface = FVector::ZeroVector;
		FVector WorldSurfaceNormal = FVector::ZeroVector;
		if (!TrySampleTerrainSurface(Sampler, WorldQuery, WorldSurface, WorldSurfaceNormal))
		{
			return false;
		}

		const FVector LocalSurface = ActorTransform.InverseTransformPosition(WorldSurface);
		if (!IsFiniteVector(LocalSurface))
		{
			return false;
		}

		OutLocalSurfaceZCm = LocalSurface.Z;
		return true;
	}

	void WriteTiledCellSurface(
		TArray<float>& OutLocalSurfaceZ,
		TArray<uint8>& OutValidSurface,
		TArray<float>& OutSlopeDegrees,
		TArray<float>& OutAspectNormalized,
		const int32 TileCellIndex,
		const double LocalSurfaceZ,
		const FVector& LocalNormal)
	{
		OutLocalSurfaceZ[TileCellIndex] = static_cast<float>(LocalSurfaceZ);
		OutValidSurface[TileCellIndex] = 1;
		OutSlopeDegrees[TileCellIndex] = static_cast<float>(
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(LocalNormal.Z, -1.0, 1.0))));
		const FVector2D Downhill(LocalNormal.X, LocalNormal.Y);
		if (!Downhill.IsNearlyZero())
		{
			double AspectRadians = FMath::Atan2(Downhill.Y, Downhill.X);
			if (AspectRadians < 0.0)
			{
				AspectRadians += UE_TWO_PI;
			}
			const float NormalizedAspect = static_cast<float>(AspectRadians / UE_TWO_PI);
			OutAspectNormalized[TileCellIndex] = NormalizedAspect >= 0.0f && NormalizedAspect < 1.0f ? NormalizedAspect : 0.0f;
		}
	}

	bool GetOwnedRect(
		const FArcEcotopeChunkRect& Chunk,
		const FArcEcotopeSiteMetadata& Metadata,
		const uint8 Level,
		FIntRect& OutOwnedRect)
	{
		return FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
			Chunk.BaseCoreRect, Metadata.GridDimensions, Level, OutOwnedRect);
	}

	template <typename EvaluateBaseType>
	bool BuildReducedOwnedField(
		const FArcEcotopeSiteMetadata& Metadata,
		const uint8 Level,
		const FIntRect& OwnedRect,
		EvaluateBaseType&& EvaluateBase,
		TArray<float>& OutValues)
	{
		const int32 Divisor = 1 << Level;
		if (Divisor <= 0 || OwnedRect.Width() <= 0 || OwnedRect.Height() <= 0)
		{
			return false;
		}
		OutValues.Reset();
		OutValues.Reserve(OwnedRect.Width() * OwnedRect.Height());
		for (int32 LevelY = OwnedRect.Min.Y; LevelY < OwnedRect.Max.Y; ++LevelY)
		{
			for (int32 LevelX = OwnedRect.Min.X; LevelX < OwnedRect.Max.X; ++LevelX)
			{
				const int32 MinBaseX = LevelX * Divisor;
				const int32 MinBaseY = LevelY * Divisor;
				const int32 MaxBaseX = FMath::Min(MinBaseX + Divisor, Metadata.GridDimensions.X);
				const int32 MaxBaseY = FMath::Min(MinBaseY + Divisor, Metadata.GridDimensions.Y);
				double Sum = 0.0;
				int32 Count = 0;
				for (int32 BaseY = MinBaseY; BaseY < MaxBaseY; ++BaseY)
				{
					for (int32 BaseX = MinBaseX; BaseX < MaxBaseX; ++BaseX)
					{
						Sum += static_cast<double>(EvaluateBase(BaseX, BaseY));
						++Count;
					}
				}
				if (Count <= 0)
				{
					return false;
				}
				OutValues.Add(static_cast<float>(FMath::Clamp(Sum / static_cast<double>(Count), 0.0, 1.0)));
			}
		}
		return true;
	}

	bool SliceAndWriteGlobalFloat(
		FArcEcotopeSiteChunkBuildWriter& Writer,
		const FArcEcotopeChunkLayout& Layout,
		const FArcEcotopeSiteMetadata& Metadata,
		const EArcEcotopeSiteField Field,
		const uint8 Level,
		const TConstArrayView<float> FieldValues,
		FString& OutError)
	{
		const FIntPoint Dimensions = Metadata.GetLevelDimensions(Level);
		if (Dimensions.X <= 0 || Dimensions.Y <= 0 || FieldValues.Num() != Dimensions.X * Dimensions.Y)
		{
			OutError = TEXT("ArcEcotope tiled producer received a malformed global field.");
			return false;
		}
		for (const FArcEcotopeChunkRect& Chunk : Layout.GetChunks())
		{
			FIntRect OwnedRect;
			if (!GetOwnedRect(Chunk, Metadata, Level, OwnedRect))
			{
				OutError = TEXT("ArcEcotope tiled producer could not map a global field ownership rectangle.");
				return false;
			}
			TArray<float> TileValues;
			TileValues.Reserve(OwnedRect.Width() * OwnedRect.Height());
			for (int32 Y = OwnedRect.Min.Y; Y < OwnedRect.Max.Y; ++Y)
			{
				TileValues.Append(FieldValues.GetData() + OwnedRect.Min.X + Y * Dimensions.X, OwnedRect.Width());
			}
			if (!Writer.WriteFloatField(Chunk.Id, Field, Level, OwnedRect, TileValues, OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool BuildChannelDistanceField(
		const FArcEcotopeSiteMetadata& Metadata,
		const float ChannelThreshold,
		const TConstArrayView<float> BaseChannelExclusion,
		TArray<double>& OutSquaredCellDistances)
	{
		const int32 Width = Metadata.GridDimensions.X;
		const int32 Height = Metadata.GridDimensions.Y;
		const int32 CellCount = Metadata.GetCellCount();
		if (Width <= 0 || Height <= 0 || CellCount <= 0 || BaseChannelExclusion.Num() != CellCount)
		{
			return false;
		}

		TArray<uint8> ChannelMask;
		ChannelMask.SetNumUninitialized(CellCount);
		for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			ChannelMask[CellIndex] = BaseChannelExclusion[CellIndex] >= ChannelThreshold ? 1 : 0;
		}
		ArcTerrainMaskDistance::BuildSquaredCellDistanceToMask(
			ChannelMask, Width, Height, OutSquaredCellDistances);
		return true;
	}

	bool BuildChannelBufferField(
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcEcotopeTerrainDataSettings& TerrainData,
		const TConstArrayView<float> BaseChannelExclusion,
		const TConstArrayView<double> SquaredCellDistances,
		TArray<float>& OutBaseChannelBuffer)
	{
		const int32 CellCount = Metadata.GetCellCount();
		if (CellCount <= 0 || BaseChannelExclusion.Num() != CellCount || SquaredCellDistances.Num() != CellCount)
		{
			return false;
		}

		const double CellSizeCm = static_cast<double>(Metadata.CellSizeCm);
		const double HardDistanceCm = static_cast<double>(TerrainData.TreeBufferHardDistanceCm);
		const double FalloffDistanceCm = static_cast<double>(TerrainData.TreeBufferFalloffDistanceCm);
		OutBaseChannelBuffer.Reset();
		OutBaseChannelBuffer.SetNumUninitialized(CellCount);
		for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			const double DistanceCm = FMath::Sqrt(SquaredCellDistances[CellIndex]) * CellSizeCm;
			double Ramp = 0.0;
			if (DistanceCm <= HardDistanceCm)
			{
				Ramp = 1.0;
			}
			else if (FalloffDistanceCm > 0.0 && DistanceCm < HardDistanceCm + FalloffDistanceCm)
			{
				Ramp = FMath::Clamp(1.0 - (DistanceCm - HardDistanceCm) / FalloffDistanceCm, 0.0, 1.0);
			}
			OutBaseChannelBuffer[CellIndex] = FMath::Max(BaseChannelExclusion[CellIndex], static_cast<float>(Ramp));
		}
		return true;
	}

	const TCHAR* GetDataMapName(const EArcTerrainDataMap Map)
	{
		switch (Map)
		{
		case EArcTerrainDataMap::Deposits: return TEXT("Deposits");
		case EArcTerrainDataMap::Wear: return TEXT("Wear");
		case EArcTerrainDataMap::Flow: return TEXT("Flow");
		case EArcTerrainDataMap::Soil: return TEXT("Soil");
		case EArcTerrainDataMap::Scree: return TEXT("Scree");
		case EArcTerrainDataMap::RockMap: return TEXT("RockMap");
		case EArcTerrainDataMap::FlowMapX: return TEXT("FlowMapX");
		case EArcTerrainDataMap::FlowMapY: return TEXT("FlowMapY");
		case EArcTerrainDataMap::Rivers: return TEXT("Rivers");
		default: return TEXT("Unknown");
		}
	}

	struct FDataMapCoverage
	{
		int32 CellCount = 0;
		int32 AvailableCount = 0;
		double MinValue = 0.0;
		double MaxValue = 0.0;
		double ValueSum = 0.0;

		void Accumulate(const bool bAvailable, const float Value)
		{
			++CellCount;
			if (!bAvailable)
			{
				return;
			}
			const double Reading = static_cast<double>(Value);
			MinValue = AvailableCount == 0 ? Reading : FMath::Min(MinValue, Reading);
			MaxValue = AvailableCount == 0 ? Reading : FMath::Max(MaxValue, Reading);
			ValueSum += Reading;
			++AvailableCount;
		}
	};

	void ReportDataMapCoverage(
		UE::ArcForest::FDiagnosticScopeReport& Report,
		const FString& VolumeName,
		const EArcTerrainDataMap Map,
		const FDataMapCoverage& Coverage,
		const TCHAR* Scope)
	{
		if (Coverage.CellCount <= 0 || Coverage.AvailableCount <= 0)
		{
			UE_LOGF(LogArcEcotope, Error,"ArcEcotope site bake of %ls: terrain data map '%ls' has no reading anywhere in the sampled bounds (uncovered=1.000); its consumers fall back to the authored substitute.",
				*VolumeName, GetDataMapName(Map));
			Report.AddDetail(FString::Printf(TEXT("dataMap %s[%s] uncovered=1.000"), GetDataMapName(Map), Scope));
			return;
		}
		const double UncoveredFraction =
			1.0 - static_cast<double>(Coverage.AvailableCount) / static_cast<double>(Coverage.CellCount);
		FString Line = FString::Printf(
			TEXT("dataMap %s[%s] uncovered=%.3f min=%.5f max=%.5f mean=%.5f"),
			GetDataMapName(Map), Scope, UncoveredFraction, Coverage.MinValue, Coverage.MaxValue,
			Coverage.ValueSum / static_cast<double>(Coverage.AvailableCount));
		if (UncoveredFraction > 0.0)
		{
			UE_LOGF(LogArcEcotope, Warning,"ArcEcotope site bake of %ls: %ls", *VolumeName, *Line);
		}
		else
		{
			UE_LOGF(LogArcEcotope, Log,"ArcEcotope site bake of %ls: %ls", *VolumeName, *Line);
		}
		Report.AddDetail(MoveTemp(Line));
	}

	/** One terrain data map read at one site cell and turned into a [0,1] site field. */
	struct FTerrainDataFieldSampler
	{
		const UArcTerrainSubsystem* Subsystem = nullptr;
		AArcTerrain* Terrain = nullptr;
		const FArcEcotopeSiteMetadata* Metadata = nullptr;
		const FTransform* ActorTransform = nullptr;

		float Evaluate(
			const EArcTerrainDataMap Map,
			const double Weight,
			const int32 CellX,
			const int32 CellY,
			FDataMapCoverage& InOutCoverage) const
		{
			if (Weight <= 0.0)
			{
				return 0.0f;
			}
			if (Subsystem == nullptr || Terrain == nullptr)
			{
				return 0.0f;
			}
			const FVector WorldPosition =
				ActorTransform->TransformPosition(Metadata->GetCellCenterLocal(CellX, CellY));
			float Reading = 0.0f;
			const bool bHasReading =
				Subsystem->SampleDataMap(*Terrain, Map, WorldPosition, Reading)
				&& FMath::IsFinite(Reading);
			InOutCoverage.Accumulate(bHasReading, Reading);
			if (!bHasReading)
			{
				return 0.0f;
			}
			return static_cast<float>(FMath::Clamp(Weight * static_cast<double>(Reading), 0.0, 1.0));
		}
	};

	void BuildSuppliedSoilDepth(
		const UArcTerrainSubsystem* Subsystem,
		AArcTerrain* Terrain,
		const FArcEcotopeTerrainDataSettings& Settings,
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcEcotopeMarginedHeightGrid& HeightGrid,
		const FTransform& ActorTransform,
		const FIntRect& BaseRect,
		FDataMapCoverage& InOutDepositsCoverage,
		TArray<float>& OutSuppliedDepthM)
	{
		const bool bCanSample = Subsystem != nullptr && Terrain != nullptr;
		// Deposits carries no unit; these two endpoints are what turn it into metres.
		const double DepthAtZeroM = static_cast<double>(Settings.DepositsAtZeroDepthM);
		const double DepthAtMaxM = static_cast<double>(Settings.DepositsAtMaxDepthM);
		const double MissingDepthM = static_cast<double>(Settings.MissingDepositsDepthM);
		OutSuppliedDepthM.Reset();
		OutSuppliedDepthM.Reserve(BaseRect.Width() * BaseRect.Height());
		for (int32 SiteY = BaseRect.Min.Y; SiteY < BaseRect.Max.Y; ++SiteY)
		{
			for (int32 SiteX = BaseRect.Min.X; SiteX < BaseRect.Max.X; ++SiteX)
			{
				float Reading = 0.0f;
				bool bHasReading = false;
				if (bCanSample)
				{
					const int32 CellIndex = HeightGrid.GetIndexFromSiteCell(SiteX, SiteY);
					const FVector2D LocalCenter = GetUniformCellCenterLocal(Metadata, SiteX, SiteY);
					const FVector WorldPosition = ActorTransform.TransformPosition(
						FVector(LocalCenter.X, LocalCenter.Y, HeightGrid.HeightMeters[CellIndex] * 100.0));
					bHasReading =
						Subsystem->SampleDataMap(*Terrain, EArcTerrainDataMap::Deposits, WorldPosition, Reading)
						&& FMath::IsFinite(Reading);
					InOutDepositsCoverage.Accumulate(bHasReading, Reading);
				}
				const double DepthM = bHasReading
					? DepthAtZeroM + (DepthAtMaxM - DepthAtZeroM) * static_cast<double>(Reading)
					: MissingDepthM;
				OutSuppliedDepthM.Add(static_cast<float>(DepthM));
			}
		}
	}

}

namespace ArcEcotopeSiteBakerPrivate
{
	bool BakeTiled(AArcEcotopeVolume& Volume, FArcEcotopeSiteBakeResult& OutResult, FText& OutError)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_SiteBakeTiled);
		UE::ArcForest::FDiagnosticScopeReport Report(TEXT("ArcEcotope SiteBaker::BakeTiled"));
		const FPlatformMemoryStats MemoryBefore = FPlatformMemory::GetStats();
		uint64 PeakTrackedBytes = 0;

		if (!IsInGameThread())
		{
			return SetError(OutError, TEXT("ArcEcotope site baking must run on the game thread."));
		}
		UWorld* World = Volume.GetWorld();
		UBoxComponent* VolumeBounds = Volume.VolumeBounds;
		if (World == nullptr || VolumeBounds == nullptr)
		{
			return SetError(OutError, TEXT("ArcEcotope site baking requires a world and VolumeBounds component."));
		}

		const FTransform ActorTransform = Volume.GetActorTransform();
		const FVector ActorScale = ActorTransform.GetScale3D();
		if (!IsFiniteVector(ActorTransform.GetLocation()) || !ActorTransform.GetRotation().IsNormalized()
			|| !IsFiniteVector(ActorScale) || ActorScale.X <= 0.0f || ActorScale.Y <= 0.0f || ActorScale.Z <= 0.0f
			|| !FMath::IsNearlyEqual(ActorScale.X, ActorScale.Y) || !FMath::IsNearlyEqual(ActorScale.X, ActorScale.Z))
		{
			return SetError(OutError, TEXT("ArcEcotope site baking requires a finite transform with positive, uniform actor scale."));
		}
		const FVector Extent = VolumeBounds->GetUnscaledBoxExtent();
		if (!IsFiniteVector(Extent) || Extent.X <= 0.0f || Extent.Y <= 0.0f || Extent.Z <= 0.0f
			|| !FMath::IsFinite(Volume.CellSizeCm) || Volume.CellSizeCm <= 0.0f
			|| !FMath::IsFinite(Volume.MaxInvalidFraction) || Volume.MaxInvalidFraction < 0.0f || Volume.MaxInvalidFraction > 1.0f)
		{
			return SetError(OutError, TEXT("ArcEcotope site baking requires finite positive extents, cell size, and a normalized MaxInvalidFraction."));
		}
		FText SettingsError;
		if (!Volume.SiteFieldSettings.IsValid(SettingsError) || !Volume.TerrainDataSettings.IsValid(SettingsError)
			|| Volume.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel)
		{
			return SetError(OutError, SettingsError.IsEmpty()
				? TEXT("ArcEcotope site baking requires a supported RecordGridLevel.")
				: *FString::Printf(TEXT("ArcEcotope site baking requires valid field settings: %s"), *SettingsError.ToString()));
		}

		const double EdgeBandCm = FMath::IsFinite(Volume.EdgeGradientBandCm)
			? FMath::Max(Volume.EdgeGradientBandCm, 0.0) : 0.0;
		const FVector GridExtent(Extent.X + EdgeBandCm, Extent.Y + EdgeBandCm, Extent.Z);
		const double CellSize = Volume.CellSizeCm;
		const double Width = static_cast<double>(GridExtent.X) * 2.0;
		const double Height = static_cast<double>(GridExtent.Y) * 2.0;
		const double WidthRatio = Width / CellSize;
		const double HeightRatio = Height / CellSize;
		if (!FMath::IsFinite(WidthRatio) || !FMath::IsFinite(HeightRatio) || WidthRatio <= 0.0 || HeightRatio <= 0.0)
		{
			return SetError(OutError, TEXT("ArcEcotope site grid has invalid dimensions."));
		}
		const int64 GridWidth = FMath::CeilToInt64(WidthRatio);
		const int64 GridHeight = FMath::CeilToInt64(HeightRatio);
		if (GridWidth <= 0 || GridHeight <= 0 || GridWidth > MaxTiledBakedCells || GridHeight > MaxTiledBakedCells
			|| GridWidth > MAX_int32 || GridHeight > MAX_int32 || GridWidth > MaxTiledBakedCells / GridHeight)
		{
			return SetError(OutError, TEXT("ArcEcotope site grid exceeds the supported cell allocation limit."));
		}
		const int64 CellCount64 = GridWidth * GridHeight;
		const int32 CellCount = static_cast<int32>(CellCount64);
		const int64 MarginedWidth = GridWidth + 2 * SiteMarginCells;
		const int64 MarginedHeight = GridHeight + 2 * SiteMarginCells;
		if (MarginedWidth > MAX_int32 || MarginedHeight > MAX_int32 || MarginedWidth > MaxTiledMarginedCells / MarginedHeight)
		{
			return SetError(OutError, TEXT("ArcEcotope margined site grid exceeds the supported allocation limit."));
		}
		const int32 MarginedCellCount = static_cast<int32>(MarginedWidth * MarginedHeight);
		const FBox LocalSiteBounds(-GridExtent, GridExtent);
		FArcClimateFrozenSourcePtr ClimateSource;
		for (TActorIterator<AArcClimateVolume> ClimateVolumeIt(World); ClimateVolumeIt; ++ClimateVolumeIt)
		{
			AArcClimateVolume* CandidateVolume = *ClimateVolumeIt;
			if (!IsValid(CandidateVolume))
			{
				continue;
			}
			FArcClimateFrozenSourcePtr CandidateSource;
			FText CandidateError;
			if (!CandidateVolume->FreezeClimateSource(CandidateSource, CandidateError)
				|| !CandidateSource.IsValid()
				|| CandidateSource->Metadata.FieldValidityMask
					!= FArcClimateProductMetadata::AllFieldValidityMask
				|| !DoesClimateCoverSite(CandidateSource->Metadata, ActorTransform, LocalSiteBounds))
			{
				continue;
			}
			if (ClimateSource.IsValid())
			{
				return SetError(OutError, TEXT("ArcEcotope site baking found more than one complete climate product covering the site footprint."));
			}
			ClimateSource = MoveTemp(CandidateSource);
		}
		if (!ClimateSource.IsValid())
		{
			return SetError(OutError, TEXT("ArcEcotope site baking requires exactly one complete climate product covering the entire site footprint."));
		}
		FArcClimateProductReader ClimateReader;
		FString ClimateError;
		if (!ClimateReader.Open(ClimateSource, ClimateError))
		{
			return SetError(OutError, *FString::Printf(
				TEXT("ArcEcotope site baking could not open the covering climate product: %s"), *ClimateError));
		}
		FArcClimateHydrologySettings ClimateHydrology;
		ClimateHydrology.bUseRivers = ClimateSource->Metadata.bUseRivers;
		ClimateHydrology.RiversWeight = ClimateSource->Metadata.RiversWeight;
		ClimateHydrology.RiversGamma = ClimateSource->Metadata.RiversGamma;
		ClimateHydrology.RiversChannelThreshold = ClimateSource->Metadata.RiversChannelThreshold;
		const FVector MarginedMin(-GridExtent.X - static_cast<double>(SiteMarginCells) * CellSize,
			-GridExtent.Y - static_cast<double>(SiteMarginCells) * CellSize, -GridExtent.Z);
		const FVector MarginedMax(-GridExtent.X + static_cast<double>(GridWidth + SiteMarginCells) * CellSize,
			-GridExtent.Y + static_cast<double>(GridHeight + SiteMarginCells) * CellSize, GridExtent.Z);
		const FBox WorldSampleBounds = FBox(MarginedMin, MarginedMax).TransformBy(ActorTransform);
		const FBox WorldVolumeBounds = LocalSiteBounds.TransformBy(ActorTransform);
		const FTerrainSelection Selection = SelectTerrain(*World, WorldVolumeBounds, WorldSampleBounds, ActorTransform);
		if (Selection.Terrain == nullptr)
		{
			UE_LOGF(LogArcEcotope, Error,"ArcEcotope site bake of %ls: no ArcTerrain carries the volume; baking with no terrain data at all, falling back to line traces for the ground and clearing every terrain data map.",
				*Volume.GetName());
			Report.AddDetail(TEXT("no ArcTerrain under the volume: baking with no terrain data at all"));
		}
		const int32 TerrainQuadsPerCell = Selection.Terrain != nullptr ? Selection.Terrain->Config.QuadsPerCell() : 0;
		FSiteBakeMemoryProjection Projection;
		if (!ProjectTiledBakePeak(
				CellCount64,
				MarginedCellCount,
				Selection.MarginedSpannedCellCount,
				TerrainQuadsPerCell,
				Projection))
		{
			Report.SetOutcome(TEXT("rejected because terrain peak could not be bounded"));
			return SetError(OutError, TEXT("ArcEcotope tiled site bake could not conservatively bound terrain-cache memory; validate the selected ArcTerrain cell size and QuadsPerCell before baking."));
		}
		const uint64 ProjectedPeakBytes = Projection.GetTotalBytes();
		const uint64 PeakGateBytes = static_cast<uint64>(FMath::Max(
			CVarMaxProjectedSiteBakePeakMiB.GetValueOnGameThread(), 1)) * 1024ull * 1024ull;
		const FString ProjectionDetail = FString::Printf(
			TEXT("tiled preflight base=%lld margined=%d terrainCells=%lld terrainQuads=%d projectedPeak=%.2fMiB gate=%.2fMiB writerPublication=%.2fMiB globalGenerators=%.2fMiB terrainCache=%.2fMiB fixed=%.2fMiB"),
			CellCount64,
			MarginedCellCount,
			Selection.MarginedSpannedCellCount,
			TerrainQuadsPerCell,
			static_cast<double>(ProjectedPeakBytes) / (1024.0 * 1024.0),
			static_cast<double>(PeakGateBytes) / (1024.0 * 1024.0),
			static_cast<double>(Projection.WriterAndPublicationBytes) / (1024.0 * 1024.0),
			static_cast<double>(Projection.GlobalGeneratorBytes) / (1024.0 * 1024.0),
			static_cast<double>(Projection.TerrainCacheBytes) / (1024.0 * 1024.0),
			static_cast<double>(Projection.FixedAllowanceBytes) / (1024.0 * 1024.0));
		Report.AddDetail(FString(ProjectionDetail));
		UE_LOGF(LogArcEcotope, Log,"ArcEcotope site bake %ls", *ProjectionDetail);
		if (ProjectedPeakBytes > PeakGateBytes)
		{
			Report.SetOutcome(TEXT("rejected by projected peak memory gate"));
			return SetError(OutError, *FString::Printf(
				TEXT("ArcEcotope tiled site bake projects %.2f MiB, above the configured %.2f MiB peak gate; reduce the volume/cell density or raise arcecotope.SiteBake.MaxProjectedPeakMiB deliberately."),
				static_cast<double>(ProjectedPeakBytes) / (1024.0 * 1024.0),
				static_cast<double>(PeakGateBytes) / (1024.0 * 1024.0)));
		}
		FArcEcotopeSiteMetadata Metadata;
		Metadata.SiteSchemaVersion = FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion();
		Metadata.LocalBounds = LocalSiteBounds;
		Metadata.CellSizeCm = Volume.CellSizeCm;
		Metadata.GridDimensions = FIntPoint(static_cast<int32>(GridWidth), static_cast<int32>(GridHeight));
		Metadata.RecordGridLevel = Volume.RecordGridLevel;
		Metadata.MoistureLevel = 0;
		Metadata.FertilityLevel = 0;
		Metadata.SunLoadLevel = 0;
		Metadata.WindExposureLevel = 0;
		Metadata.SkyViewFactorLevel = 0;
		Metadata.TemperatureLevel = 0;
		Metadata.ModificationCutLevel = 0;
		Metadata.BareRockLevel = 0;
		Metadata.ChannelExclusionLevel = 0;
		Metadata.EstablishmentWearLevel = 0;
		Metadata.SubstrateMobilityLevel = 0;

		Report.AddDetail(FString::Printf(
			TEXT("climateResolution climateCellSizeCm=%.2f siteCellSizeCm=%.2f"),
			static_cast<double>(ClimateSource->Metadata.CellSizeCm), static_cast<double>(Metadata.CellSizeCm)));
		if (static_cast<double>(ClimateSource->Metadata.CellSizeCm) > 2.0 * static_cast<double>(Metadata.CellSizeCm))
		{
			UE_LOGF(LogArcEcotope, Warning,"ArcEcotope site bake of %ls: climate cell size %.2f cm is more than 2x the site cell size %.2f cm (ratio %.2f); climate fields including moisture will read as flat across blocks of site cells.",
				*Volume.GetName(), static_cast<double>(ClimateSource->Metadata.CellSizeCm),
				static_cast<double>(Metadata.CellSizeCm),
				static_cast<double>(ClimateSource->Metadata.CellSizeCm) / static_cast<double>(Metadata.CellSizeCm));
		}

		TArray<float> BaseTemperature;
		TArray<float> BaseSunLoad;
		TArray<float> BaseWindExposure;
		TArray<float> BaseSkyViewFactor;
		TArray<float> BaseMoisture;
		BaseTemperature.SetNumUninitialized(CellCount);
		BaseSunLoad.SetNumUninitialized(CellCount);
		BaseWindExposure.SetNumUninitialized(CellCount);
		BaseSkyViewFactor.SetNumUninitialized(CellCount);
		BaseMoisture.SetNumUninitialized(CellCount);
		const EArcClimateField ClimateFields[5] =
		{
			EArcClimateField::Temperature,
			EArcClimateField::SunLoad,
			EArcClimateField::WindExposure,
			EArcClimateField::SkyViewFactor,
			EArcClimateField::Moisture,
		};
		const TCHAR* ClimateFieldNames[5] =
		{
			TEXT("Temperature"),
			TEXT("SunLoad"),
			TEXT("WindExposure"),
			TEXT("SkyViewFactor"),
			TEXT("Moisture"),
		};
		TArray<float>* ClimateFieldValues[5] =
		{
			&BaseTemperature,
			&BaseSunLoad,
			&BaseWindExposure,
			&BaseSkyViewFactor,
			&BaseMoisture,
		};
		for (int32 FieldIndex = 0; FieldIndex < 5; ++FieldIndex)
		{
			for (int32 CellY = 0; CellY < Metadata.GridDimensions.Y; ++CellY)
			{
				for (int32 CellX = 0; CellX < Metadata.GridDimensions.X; ++CellX)
				{
					const int32 CellIndex = CellX + CellY * Metadata.GridDimensions.X;
					const FVector WorldPosition = ActorTransform.TransformPosition(
						Metadata.GetCellCenterLocal(CellX, CellY));
					float Value = 0.0f;
					bool bFieldValid = false;
					ClimateError.Reset();
					if (!ClimateReader.SampleAtWorldPosition(
							WorldPosition, ClimateFields[FieldIndex], Value, bFieldValid, ClimateError)
						|| !bFieldValid || !FMath::IsFinite(Value))
					{
						return SetError(OutError, *FString::Printf(
							TEXT("ArcEcotope site baking could not read valid %s climate at cell (%d, %d): %s"),
							ClimateFieldNames[FieldIndex], CellX, CellY, *ClimateError));
					}
					(*ClimateFieldValues[FieldIndex])[CellIndex] = Value;
				}
			}
		}

		FArcEcotopeChunkLayout Layout;
		FString WriterError;
		if (!FArcEcotopeChunkLayout::Build(Metadata, Volume.ChunkBaseCellSpan, 0, Layout, WriterError))
		{
			return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled site layout failed: %s"), *WriterError));
		}
		FArcEcotopeSiteChunkBuildWriter Writer;
		if (!Writer.Begin(Metadata, Layout, WriterError))
		{
			return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled writer could not begin: %s"), *WriterError));
		}
		FTerrainSurfaceSampler Sampler;
		auto NoteMemory = [&Report, &MemoryBefore, &PeakTrackedBytes, &Writer, &Sampler](
			const TCHAR* Phase, const uint64 CurrentNonWriterBytes, const uint64 PhasePeakNonWriterBytes = 0)
		{
			uint64 WriterPayloadBytes = 0;
			uint64 WriterOverheadBytes = 0;
			Writer.GetAllocatedBytes(WriterPayloadBytes, WriterOverheadBytes);
			const uint64 PersistentBytes = GetTerrainSamplerAllocatedBytes(Sampler) + WriterPayloadBytes
				+ WriterOverheadBytes;
			const uint64 CurrentBytes = CurrentNonWriterBytes + PersistentBytes;
			const uint64 PhasePeakBytes = (PhasePeakNonWriterBytes > 0 ? PhasePeakNonWriterBytes : CurrentNonWriterBytes)
				+ PersistentBytes;
			PeakTrackedBytes = FMath::Max(PeakTrackedBytes, PhasePeakBytes);
			const FPlatformMemoryStats CurrentMemory = FPlatformMemory::GetStats();
			const int64 PhysicalDelta = static_cast<int64>(CurrentMemory.UsedPhysical) - static_cast<int64>(MemoryBefore.UsedPhysical);
			Report.AddDetail(FString::Printf(TEXT("memory %s current=%.2fMiB peak=%.2fMiB writerPayload=%.2fMiB writerOverhead=%.2fMiB physicalDelta=%.2fMiB"),
				Phase,
				static_cast<double>(CurrentBytes) / (1024.0 * 1024.0),
				static_cast<double>(PeakTrackedBytes) / (1024.0 * 1024.0),
				static_cast<double>(WriterPayloadBytes) / (1024.0 * 1024.0),
				static_cast<double>(WriterOverheadBytes) / (1024.0 * 1024.0),
				static_cast<double>(PhysicalDelta) / (1024.0 * 1024.0)));
		};

		if (Selection.Terrain != nullptr)
		{
			Sampler.Terrain = Selection.Terrain;
			Sampler.Config = &Selection.Terrain->Config;
			Sampler.TerrainTransform = Selection.Terrain->GetTerrainTransform();
		}

		FArcEcotopeMarginedHeightGrid HeightGrid;
		HeightGrid.Dimensions = FIntPoint(static_cast<int32>(MarginedWidth), static_cast<int32>(MarginedHeight));
		HeightGrid.SiteDimensions = Metadata.GridDimensions;
		HeightGrid.MarginCells = static_cast<int32>(SiteMarginCells);
		HeightGrid.CellSizeM = CellSize * 0.01;
		HeightGrid.HeightMeters.Init(0.0, MarginedCellCount);
		TArray<uint8> SiteHeightResolved;
		SiteHeightResolved.Init(0, CellCount);
		TArray<uint8> RetainedValidSurface;
		RetainedValidSurface.Init(0, CellCount);
		TArray<float> RetainedSlopeDegrees;
		RetainedSlopeDegrees.Init(0.0f, CellCount);
		NoteMemory(TEXT("terrainSampler+globalHeight+resolvedMask"),
			static_cast<uint64>(HeightGrid.HeightMeters.GetAllocatedSize()) + SiteHeightResolved.GetAllocatedSize());

		const bool bTrailingColumnClamped = static_cast<double>(GridWidth) * CellSize > Width;
		const bool bTrailingRowClamped = static_cast<double>(GridHeight) * CellSize > Height;
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ArcEcotopeSiteBake), false);
		TraceParams.AddIgnoredActor(&Volume);
		int32 InvalidCount = 0;
		int32 BakedSampleCount = 0;
		int32 FallbackTraceCount = 0;
		int32 MissedTraceCount = 0;
		TUniquePtr<UE::ArcForest::FDiagnosticScopeTimer> PhaseTimer = MakeUnique<UE::ArcForest::FDiagnosticScopeTimer>(
			Report.GetPhases(), TEXT("SiteBake_TiledSurface"));

		for (const FArcEcotopeChunkRect& Chunk : Layout.GetChunks())
		{
			const FIntRect BaseRect = Chunk.BaseCoreRect;
			const int32 TileCellCount = BaseRect.Width() * BaseRect.Height();
			TArray<float> TileSurfaceZ; TileSurfaceZ.Init(0.0f, TileCellCount);
			TArray<uint8> TileValid; TileValid.Init(0, TileCellCount);
			TArray<float> TileSlope; TileSlope.Init(0.0f, TileCellCount);
			TArray<float> TileAspect; TileAspect.Init(0.0f, TileCellCount);
			for (int32 CellY = BaseRect.Min.Y; CellY < BaseRect.Max.Y; ++CellY)
			{
				for (int32 CellX = BaseRect.Min.X; CellX < BaseRect.Max.X; ++CellX)
				{
					const int32 GlobalIndex = CellX + CellY * Metadata.GridDimensions.X;
					const int32 TileIndex = (CellX - BaseRect.Min.X) + (CellY - BaseRect.Min.Y) * BaseRect.Width();
					const FVector LocalCenter = Metadata.GetCellCenterLocal(CellX, CellY);
					const FVector WorldStart = ActorTransform.TransformPosition(FVector(LocalCenter.X, LocalCenter.Y, Extent.Z));
					double SurfaceZCm = 0.0;
					bool bHasSurfaceZ = false;
					bool bSampledBaked = false;
					if (Sampler.Terrain != nullptr)
					{
						FVector WorldSurface = FVector::ZeroVector;
						FVector WorldNormal = FVector::ZeroVector;
						if (TrySampleTerrainSurface(Sampler, WorldStart, WorldSurface, WorldNormal))
						{
							const FVector LocalSurface = ActorTransform.InverseTransformPosition(WorldSurface);
							const FVector LocalNormal = ActorTransform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
							if (IsFiniteVector(LocalSurface)) { SurfaceZCm = LocalSurface.Z; bHasSurfaceZ = true; }
							if (bHasSurfaceZ && IsFiniteVector(LocalNormal) && !LocalNormal.IsNearlyZero()
								&& LocalSurface.Z >= -Extent.Z && LocalSurface.Z <= Extent.Z)
							{
								WriteTiledCellSurface(TileSurfaceZ, TileValid, TileSlope, TileAspect, TileIndex, LocalSurface.Z, LocalNormal);
								++BakedSampleCount;
								bSampledBaked = true;
							}
						}
					}
					if (!bSampledBaked)
					{
						++FallbackTraceCount;
						FHitResult Hit;
						const FVector WorldEnd = ActorTransform.TransformPosition(FVector(LocalCenter.X, LocalCenter.Y, -Extent.Z));
						if (World->LineTraceSingleByChannel(Hit, WorldStart, WorldEnd, Volume.GroundTraceChannel, TraceParams))
						{
							const FVector LocalImpact = ActorTransform.InverseTransformPosition(Hit.ImpactPoint);
							const FVector LocalNormal = ActorTransform.InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
							if (IsFiniteVector(LocalImpact) && IsFiniteVector(LocalNormal) && !LocalNormal.IsNearlyZero())
							{
								SurfaceZCm = LocalImpact.Z; bHasSurfaceZ = true;
								WriteTiledCellSurface(TileSurfaceZ, TileValid, TileSlope, TileAspect, TileIndex, SurfaceZCm, LocalNormal);
							}
							else { ++MissedTraceCount; ++InvalidCount; }
						}
						else { ++MissedTraceCount; ++InvalidCount; }
					}
					RetainedValidSurface[GlobalIndex] = TileValid[TileIndex];
					RetainedSlopeDegrees[GlobalIndex] = TileSlope[TileIndex];
					double LatticeSurfaceZCm = SurfaceZCm;
					bool bHasLatticeSurfaceZ = bHasSurfaceZ;
					if ((bTrailingColumnClamped && CellX == Metadata.GridDimensions.X - 1)
						|| (bTrailingRowClamped && CellY == Metadata.GridDimensions.Y - 1))
					{
						double LatticeZCm = 0.0;
						if (TryGetLocalSurfaceZCm(Sampler, ActorTransform, GetUniformCellCenterLocal(Metadata, CellX, CellY), Extent.Z, LatticeZCm))
						{
							LatticeSurfaceZCm = LatticeZCm; bHasLatticeSurfaceZ = true;
						}
					}
					if (bHasLatticeSurfaceZ)
					{
						HeightGrid.HeightMeters[HeightGrid.GetIndexFromSiteCell(CellX, CellY)] = LatticeSurfaceZCm * 0.01;
						SiteHeightResolved[GlobalIndex] = 1;
					}
				}
			}
			if (!Writer.WriteFloatField(Chunk.Id, EArcEcotopeSiteField::LocalSurfaceZ, 0, BaseRect, TileSurfaceZ, WriterError)
				|| !Writer.WriteByteField(Chunk.Id, EArcEcotopeSiteField::ValidSurface, 0, BaseRect, TileValid, WriterError)
				|| !Writer.WriteFloatField(Chunk.Id, EArcEcotopeSiteField::Slope, 0, BaseRect, TileSlope, WriterError)
				|| !Writer.WriteFloatField(Chunk.Id, EArcEcotopeSiteField::Aspect, 0, BaseRect, TileAspect, WriterError))
			{
				return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled surface write failed: %s"), *WriterError));
			}
			NoteMemory(TEXT("tileScratch+retainedSlopeValidity"), static_cast<uint64>(TileSurfaceZ.GetAllocatedSize())
				+ TileValid.GetAllocatedSize() + TileSlope.GetAllocatedSize() + TileAspect.GetAllocatedSize()
				+ RetainedValidSurface.GetAllocatedSize() + RetainedSlopeDegrees.GetAllocatedSize()
				+ HeightGrid.HeightMeters.GetAllocatedSize() + SiteHeightResolved.GetAllocatedSize());
		}
		if (!SliceAndWriteGlobalFloat(
				Writer, Layout, Metadata, EArcEcotopeSiteField::Temperature, 0, BaseTemperature, WriterError)
			|| !SliceAndWriteGlobalFloat(
				Writer, Layout, Metadata, EArcEcotopeSiteField::SunLoad, 0, BaseSunLoad, WriterError)
			|| !SliceAndWriteGlobalFloat(
				Writer, Layout, Metadata, EArcEcotopeSiteField::WindExposure, 0, BaseWindExposure, WriterError)
			|| !SliceAndWriteGlobalFloat(
				Writer, Layout, Metadata, EArcEcotopeSiteField::SkyViewFactor, 0, BaseSkyViewFactor, WriterError)
			|| !SliceAndWriteGlobalFloat(
				Writer, Layout, Metadata, EArcEcotopeSiteField::Moisture, 0, BaseMoisture, WriterError))
		{
			return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled climate write failed: %s"), *WriterError));
		}
		BaseTemperature.Reset(); BaseTemperature.Shrink();
		BaseSunLoad.Reset(); BaseSunLoad.Shrink();
		BaseWindExposure.Reset(); BaseWindExposure.Shrink();
		BaseSkyViewFactor.Reset(); BaseSkyViewFactor.Shrink();
		BaseMoisture.Reset(); BaseMoisture.Shrink();
		PhaseTimer.Reset();
		PhaseTimer = MakeUnique<UE::ArcForest::FDiagnosticScopeTimer>(Report.GetPhases(), TEXT("SiteBake_MarginSurface"));

		// This deliberately preserves the canonical global row-major mean/order. It is not tile-local: the
		// resolved sentinel and the single margined grid preserve its cross-chunk result exactly.
		int32 SiteHoleFillCount = 0;
		{
			double ResolvedHeightSumMeters = 0.0;
			int32 ResolvedHeightCount = 0;
			for (int32 CellY = 0; CellY < Metadata.GridDimensions.Y; ++CellY)
			{
				for (int32 CellX = 0; CellX < Metadata.GridDimensions.X; ++CellX)
				{
					const int32 CellIndex = CellX + CellY * Metadata.GridDimensions.X;
					if (SiteHeightResolved[CellIndex] != 0)
					{
						ResolvedHeightSumMeters += HeightGrid.HeightMeters[HeightGrid.GetIndexFromSiteCell(CellX, CellY)];
						++ResolvedHeightCount;
					}
				}
			}
			const double MeanHeightMeters = ResolvedHeightCount > 0 ? ResolvedHeightSumMeters / static_cast<double>(ResolvedHeightCount) : 0.0;
			for (int32 CellY = 0; CellY < Metadata.GridDimensions.Y; ++CellY)
			{
				for (int32 CellX = 0; CellX < Metadata.GridDimensions.X; ++CellX)
				{
					const int32 CellIndex = CellX + CellY * Metadata.GridDimensions.X;
					if (SiteHeightResolved[CellIndex] == 0)
					{
						HeightGrid.HeightMeters[HeightGrid.GetIndexFromSiteCell(CellX, CellY)] = MeanHeightMeters;
						++SiteHoleFillCount;
					}
				}
			}
		}

		int32 MarginEdgeClampCount = 0;
		for (int32 MarginedY = 0; MarginedY < HeightGrid.Dimensions.Y; ++MarginedY)
		{
			const int32 SiteY = MarginedY - HeightGrid.MarginCells;
			const bool bRowInsideSite = SiteY >= 0 && SiteY < Metadata.GridDimensions.Y;
			for (int32 MarginedX = 0; MarginedX < HeightGrid.Dimensions.X; ++MarginedX)
			{
				const int32 SiteX = MarginedX - HeightGrid.MarginCells;
				if (bRowInsideSite && SiteX >= 0 && SiteX < Metadata.GridDimensions.X)
				{
					continue;
				}
				double SurfaceZCm = 0.0;
				const bool bHasSurfaceZ = TryGetLocalSurfaceZCm(Sampler, ActorTransform,
					GetUniformCellCenterLocal(Metadata, SiteX, SiteY), Extent.Z, SurfaceZCm);
				const int32 MarginedIndex = HeightGrid.GetIndexFromSiteCell(SiteX, SiteY);
				if (bHasSurfaceZ)
				{
					HeightGrid.HeightMeters[MarginedIndex] = SurfaceZCm * 0.01;
				}
				else
				{
					const int32 ClampedX = FMath::Clamp(SiteX, 0, Metadata.GridDimensions.X - 1);
					const int32 ClampedY = FMath::Clamp(SiteY, 0, Metadata.GridDimensions.Y - 1);
					HeightGrid.HeightMeters[MarginedIndex] = HeightGrid.HeightMeters[HeightGrid.GetIndexFromSiteCell(ClampedX, ClampedY)];
					++MarginEdgeClampCount;
				}
			}
		}
		SiteHeightResolved.Reset();
		SiteHeightResolved.Shrink();
		if (static_cast<double>(InvalidCount) / static_cast<double>(CellCount) > Volume.MaxInvalidFraction)
		{
			Report.SetOutcome(TEXT("rejected by MaxInvalidFraction"));
			return SetError(OutError, *FString::Printf(TEXT("ArcEcotope site bake rejected %.1f%% invalid cells; MaxInvalidFraction is %.1f%%."),
				100.0 * static_cast<double>(InvalidCount) / static_cast<double>(CellCount), 100.0 * static_cast<double>(Volume.MaxInvalidFraction)));
		}
		Report.AddDetail(FString::Printf(TEXT("tiled surface cells=%d baked=%d fallback=%d missed=%d holes=%d edgeClamped=%d"),
			CellCount, BakedSampleCount, FallbackTraceCount, MissedTraceCount, SiteHoleFillCount, MarginEdgeClampCount));
		ReportUnusableSections(Report, Sampler, Volume.GetName());
		NoteMemory(TEXT("globalHeight+retainedSlopeValidity"), static_cast<uint64>(HeightGrid.HeightMeters.GetAllocatedSize())
			+ RetainedSlopeDegrees.GetAllocatedSize() + RetainedValidSurface.GetAllocatedSize());
		PhaseTimer.Reset();
		PhaseTimer = MakeUnique<UE::ArcForest::FDiagnosticScopeTimer>(Report.GetPhases(), TEXT("SiteBake_SoilDepth"));

		const UArcTerrainSubsystem* TerrainSubsystem =
			Selection.Terrain != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;

		FText FieldError;
		FDataMapCoverage DepositsCoverage;
		for (const FArcEcotopeChunkRect& Chunk : Layout.GetChunks())
		{
			const FIntRect BaseRect = Chunk.BaseCoreRect;
			TArray<float> TileSlope;
			TArray<uint8> TileValid;
			TileSlope.Reserve(BaseRect.Width() * BaseRect.Height());
			TileValid.Reserve(BaseRect.Width() * BaseRect.Height());
			for (int32 Y = BaseRect.Min.Y; Y < BaseRect.Max.Y; ++Y)
			{
				for (int32 X = BaseRect.Min.X; X < BaseRect.Max.X; ++X)
				{
					const int32 Index = X + Y * Metadata.GridDimensions.X;
					TileSlope.Add(RetainedSlopeDegrees[Index]);
					TileValid.Add(RetainedValidSurface[Index]);
				}
			}
			TArray<float> TileSuppliedDepthM;
			BuildSuppliedSoilDepth(TerrainSubsystem, Selection.Terrain, Volume.TerrainDataSettings, Metadata,
				HeightGrid, ActorTransform, BaseRect, DepositsCoverage, TileSuppliedDepthM);
			TArray<float> TileSoil;
			ArcEcotopeSiteFields::FSoilDepthDiagnostics SoilDiagnostics;
			if (!ArcEcotopeSiteFields::GenerateSoilDepthCapacityRect(Metadata, TileSlope, TileValid,
				TileSuppliedDepthM, Volume.SiteFieldSettings, BaseRect, TileSoil, SoilDiagnostics, FieldError))
			{
				return SetError(OutError, *FieldError.ToString());
			}
			FIntRect SoilOwnedRect;
			if (!GetOwnedRect(Chunk, Metadata, Metadata.FertilityLevel, SoilOwnedRect))
			{
				return SetError(OutError, TEXT("ArcEcotope tiled soil ownership mapping failed."));
			}
			TArray<float> ReducedSoil;
			if (!BuildReducedOwnedField(Metadata, Metadata.FertilityLevel, SoilOwnedRect,
				[&TileSoil, &BaseRect](const int32 X, const int32 Y) { return TileSoil[(X - BaseRect.Min.X) + (Y - BaseRect.Min.Y) * BaseRect.Width()]; }, ReducedSoil)
				|| !Writer.WriteFloatField(Chunk.Id, EArcEcotopeSiteField::Fertility, Metadata.FertilityLevel, SoilOwnedRect, ReducedSoil, WriterError))
			{
				return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled soil write failed: %s"), *WriterError));
			}
			NoteMemory(TEXT("tileSoil+retainedSlopeValidity"), static_cast<uint64>(TileSlope.GetAllocatedSize()) + TileValid.GetAllocatedSize()
				+ TileSuppliedDepthM.GetAllocatedSize()
				+ TileSoil.GetAllocatedSize() + ReducedSoil.GetAllocatedSize() + RetainedSlopeDegrees.GetAllocatedSize()
				+ RetainedValidSurface.GetAllocatedSize() + HeightGrid.HeightMeters.GetAllocatedSize());
		}
		if (DepositsCoverage.CellCount > 0)
		{
			ReportDataMapCoverage(Report, Volume.GetName(), EArcTerrainDataMap::Deposits, DepositsCoverage, TEXT("soilDepth"));
		}
		HeightGrid.HeightMeters.Reset(); HeightGrid.HeightMeters.Shrink();
		NoteMemory(TEXT("releasedGlobalHeight"), RetainedSlopeDegrees.GetAllocatedSize() + RetainedValidSurface.GetAllocatedSize());
		PhaseTimer.Reset();
		PhaseTimer = MakeUnique<UE::ArcForest::FDiagnosticScopeTimer>(Report.GetPhases(), TEXT("SiteBake_Modification"));

		const FBox WorldSiteBounds = Metadata.LocalBounds.TransformBy(ActorTransform);
		TArray<UArcEcotopeModificationMask*> SortedMasks;
		UE::ArcEcotope::GatherSortedMasks(*World, WorldSiteBounds, SortedMasks);
		const bool bFoldEdgeMask = EdgeBandCm > 0.0;
		const FArcEcotopeTerrainDataSettings& TerrainData = Volume.TerrainDataSettings;
		FTerrainDataFieldSampler DataFieldSampler;
		DataFieldSampler.Subsystem = TerrainSubsystem;
		DataFieldSampler.Terrain = Selection.Terrain;
		DataFieldSampler.Metadata = &Metadata;
		DataFieldSampler.ActorTransform = &ActorTransform;
		FDataMapCoverage RockMapCoverage;
		FDataMapCoverage RiversFieldCoverage;
		FDataMapCoverage WearCoverage;
		FDataMapCoverage ScreeCoverage;
		auto EvaluateChannelExclusion = [&DataFieldSampler, &TerrainData, &ClimateHydrology, &RiversFieldCoverage](const int32 X, const int32 Y) -> float
		{
			const double RiversWeight = TerrainData.bUseTreeRiverBuffer && ClimateHydrology.bUseRivers
				? static_cast<double>(ClimateHydrology.RiversWeight)
				: 0.0;
			const float Weighted = DataFieldSampler.Evaluate(EArcTerrainDataMap::Rivers, RiversWeight, X, Y, RiversFieldCoverage);
			const double Gamma = static_cast<double>(ClimateHydrology.RiversGamma);
			if (Gamma <= 0.0)
			{
				return 0.0f;
			}
			return static_cast<float>(FMath::Clamp(FMath::Pow(static_cast<double>(Weighted), Gamma), 0.0, 1.0));
		};
		TArray<float> BaseChannelExclusion;
		BaseChannelExclusion.SetNumUninitialized(CellCount);
		for (int32 CellY = 0; CellY < Metadata.GridDimensions.Y; ++CellY)
		{
			for (int32 CellX = 0; CellX < Metadata.GridDimensions.X; ++CellX)
			{
				BaseChannelExclusion[CellX + CellY * Metadata.GridDimensions.X] = EvaluateChannelExclusion(CellX, CellY);
			}
		}
		TArray<double> ChannelSquaredCellDistances;
		if (!BuildChannelDistanceField(
				Metadata, ClimateHydrology.RiversChannelThreshold, BaseChannelExclusion,
				ChannelSquaredCellDistances))
		{
			return SetError(OutError, TEXT("ArcEcotope tiled channel distance field received a malformed base field."));
		}
		const uint64 ChannelDistancePersistentBytes = RetainedSlopeDegrees.GetAllocatedSize()
			+ RetainedValidSurface.GetAllocatedSize();
		const uint64 ChannelDistanceResidentBytes = static_cast<uint64>(BaseChannelExclusion.GetAllocatedSize())
			+ ChannelSquaredCellDistances.GetAllocatedSize();
		NoteMemory(TEXT("globalChannelExclusion+channelDistances"),
			ChannelDistanceResidentBytes + ChannelDistancePersistentBytes,
			ChannelDistanceResidentBytes + ChannelDistancePersistentBytes
				+ static_cast<uint64>(CellCount) * sizeof(uint8));
		for (const FArcEcotopeChunkRect& Chunk : Layout.GetChunks())
		{
			auto EvaluateModificationCut = [&SortedMasks, bFoldEdgeMask, &Volume, &ActorTransform, &Metadata](const int32 X, const int32 Y)
			{
				const FVector WorldPosition = ActorTransform.TransformPosition(Metadata.GetCellCenterLocal(X, Y));
				double Cut = FMath::Clamp(UE::ArcEcotope::EvaluateCutAt(SortedMasks, WorldPosition), 0.0, 1.0);
				if (bFoldEdgeMask)
				{
					const double RawEdgeMask = EvaluateEdgeMask(Volume, WorldPosition);
					const double EdgeMask = FMath::IsFinite(RawEdgeMask) ? FMath::Clamp(RawEdgeMask, 0.0, 1.0) : 0.0;
					Cut = 1.0 - (1.0 - Cut) * EdgeMask;
				}
				return static_cast<float>(Cut);
			};
			auto EvaluateBareRock = [&DataFieldSampler, &TerrainData, &RockMapCoverage](const int32 X, const int32 Y) -> float
			{
				return DataFieldSampler.Evaluate(EArcTerrainDataMap::RockMap,
					static_cast<double>(TerrainData.RockWeight), X, Y, RockMapCoverage);
			};
			auto EvaluateEstablishmentWear = [&DataFieldSampler, &TerrainData, &WearCoverage](const int32 X, const int32 Y) -> float
			{
				return DataFieldSampler.Evaluate(EArcTerrainDataMap::Wear,
					static_cast<double>(TerrainData.WearWeight), X, Y, WearCoverage);
			};
			auto EvaluateSubstrateMobility = [&DataFieldSampler, &TerrainData, &ScreeCoverage](const int32 X, const int32 Y) -> float
			{
				return DataFieldSampler.Evaluate(EArcTerrainDataMap::Scree,
					static_cast<double>(TerrainData.ScreeWeight), X, Y, ScreeCoverage);
			};
			auto WriteReduced = [&Writer, &WriterError, &Metadata, &Chunk](const EArcEcotopeSiteField Field, const uint8 Level, auto&& Evaluate) -> bool
			{
				FIntRect OwnedRect;
				TArray<float> Values;
				return GetOwnedRect(Chunk, Metadata, Level, OwnedRect)
					&& BuildReducedOwnedField(Metadata, Level, OwnedRect, Evaluate, Values)
					&& Writer.WriteFloatField(Chunk.Id, Field, Level, OwnedRect, Values, WriterError);
			};
			if (!WriteReduced(EArcEcotopeSiteField::ModificationCut, Metadata.ModificationCutLevel, EvaluateModificationCut)
				|| !WriteReduced(EArcEcotopeSiteField::BareRock, Metadata.BareRockLevel, EvaluateBareRock)
				|| !WriteReduced(EArcEcotopeSiteField::EstablishmentWear, Metadata.EstablishmentWearLevel, EvaluateEstablishmentWear)
				|| !WriteReduced(EArcEcotopeSiteField::SubstrateMobility, Metadata.SubstrateMobilityLevel, EvaluateSubstrateMobility))
			{
				return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled modification write failed: %s"), *WriterError));
			}
		}

		TArray<float> BaseChannelBuffer;
		if (!BuildChannelBufferField(Metadata, TerrainData, BaseChannelExclusion, ChannelSquaredCellDistances, BaseChannelBuffer))
		{
			return SetError(OutError, TEXT("ArcEcotope tiled channel buffer generation received a malformed base field."));
		}
		TArray<float> LevelChannelExclusion;
		uint64 ChannelExclusionReductionPeakBytes = 0;
		if (!ArcEcotopeSiteFields::ReduceFieldToLevel(Metadata, BaseChannelExclusion, Metadata.ChannelExclusionLevel,
				LevelChannelExclusion, &ChannelExclusionReductionPeakBytes)
			|| !SliceAndWriteGlobalFloat(Writer, Layout, Metadata, EArcEcotopeSiteField::ChannelExclusion,
				Metadata.ChannelExclusionLevel, LevelChannelExclusion, WriterError)
			|| !SliceAndWriteGlobalFloat(Writer, Layout, Metadata, EArcEcotopeSiteField::ChannelBuffer,
				0, BaseChannelBuffer, WriterError))
		{
			return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled channel field write failed: %s"), *WriterError));
		}
		const uint64 ChannelPersistentBytes = RetainedSlopeDegrees.GetAllocatedSize()
			+ RetainedValidSurface.GetAllocatedSize();
		const uint64 ChannelResidentBytes = static_cast<uint64>(BaseChannelExclusion.GetAllocatedSize())
			+ ChannelSquaredCellDistances.GetAllocatedSize()
			+ BaseChannelBuffer.GetAllocatedSize() + LevelChannelExclusion.GetAllocatedSize();
		NoteMemory(TEXT("globalChannelExclusion+channelBuffer"),
			ChannelResidentBytes + ChannelPersistentBytes,
			ChannelResidentBytes + ChannelPersistentBytes + ChannelExclusionReductionPeakBytes);
		BaseChannelExclusion.Reset(); BaseChannelExclusion.Shrink();
		ChannelSquaredCellDistances.Reset(); ChannelSquaredCellDistances.Shrink();
		BaseChannelBuffer.Reset(); BaseChannelBuffer.Shrink();
		LevelChannelExclusion.Reset(); LevelChannelExclusion.Shrink();

		if (RockMapCoverage.CellCount > 0)
		{
			ReportDataMapCoverage(Report, Volume.GetName(), EArcTerrainDataMap::RockMap, RockMapCoverage, TEXT("siteField"));
		}
		if (RiversFieldCoverage.CellCount > 0)
		{
			ReportDataMapCoverage(Report, Volume.GetName(), EArcTerrainDataMap::Rivers, RiversFieldCoverage, TEXT("siteField"));
		}
		if (WearCoverage.CellCount > 0)
		{
			ReportDataMapCoverage(Report, Volume.GetName(), EArcTerrainDataMap::Wear, WearCoverage, TEXT("siteField"));
		}
		if (ScreeCoverage.CellCount > 0)
		{
			ReportDataMapCoverage(Report, Volume.GetName(), EArcTerrainDataMap::Scree, ScreeCoverage, TEXT("siteField"));
		}
		PhaseTimer.Reset();
		PhaseTimer = MakeUnique<UE::ArcForest::FDiagnosticScopeTimer>(Report.GetPhases(), TEXT("SiteBake_RecordValidAndFinalize"));
		// Record validity is a base-child OR in ascending row-major order.  The ownership rect uses the
		// same min-corner rule as every coarse field, so a record is written by exactly one chunk.
		const int32 RecordDivisor = 1 << Metadata.RecordGridLevel;
		for (const FArcEcotopeChunkRect& Chunk : Layout.GetChunks())
		{
			FIntRect RecordRect;
			if (!GetOwnedRect(Chunk, Metadata, Metadata.RecordGridLevel, RecordRect))
			{
				return SetError(OutError, TEXT("ArcEcotope tiled record ownership mapping failed."));
			}
			TArray<uint8> RecordValid;
			RecordValid.Reserve(RecordRect.Width() * RecordRect.Height());
			for (int32 RecordY = RecordRect.Min.Y; RecordY < RecordRect.Max.Y; ++RecordY)
			{
				for (int32 RecordX = RecordRect.Min.X; RecordX < RecordRect.Max.X; ++RecordX)
				{
					uint8 bAnyChildValid = 0;
					const int32 MinBaseX = RecordX * RecordDivisor;
					const int32 MinBaseY = RecordY * RecordDivisor;
					const int32 MaxBaseX = FMath::Min(MinBaseX + RecordDivisor, Metadata.GridDimensions.X);
					const int32 MaxBaseY = FMath::Min(MinBaseY + RecordDivisor, Metadata.GridDimensions.Y);
					for (int32 BaseY = MinBaseY; BaseY < MaxBaseY && bAnyChildValid == 0; ++BaseY)
					{
						for (int32 BaseX = MinBaseX; BaseX < MaxBaseX; ++BaseX)
						{
							bAnyChildValid |= RetainedValidSurface[BaseX + BaseY * Metadata.GridDimensions.X] != 0 ? 1 : 0;
						}
					}
					RecordValid.Add(bAnyChildValid);
				}
			}
			if (!Writer.WriteByteField(Chunk.Id, EArcEcotopeSiteField::RecordValid, Metadata.RecordGridLevel, RecordRect, RecordValid, WriterError))
			{
				return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled RecordValid write failed: %s"), *WriterError));
			}
		}
		RetainedSlopeDegrees.Reset(); RetainedSlopeDegrees.Shrink();
		RetainedValidSurface.Reset(); RetainedValidSurface.Shrink();
		NoteMemory(TEXT("releasedSlopeValidity"), 0);

		FArcEcotopeSiteMetadata FinalMetadata;
		FArcEcotopeSiteChunkManifest FinalManifest;
		TArray<FSharedBuffer> FinalPayloads;
		if (!Writer.Finalize(Metadata, FinalMetadata, FinalManifest, FinalPayloads, WriterError))
		{
			return SetError(OutError, *FString::Printf(TEXT("ArcEcotope tiled writer finalization failed: %s"), *WriterError));
		}
		uint64 PayloadBytes = 0;
		for (const FSharedBuffer& Payload : FinalPayloads)
		{
			PayloadBytes += Payload.GetSize();
		}
		NoteMemory(TEXT("writerPayload+publicationStaging"), PayloadBytes + FinalPayloads.GetAllocatedSize()
			+ FinalManifest.Chunks.GetAllocatedSize());
		Report.AddDetail(FString::Printf(TEXT("tiled writer chunks=%d payload=%.2fMiB terrainCacheAndLoadedSource=%.2fMiB sourceTouched=%.2fMiB sourceLoadedSync=%.2fMiB"),
			FinalPayloads.Num(), static_cast<double>(PayloadBytes) / (1024.0 * 1024.0),
			static_cast<double>(GetTerrainSamplerAllocatedBytes(Sampler)) / (1024.0 * 1024.0),
			static_cast<double>(Sampler.SourceHeightBytesTouched) / (1024.0 * 1024.0),
			static_cast<double>(Sampler.SourceHeightBytesLoadedSync) / (1024.0 * 1024.0)));

		FArcEcotopeSiteBakeResult Result;
		Result.Metadata = MoveTemp(FinalMetadata);
		Result.ChunkManifest = MoveTemp(FinalManifest);
		Result.ChunkPayloads = MoveTemp(FinalPayloads);
		PhaseTimer.Reset();
		OutResult = MoveTemp(Result);
		OutError = FText::GetEmpty();
		Report.SetOutcome(TEXT("succeeded (tiled producer)"));
		return true;
	}
}

bool FArcEcotopeSiteBaker::Bake(AArcEcotopeVolume& Volume, FArcEcotopeSiteBakeResult& OutResult, FText& OutError)
{
	return ArcEcotopeSiteBakerPrivate::BakeTiled(Volume, OutResult, OutError);
}
