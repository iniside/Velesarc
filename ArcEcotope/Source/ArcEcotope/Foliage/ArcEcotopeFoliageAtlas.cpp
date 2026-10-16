// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliageAtlas.h"

#include "ArcEcotopeModule.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"

#include "Misc/LargeWorldRenderPosition.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "SystemTextures.h"
#include "TextureResource.h"

/** File-scoped and uniquely named (CLAUDE.md C++ Code Style). */
namespace ArcEcotopeFoliageAtlasPrivate
{
	/** Slack on every side of the touched span: 1 for the bilinear footprint of an edge sample, 1 more
	 *  for the normal's central-difference tap, which steps one atlas texel outward before fetching. */
	constexpr int32 PadTexels = 2;

	/** ArcTerrainTypes.cpp::ArcCellIndex — floor division, LWC-safe in double. */
	int32 CellIndex(double Coord, double CellSize)
	{
		return static_cast<int32>(FMath::FloorToDouble(Coord / CellSize));
	}

	/** ArcTerrainTypes.cpp::ArcOnCellBoundary — CellSize is contractually a whole number of cm, so the
	 *  Fmod comparison is meaningful. */
	bool OnCellBoundary(double Coord, double CellSize)
	{
		return FMath::Fmod(Coord, CellSize) == 0.0;
	}

	/** Padded, tile-clamped inclusive texel span a [Frac0, Frac1] cell-local span touches. Returns
	 *  false when the span had to be clamped against the tile edge. */
	bool TexelSpan(float Frac0, float Frac1, int32 TileRes, int32& OutLo, int32& OutHi)
	{
		const float Texel0 = ArcEcotopeFoliageAtlas::WorldToTileTexel(FMath::Min(Frac0, Frac1), TileRes);
		const float Texel1 = ArcEcotopeFoliageAtlas::WorldToTileTexel(FMath::Max(Frac0, Frac1), TileRes);

		const int32 RawLo = FMath::FloorToInt32(Texel0) - PadTexels;
		const int32 RawHi = FMath::CeilToInt32(Texel1) + PadTexels;

		OutLo = FMath::Clamp(RawLo, 0, TileRes - 1);
		OutHi = FMath::Clamp(RawHi, 0, TileRes - 1);

		return RawLo == OutLo && RawHi == OutHi;
	}

	/** The matrices handed to the shader are relative to LWCTile, so the shader's LWCMultiply never
	 *  multiplies a world position by a matrix carrying a multi-kilometre translation. */
	void SplitTerrainTransformLWC(const FTransform& InTerrainTransform, FVector3f& OutLWCTile,
		FMatrix44f& OutLocalToWorld, FMatrix44f& OutWorldToLocal, FVector3f& OutInvNonUniformScale)
	{
		FTransform TileRelative = InTerrainTransform;
		const FLargeWorldRenderPosition Origin(InTerrainTransform.GetLocation());

		OutLWCTile = Origin.GetTile();
		TileRelative.SetLocation(FVector(Origin.GetOffset()));

		const FMatrix LocalToWorld = TileRelative.ToMatrixWithScale();
		OutLocalToWorld = FMatrix44f(LocalToWorld);
		OutWorldToLocal = FMatrix44f(LocalToWorld.Inverse());

		// RotateLocalToWorld's InvScale renormalizes the LocalToWorld basis ROWS, so it is the
		// reciprocal MAGNITUDE — not the signed reciprocal.
		const FVector Scale = InTerrainTransform.GetScale3D();
		OutInvNonUniformScale = FVector3f(
			FMath::IsNearlyZero(Scale.X) ? 1.0f : static_cast<float>(1.0 / FMath::Abs(Scale.X)),
			FMath::IsNearlyZero(Scale.Y) ? 1.0f : static_cast<float>(1.0 / FMath::Abs(Scale.Y)),
			FMath::IsNearlyZero(Scale.Z) ? 1.0f : static_cast<float>(1.0 / FMath::Abs(Scale.Z)));
	}

	/** ArcTerrain hands out BYTES, never a texture, so the GPU copy source is built here. */
	FArcEcotopeFoliageTileTexturePtr UploadTileTexture(TArray<uint8>&& Bytes, int32 Resolution,
		int32 BytesPerTexel, EPixelFormat Format, const TCHAR* DebugName)
	{
		if (Resolution < 4 || Bytes.Num() != Resolution * Resolution * BytesPerTexel)
		{
			return FArcEcotopeFoliageTileTexturePtr();
		}

		FArcEcotopeFoliageTileTexturePtr Tile =
			MakeShared<FArcEcotopeFoliageTileTexture, ESPMode::ThreadSafe>();

		ENQUEUE_RENDER_COMMAND(ArcEcotopeUploadTerrainTile)(
			[Tile, Payload = MoveTemp(Bytes), Resolution, BytesPerTexel, Format, DebugName]
			(FRHICommandListImmediate& RHICmdList)
			{
				const FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create2D(DebugName, Resolution, Resolution, Format)
						.SetFlags(ETextureCreateFlags::ShaderResource)
						.SetInitialState(ERHIAccess::SRVMask);
				FTextureRHIRef Created = RHICmdList.CreateTexture(Desc);

				const FUpdateTextureRegion2D Region(0, 0, 0, 0, Resolution, Resolution);
				RHICmdList.UpdateTexture2D(Created, /*MipIndex=*/0, Region,
					static_cast<uint32>(Resolution * BytesPerTexel), Payload.GetData());
				Tile->TextureRHI = MoveTemp(Created);
			});

		return Tile;
	}

	/** Rejects a texture that cannot hold the planned sub-rect — a cell baked at a different raster
	 *  would make the copy read out of bounds — or whose format does not match the atlas.
	 *
	 *  A mismatch is a legal hole, handled like the null case: skip the slot and pick it up once the
	 *  upload that fills this box has run. */
	FRDGTextureRef RegisterTileSource(FRDGBuilder& GraphBuilder,
		const FArcEcotopeFoliageTileTexturePtr& Tile, FIntPoint SrcOrigin, FIntPoint SubRect,
		EPixelFormat ExpectedFormat, const TCHAR* Name)
	{
		if (!Tile.IsValid() || !Tile->TextureRHI.IsValid())
		{
			return nullptr;
		}
		const FTextureRHIRef& TextureRHI = Tile->TextureRHI;

		if (TextureRHI->GetFormat() != ExpectedFormat)
		{
			UE_LOGF(LogArcEcotope, Verbose,"[Foliage] Tile source '%ls' has format %d, expected %d — slot skipped this frame (texture still compiling?)",
				*TextureRHI->GetName().ToString(), int32(TextureRHI->GetFormat()), int32(ExpectedFormat));
			return nullptr;
		}

		const FIntPoint SourceSize = TextureRHI->GetSizeXY();
		if (SrcOrigin.X < 0 || SrcOrigin.Y < 0 || SrcOrigin.X + SubRect.X > SourceSize.X
			|| SrcOrigin.Y + SubRect.Y > SourceSize.Y)
		{
			return nullptr;
		}

		return RegisterExternalTexture(GraphBuilder, TextureRHI, Name);
	}

	void CopySubRect(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Dest,
		FIntPoint SrcOrigin, FIntPoint SubRect, int32 SrcSlice, int32 DestSlice)
	{
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size = FIntVector(SubRect.X, SubRect.Y, 1);
		CopyInfo.SourcePosition = FIntVector(SrcOrigin.X, SrcOrigin.Y, 0);
		CopyInfo.DestPosition = FIntVector::ZeroValue;
		CopyInfo.SourceSliceIndex = static_cast<uint32>(SrcSlice);
		CopyInfo.DestSliceIndex = static_cast<uint32>(DestSlice);
		CopyInfo.NumSlices = 1;

		AddCopyTexturePass(GraphBuilder, Source, Dest, CopyInfo);
	}

}

namespace ArcEcotopeFoliageAtlas
{

float WorldToTileTexel(float Frac, int32 TileRes)
{
	return Frac * static_cast<float>(TileRes - 3) + 1.5f;
}

int32 ResolveLayerFilterChannel(FName LayerChannel, const UArcTerrainChannelSet* ChannelSet,
	bool& bOutAuthoringError)
{
	bOutAuthoringError = false;

	if (LayerChannel.IsNone())
	{
		return INDEX_NONE;
	}

	if (ChannelSet == nullptr)
	{
		bOutAuthoringError = true;
		return INDEX_NONE;
	}

	const int32 Index = ChannelSet->FindChannelIndex(LayerChannel);
	if (Index == INDEX_NONE)
	{
		bOutAuthoringError = true;
		return INDEX_NONE;
	}

	return Index;
}

FArcEcotopeFoliageAtlasLayout ComputeAtlasLayout(const FBox& LocalBounds, double CellSize,
	int32 HeightTileRes, int32 WeightTileRes, int32 MaxCells)
{
	using namespace ArcEcotopeFoliageAtlasPrivate;

	FArcEcotopeFoliageAtlasLayout Layout;

	// HeightTileRes < 4 is impossible, not degraded: TileRes == Quads + 3 with Quads >= 1.
	if (!LocalBounds.IsValid || CellSize <= 0.0 || HeightTileRes < 4 || MaxCells <= 0)
	{
		return Layout;
	}

	// EDGE-INCLUSIVE, identical to ArcTerrain::CellsIntersectingLocalBounds.
	int32 MinCellX = CellIndex(LocalBounds.Min.X, CellSize);
	int32 MinCellY = CellIndex(LocalBounds.Min.Y, CellSize);
	const int32 MaxCellX = CellIndex(LocalBounds.Max.X, CellSize);
	const int32 MaxCellY = CellIndex(LocalBounds.Max.Y, CellSize);

	if (OnCellBoundary(LocalBounds.Min.X, CellSize))
	{
		--MinCellX;
	}
	if (OnCellBoundary(LocalBounds.Min.Y, CellSize))
	{
		--MinCellY;
	}

	int32 CountX = FMath::Max(MaxCellX - MinCellX + 1, 1);
	int32 CountY = FMath::Max(MaxCellY - MinCellY + 1, 1);

	Layout.RequestedCellCount = FIntPoint(CountX, CountY);

	// Shrink the LARGER axis first so the retained rectangle stays as square as possible around the min
	// corner, which is the corner the gather scans from.
	while (static_cast<int64>(CountX) * static_cast<int64>(CountY) > static_cast<int64>(MaxCells))
	{
		Layout.bCellRectClamped = true;
		if (CountX >= CountY && CountX > 1)
		{
			--CountX;
		}
		else if (CountY > 1)
		{
			--CountY;
		}
		else
		{
			break;
		}
	}

	Layout.OriginCell = FIntPoint(MinCellX, MinCellY);
	Layout.CellCount = FIntPoint(CountX, CountY);

	const bool bHasWeights = WeightTileRes >= 4;

	TArray<FIntPoint> HeightLo;
	TArray<FIntPoint> WeightLo;
	HeightLo.Reserve(CountX * CountY);
	WeightLo.Reserve(CountX * CountY);

	FIntPoint HeightSize(1, 1);
	FIntPoint WeightSize(1, 1);

	for (int32 CellY = MinCellY; CellY < MinCellY + CountY; ++CellY)
	{
		for (int32 CellX = MinCellX; CellX < MinCellX + CountX; ++CellX)
		{
			const double CellMinX = static_cast<double>(CellX) * CellSize;
			const double CellMinY = static_cast<double>(CellY) * CellSize;

			const double OverlapMinX = FMath::Max(LocalBounds.Min.X, CellMinX);
			const double OverlapMaxX = FMath::Min(LocalBounds.Max.X, CellMinX + CellSize);
			const double OverlapMinY = FMath::Max(LocalBounds.Min.Y, CellMinY);
			const double OverlapMaxY = FMath::Min(LocalBounds.Max.Y, CellMinY + CellSize);

			const float FracMinX = static_cast<float>(FMath::Clamp((OverlapMinX - CellMinX) / CellSize, 0.0, 1.0));
			const float FracMaxX = static_cast<float>(FMath::Clamp((OverlapMaxX - CellMinX) / CellSize, 0.0, 1.0));
			const float FracMinY = static_cast<float>(FMath::Clamp((OverlapMinY - CellMinY) / CellSize, 0.0, 1.0));
			const float FracMaxY = static_cast<float>(FMath::Clamp((OverlapMaxY - CellMinY) / CellSize, 0.0, 1.0));

			int32 LoX = 0;
			int32 HiX = 0;
			int32 LoY = 0;
			int32 HiY = 0;
			const bool bFitX = TexelSpan(FracMinX, FracMaxX, HeightTileRes, LoX, HiX);
			const bool bFitY = TexelSpan(FracMinY, FracMaxY, HeightTileRes, LoY, HiY);
			Layout.bSubRectClamped |= !bFitX || !bFitY;

			HeightLo.Add(FIntPoint(LoX, LoY));
			HeightSize.X = FMath::Max(HeightSize.X, HiX - LoX + 1);
			HeightSize.Y = FMath::Max(HeightSize.Y, HiY - LoY + 1);

			if (bHasWeights)
			{
				int32 WLoX = 0;
				int32 WHiX = 0;
				int32 WLoY = 0;
				int32 WHiY = 0;
				const bool bWFitX = TexelSpan(FracMinX, FracMaxX, WeightTileRes, WLoX, WHiX);
				const bool bWFitY = TexelSpan(FracMinY, FracMaxY, WeightTileRes, WLoY, WHiY);
				Layout.bSubRectClamped |= !bWFitX || !bWFitY;

				WeightLo.Add(FIntPoint(WLoX, WLoY));
				WeightSize.X = FMath::Max(WeightSize.X, WHiX - WLoX + 1);
				WeightSize.Y = FMath::Max(WeightSize.Y, WHiY - WLoY + 1);
			}
			else
			{
				WeightLo.Add(FIntPoint::ZeroValue);
			}
		}
	}

	HeightSize.X = FMath::Min(HeightSize.X, HeightTileRes);
	HeightSize.Y = FMath::Min(HeightSize.Y, HeightTileRes);
	if (bHasWeights)
	{
		WeightSize.X = FMath::Min(WeightSize.X, WeightTileRes);
		WeightSize.Y = FMath::Min(WeightSize.Y, WeightTileRes);
	}
	else
	{
		WeightSize = FIntPoint(1, 1);
	}

	Layout.HeightSubRect = HeightSize;
	Layout.WeightSubRect = WeightSize;

	// Clamp each origin so origin + size stays inside its tile. Size >= that slot's own required span,
	// so the clamped window still covers it.
	Layout.Slots.Reserve(CountX * CountY);
	for (int32 LocalY = 0; LocalY < CountY; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < CountX; ++LocalX)
		{
			const int32 Index = LocalY * CountX + LocalX;

			FArcEcotopeFoliageAtlasSlot Slot;
			Slot.Key = FIntPoint(MinCellX + LocalX, MinCellY + LocalY);
			Slot.Slice = Index;
			Slot.HeightSrcOrigin = FIntPoint(
				FMath::Clamp(HeightLo[Index].X, 0, HeightTileRes - HeightSize.X),
				FMath::Clamp(HeightLo[Index].Y, 0, HeightTileRes - HeightSize.Y));
			Slot.WeightSrcOrigin = bHasWeights
				? FIntPoint(
					FMath::Clamp(WeightLo[Index].X, 0, WeightTileRes - WeightSize.X),
					FMath::Clamp(WeightLo[Index].Y, 0, WeightTileRes - WeightSize.Y))
				: FIntPoint::ZeroValue;

			Layout.Slots.Add(Slot);
		}
	}

	return Layout;
}

bool BuildTerrainData(AArcTerrain& Terrain, const FBox& WorldBounds, FName LayerChannel,
	FArcEcotopeFoliageTerrainData& OutData)
{
	using namespace ArcEcotopeFoliageAtlasPrivate;

	check(IsInGameThread());

	OutData = FArcEcotopeFoliageTerrainData();

	const FArcTerrainConfig& Config = Terrain.Config;
	if (!WorldBounds.IsValid || !FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
	{
		return false;
	}

	const FVector LocalCornerA = Terrain.WorldToTerrainLocal(WorldBounds.Min);
	const FVector LocalCornerB = Terrain.WorldToTerrainLocal(WorldBounds.Max);
	FBox LocalBounds(LocalCornerA.ComponentMin(LocalCornerB), LocalCornerA.ComponentMax(LocalCornerB));

	// MANDATORY CLAMP: the terrain is an infinite plane and the cell math applies no bound, so an
	// oversized box would enumerate a rectangle limited only by int32.
	const double MaxSpan = static_cast<double>(GArcEcotopeFoliageMaxCells) * Config.CellSize();
	LocalBounds.Max.X = FMath::Min(LocalBounds.Max.X, LocalBounds.Min.X + MaxSpan);
	LocalBounds.Max.Y = FMath::Min(LocalBounds.Max.Y, LocalBounds.Min.Y + MaxSpan);

	FArcTerrainTileGrid HeightGrid;
	if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, 0, HeightGrid))
	{
		return false;
	}

	// The rasters come from the terrain's LATTICE, not from a reference payload: every cell of a rail
	// carries the same apron-inclusive resolution by construction, so nothing has to be sampled to learn
	// it and a cell that disagrees cannot exist.
	const int32 HeightTileRes = Config.QuadsPerCell() + 3;
	if (HeightTileRes < 4)
	{
		return false;
	}

	bool bAuthoringError = false;
	OutData.LayerChannel = ResolveLayerFilterChannel(LayerChannel, Config.ChannelSet, bAuthoringError);

	if (bAuthoringError)
	{
		// Not "filter disabled": a declared channel that does not resolve must not silently paint the
		// whole terrain. No cell is eligible.
		UE_LOGF(LogArcEcotope, Warning,"[FoliageAtlas] Layer channel '%ls' does not resolve against terrain '%ls' (no channel set, or "
				 "the name is absent). No cell is eligible.",
			*LayerChannel.ToString(), *Terrain.GetName());
		return false;
	}

	// The tile names its planes by channel-name hash, never by the global index resolved above.
	const uint32 LayerChannelHash = ArcTerrain::ChannelNameHash(LayerChannel);

	FArcTerrainTileGrid WeightGrid;
	int32 WeightTileRes = 0;
	if (OutData.LayerChannel >= 0)
	{
		if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Weight, 0, WeightGrid))
		{
			return false;
		}
		WeightTileRes = WeightGrid.TileQuads / WeightGrid.CellsPerTileEdge + 3;
		if (WeightTileRes < 4)
		{
			return false;
		}
	}

	const FArcEcotopeFoliageAtlasLayout Layout = ComputeAtlasLayout(LocalBounds, Config.CellSize(),
		HeightTileRes, WeightTileRes, GArcEcotopeFoliageMaxCells);
	if (Layout.Slots.IsEmpty())
	{
		return false;
	}

	if (Layout.bCellRectClamped)
	{
		UE_LOGF(LogArcEcotope, Warning,"[FoliageAtlas] Patch spans %dx%d cells of terrain '%ls' but only %d atlas slots exist; "
				 "%dx%d cells are covered and the remainder gets no foliage.",
			Layout.RequestedCellCount.X, Layout.RequestedCellCount.Y, *Terrain.GetName(),
			GArcEcotopeFoliageMaxCells, Layout.CellCount.X, Layout.CellCount.Y);
	}

	OutData.AtlasOriginCell = Layout.OriginCell;
	OutData.AtlasCellCount = Layout.CellCount;
	OutData.HeightSubRect = Layout.HeightSubRect;
	OutData.WeightSubRect = Layout.WeightSubRect;
	OutData.CellSize = static_cast<float>(Config.CellSize());
	OutData.HeightTileRes = static_cast<float>(HeightTileRes);
	OutData.WeightTileRes = static_cast<float>(WeightTileRes);
	OutData.TexelToLocal = static_cast<float>(Config.CellSize() / static_cast<double>(HeightTileRes - 3));

	SplitTerrainTransformLWC(Terrain.GetTerrainTransform(), OutData.LWCTile, OutData.TerrainLocalToWorld,
		OutData.WorldToTerrainLocal, OutData.TerrainInvNonUniformScale);

	int32 NumResident = 0;
	bool bAnyHeight = false;

	OutData.Slots.SetNum(Layout.Slots.Num());
	for (int32 SlotIndex = 0; SlotIndex < Layout.Slots.Num(); ++SlotIndex)
	{
		FArcEcotopeFoliageTerrainData::FSlot& Slot = OutData.Slots[SlotIndex];
		Slot.HeightSrcOrigin = Layout.Slots[SlotIndex].HeightSrcOrigin;
		Slot.WeightSrcOrigin = Layout.Slots[SlotIndex].WeightSrcOrigin;

		const FArcTerrainCellKey CellKey = Layout.Slots[SlotIndex].Key;

		// FindLoadedTile, NEVER a sync load: this runs on the game thread every tick, and a miss means
		// "that area has not arrived", which the layer waits one tick on.
		FIntPoint HeightTileKey = FIntPoint::ZeroValue;
		if (!ArcTerrainTileAccess::TileKeyFromCell(Terrain, EArcTerrainRail::Height, 0, CellKey, HeightTileKey))
		{
			continue;
		}
		const FArcTerrainTilePayloadPtr HeightPayload =
			ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Height, 0, HeightTileKey);
		if (!HeightPayload.IsValid())
		{
			continue;
		}

		TArray<float> Heights;
		int32 CellResolution = 0;
		if (!ArcTerrainTileAccess::ExtractCellFloats(HeightGrid, *HeightPayload, 0, CellKey, Heights,
				CellResolution)
			|| CellResolution != HeightTileRes)
		{
			continue;
		}

		float MinHeight = TNumericLimits<float>::Max();
		float MaxHeight = TNumericLimits<float>::Lowest();
		for (const float HeightCm : Heights)
		{
			MinHeight = FMath::Min(MinHeight, HeightCm);
			MaxHeight = FMath::Max(MaxHeight, HeightCm);
		}

		TArray<uint8> HeightBytes;
		HeightBytes.SetNumUninitialized(Heights.Num() * sizeof(float));
		FMemory::Memcpy(HeightBytes.GetData(), Heights.GetData(), HeightBytes.Num());

		FArcEcotopeFoliageTileTexturePtr HeightTile = UploadTileTexture(MoveTemp(HeightBytes), HeightTileRes,
			sizeof(float), PF_R32_FLOAT, TEXT("ArcEcotopeFoliage_HeightTile"));
		if (!HeightTile.IsValid())
		{
			continue;
		}

		if (OutData.LayerChannel >= 0)
		{
			// ELIGIBILITY: a tile with no plane for this channel means "not painted here", and a tile that
			// has not arrived means "not known yet". Neither may become "no filter".
			FIntPoint WeightTileKey = FIntPoint::ZeroValue;
			if (!ArcTerrainTileAccess::TileKeyFromCell(Terrain, EArcTerrainRail::Weight, 0, CellKey,
				WeightTileKey))
			{
				continue;
			}
			const FArcTerrainTilePayloadPtr WeightPayload =
				ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Weight, 0, WeightTileKey);
			if (!WeightPayload.IsValid())
			{
				continue;
			}

			const int32 PlaneIndex = WeightPayload->FindWeightPlane(LayerChannelHash);
			if (PlaneIndex == INDEX_NONE)
			{
				continue;
			}

			TArray<uint8> WeightBytes;
			int32 WeightCellResolution = 0;
			if (!ArcTerrainTileAccess::ExtractCellPlane(WeightGrid, *WeightPayload, PlaneIndex, CellKey,
					WeightBytes, WeightCellResolution)
				|| WeightCellResolution != WeightTileRes)
			{
				continue;
			}

			FArcEcotopeFoliageTileTexturePtr WeightTile = UploadTileTexture(MoveTemp(WeightBytes),
				WeightTileRes, 1, PF_G8, TEXT("ArcEcotopeFoliage_WeightTile"));
			if (!WeightTile.IsValid())
			{
				continue;
			}

			Slot.WeightTile = MoveTemp(WeightTile);
			// One plane per upload, so the copy always reads slice 0 of its own texture.
			Slot.WeightSourceSlice = 0;
		}

		if (!bAnyHeight)
		{
			OutData.LocalHeightMinCm = MinHeight;
			OutData.LocalHeightMaxCm = MaxHeight;
			bAnyHeight = true;
		}
		else
		{
			OutData.LocalHeightMinCm = FMath::Min(OutData.LocalHeightMinCm, MinHeight);
			OutData.LocalHeightMaxCm = FMath::Max(OutData.LocalHeightMaxCm, MaxHeight);
		}

		Slot.HeightTile = MoveTemp(HeightTile);
		Slot.bResident = true;
		++NumResident;
	}

	// Verbose because a moving camera re-dispatches every frame; zero resident stays loud, since an empty
	// atlas reporting success is what this line exists to catch.
	if (NumResident > 0)
	{
		UE_LOGF(LogArcEcotope, Verbose,"[FoliageAtlas] terrain='%ls' cells=%d resident of %d slots (atlas %dx%d)",
			*Terrain.GetName(), NumResident, Layout.Slots.Num(), Layout.CellCount.X, Layout.CellCount.Y);
	}
	else
	{
		UE_LOGF(LogArcEcotope, Warning,"[FoliageAtlas] terrain='%ls' cells=%d resident of %d slots (atlas %dx%d)",
			*Terrain.GetName(), NumResident, Layout.Slots.Num(), Layout.CellCount.X, Layout.CellCount.Y);
	}

	return NumResident > 0;
}

void BuildTerrainAtlas(FRDGBuilder& GraphBuilder, const FArcEcotopeFoliageTerrainData& Data,
	FArcEcotopeFoliageTerrainParameters& OutParameters)
{
	using namespace ArcEcotopeFoliageAtlasPrivate;

	FRDGTextureSRVRef HeightAtlasSRV = nullptr;
	FRDGTextureSRVRef WeightAtlasSRV = nullptr;
	uint32 ResidencyMask = 0;

	const int32 NumSlots = FMath::Min(Data.Slots.Num(), GArcEcotopeFoliageMaxCells);

	if (NumSlots > 0 && Data.HeightSubRect.X > 0 && Data.HeightSubRect.Y > 0)
	{
		// PF_R32_FLOAT matches the baked HeightTexture — a copy requires format identity. And
		// TargetArraySlicesIndependently is required, not an optimisation: AddClearRenderTargetPass with
		// NumSlices > 1 hits an RDG checkf without it.
		FRDGTextureRef HeightAtlas = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(Data.HeightSubRect, PF_R32_FLOAT, FClearValueBinding::Black,
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable
					| ETextureCreateFlags::TargetArraySlicesIndependently,
				NumSlots),
			TEXT("ArcEcotopeFoliage_HeightAtlas"));

		FRDGTextureClearInfo ClearInfo;
		ClearInfo.FirstSliceIndex = 0;
		ClearInfo.NumSlices = static_cast<uint32>(NumSlots);
		AddClearRenderTargetPass(GraphBuilder, HeightAtlas, ClearInfo);

		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			const FArcEcotopeFoliageTerrainData::FSlot& Slot = Data.Slots[SlotIndex];
			if (!Slot.bResident)
			{
				continue;
			}

			FRDGTextureRef Source = RegisterTileSource(GraphBuilder, Slot.HeightTile,
				Slot.HeightSrcOrigin, Data.HeightSubRect, PF_R32_FLOAT,
				TEXT("ArcEcotopeFoliage_SrcHeightTile"));
			if (Source == nullptr)
			{
				continue;
			}

			// One slice per cell removes cross-cell bilinear bleed as a category, which is why no
			// per-slot UV clamp exists anywhere below.
			CopySubRect(GraphBuilder, Source, HeightAtlas, Slot.HeightSrcOrigin, Data.HeightSubRect,
				/*SrcSlice=*/0, SlotIndex);
			ResidencyMask |= 1u << static_cast<uint32>(SlotIndex);
		}

		HeightAtlasSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(HeightAtlas));
	}

	bool bAnyWeightSlot = false;
	for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
	{
		bAnyWeightSlot |= Data.Slots[SlotIndex].WeightTile.IsValid();
	}

	if (NumSlots > 0 && bAnyWeightSlot && Data.WeightSubRect.X > 0 && Data.WeightSubRect.Y > 0)
	{
		FRDGTextureRef WeightAtlas = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(Data.WeightSubRect, PF_G8, FClearValueBinding::Black,
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable
					| ETextureCreateFlags::TargetArraySlicesIndependently,
				NumSlots),
			TEXT("ArcEcotopeFoliage_WeightAtlas"));

		FRDGTextureClearInfo ClearInfo;
		ClearInfo.FirstSliceIndex = 0;
		ClearInfo.NumSlices = static_cast<uint32>(NumSlots);
		AddClearRenderTargetPass(GraphBuilder, WeightAtlas, ClearInfo);

		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			const FArcEcotopeFoliageTerrainData::FSlot& Slot = Data.Slots[SlotIndex];
			if (!Slot.bResident || !Slot.WeightTile.IsValid() || Slot.WeightSourceSlice < 0)
			{
				continue;
			}

			FRDGTextureRef Source = RegisterTileSource(GraphBuilder, Slot.WeightTile,
				Slot.WeightSrcOrigin, Data.WeightSubRect, PF_G8,
				TEXT("ArcEcotopeFoliage_SrcWeightTile"));
			if (Source == nullptr)
			{
				continue;
			}

			if (Slot.WeightSourceSlice >= static_cast<int32>(Source->Desc.ArraySize))
			{
				continue;
			}

			CopySubRect(GraphBuilder, Source, WeightAtlas, Slot.WeightSrcOrigin, Data.WeightSubRect,
				Slot.WeightSourceSlice, SlotIndex);
		}

		WeightAtlasSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(WeightAtlas));
	}

	// A bound-but-dead resource, never a null binding.
	if (HeightAtlasSRV == nullptr)
	{
		HeightAtlasSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(GSystemTextures.GetDefaultTexture(
			GraphBuilder, ETextureDimension::Texture2DArray, PF_R32_FLOAT, 0.0f)));
	}

	if (WeightAtlasSRV == nullptr)
	{
		WeightAtlasSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(GSystemTextures.GetDefaultTexture(
			GraphBuilder, ETextureDimension::Texture2DArray, PF_R32_FLOAT, 0.0f)));
	}

	FRHISamplerState* BilinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	FRHISamplerState* PointSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Filterability is a per-adapter runtime result; on the point fallback the central difference
	// degrades to a texel-quantized difference.
	FRHISamplerState* HeightSampler = RHIPixelFormatHasCapabilities(EPixelFormat::PF_R32_FLOAT,
		EPixelFormatCapabilities::TextureFilterable) ? BilinearSampler : PointSampler;

	OutParameters.ArcTerrainHeightAtlas = HeightAtlasSRV;
	OutParameters.ArcTerrainHeightAtlasSampler = HeightSampler;
	OutParameters.ArcTerrainWeightAtlas = WeightAtlasSRV;
	OutParameters.ArcTerrainWeightAtlasSampler = BilinearSampler;
	OutParameters.ArcTerrainResidencyMask = ResidencyMask;
	OutParameters.ArcTerrainWorldToLocal = Data.WorldToTerrainLocal;
	OutParameters.ArcTerrainLocalToWorld = Data.TerrainLocalToWorld;
	OutParameters.ArcTerrainInvNonUniformScale = Data.TerrainInvNonUniformScale;
	OutParameters.ArcTerrainLWCTile = Data.LWCTile;
	OutParameters.ArcTerrainAtlasOriginCell = Data.AtlasOriginCell;
	OutParameters.ArcTerrainAtlasCellCount = Data.AtlasCellCount;
	OutParameters.ArcTerrainHeightSubRectInv = FVector2f(
		Data.HeightSubRect.X > 0 ? 1.0f / static_cast<float>(Data.HeightSubRect.X) : 0.0f,
		Data.HeightSubRect.Y > 0 ? 1.0f / static_cast<float>(Data.HeightSubRect.Y) : 0.0f);
	OutParameters.ArcTerrainWeightSubRectInv = FVector2f(
		Data.WeightSubRect.X > 0 ? 1.0f / static_cast<float>(Data.WeightSubRect.X) : 0.0f,
		Data.WeightSubRect.Y > 0 ? 1.0f / static_cast<float>(Data.WeightSubRect.Y) : 0.0f);
	OutParameters.ArcTerrainCellSize = Data.CellSize;
	OutParameters.ArcTerrainHeightTileRes = Data.HeightTileRes;
	OutParameters.ArcTerrainWeightTileRes = Data.WeightTileRes;
	OutParameters.ArcTerrainTexelToLocal = Data.TexelToLocal;
	OutParameters.ArcTerrainLayerChannel = Data.LayerChannel;

	for (int32 SlotIndex = 0; SlotIndex < GArcEcotopeFoliageMaxCells; ++SlotIndex)
	{
		const bool bHasSlot = SlotIndex < NumSlots;
		const FIntPoint HeightOrigin = bHasSlot ? Data.Slots[SlotIndex].HeightSrcOrigin : FIntPoint::ZeroValue;
		const FIntPoint WeightOrigin = bHasSlot ? Data.Slots[SlotIndex].WeightSrcOrigin : FIntPoint::ZeroValue;

		// .z carries "this cell carries the filter channel": the weight atlas holds one slice per slot,
		// so a cell without the channel must sample 0 rather than another cell's layer.
		const bool bHasWeight = bHasSlot && Data.Slots[SlotIndex].WeightTile.IsValid();

		OutParameters.ArcTerrainHeightSrcOriginPerSlot[SlotIndex] =
			FVector4f(static_cast<float>(HeightOrigin.X), static_cast<float>(HeightOrigin.Y), 0.0f, 0.0f);
		OutParameters.ArcTerrainWeightSrcOriginPerSlot[SlotIndex] =
			FVector4f(static_cast<float>(WeightOrigin.X), static_cast<float>(WeightOrigin.Y),
				bHasWeight ? 1.0f : 0.0f, 0.0f);
	}
}

} // namespace ArcEcotopeFoliageAtlas
