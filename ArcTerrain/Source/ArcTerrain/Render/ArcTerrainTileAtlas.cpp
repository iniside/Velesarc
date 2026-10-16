// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainTileAtlas.h"

#include "ArcTerrainChannelSet.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainModule.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "PixelShaderUtils.h"
#include "RHIGlobals.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

/** One PF_G8 baked weight slice -> one component of the BGRA8 atlas slice (colour-write mask picks it). */
class FArcTerrainAtlasRouteWeightPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcTerrainAtlasRouteWeightPS);
	SHADER_USE_PARAMETER_STRUCT(FArcTerrainAtlasRouteWeightPS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, InDestOrigin)
		SHADER_PARAMETER(uint32, InSourceSlice)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float>, InSourceWeight)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ATLAS_ROUTE_WEIGHT"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcTerrainAtlasRouteWeightPS,
	"/Plugin/ArcTerrain/ArcTerrainAtlasFill.usf", "RouteWeightPS", SF_Pixel);

bool FArcTerrainTileAtlasLayout::IsValid() const
{
	return HeightQuads > 0 && WeightQuads > 0 && MaxResidentSectors > 0 && MaxResidentWeightCells > 0
		&& NumWeightArrays > 0
		&& SectorShift >= 0 && WeightSliceShift >= 0 && WeightSliceShift <= SectorShift;
}

bool FArcTerrainTileAtlasLayout::operator==(const FArcTerrainTileAtlasLayout& Other) const
{
	return SectorShift == Other.SectorShift
		&& WeightSliceShift == Other.WeightSliceShift
		&& HeightQuads == Other.HeightQuads
		&& WeightQuads == Other.WeightQuads
		&& NumWeightArrays == Other.NumWeightArrays
		&& BaseWeightArrayIndex == Other.BaseWeightArrayIndex
		&& MaxResidentSectors == Other.MaxResidentSectors
		&& MaxResidentWeightCells == Other.MaxResidentWeightCells
		&& BaseHeightCm == Other.BaseHeightCm;
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainTileAtlasPrivate_6b13
{
	// Fallbacks for a not-yet-initialised RHI (commandlets, early startup): the D3D12 limits, which are
	// the smallest of the platforms this project targets.
	constexpr int32 FallbackMaxTextureDimension = 16384;
	constexpr int32 FallbackMaxArrayLayers = 2048;

	int32 ResolveMaxTextureDimension()
	{
		const int32 Reported = static_cast<int32>(GetMax2DTextureDimension());
		return Reported > 0 ? Reported : FallbackMaxTextureDimension;
	}

	int32 GHeightAtlasBudgetMB = 512;
	static FAutoConsoleVariableRef CVarHeightAtlasBudgetMB(
		TEXT("arc.Terrain.Atlas.HeightBudgetMB"),
		GHeightAtlasBudgetMB,
		TEXT("Ceiling, in MB, on the height atlas array (allocatable slices + the reserved fill slice). ")
		TEXT("MaxResidentSectors is trimmed to fit for this session only; the terrain asset is not written."),
		ECVF_Default);

	int32 GWeightAtlasBudgetMB = 512;
	static FAutoConsoleVariableRef CVarWeightAtlasBudgetMB(
		TEXT("arc.Terrain.Atlas.WeightBudgetMB"),
		GWeightAtlasBudgetMB,
		TEXT("Ceiling, in MB, on all NumWeightArrays weight atlas arrays together (allocatable slices + the ")
		TEXT("reserved fill slice). MaxResidentWeightCells is trimmed to fit for this session only."),
		ECVF_Default);

	double Megabytes(int64 Bytes)
	{
		return static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}

	int32 ResolveMaxArrayLayers()
	{
		const int32 Reported = static_cast<int32>(GetMaxTextureArrayLayers());
		return Reported > 0 ? Reported : FallbackMaxArrayLayers;
	}

	FRDGTextureRef RegisterSourceTexture(FRDGBuilder& GraphBuilder, const FTextureRHIRef& TextureRHI,
		EPixelFormat ExpectedFormat, FIntPoint RequiredSize, const TCHAR* Name)
	{
		if (!TextureRHI.IsValid())
		{
			return nullptr;
		}

		// A UTexture still being compiled hands out a BGRA8 PLACEHOLDER from GetResource()->TextureRHI,
		// and a freshly baked section is exactly that for its first frames. Binding it as the declared
		// format is an illegal read; skip the source and pick it up once the real texture resolves.
		if (TextureRHI->GetFormat() != ExpectedFormat)
		{
			return nullptr;
		}

		const FIntPoint SourceSize = TextureRHI->GetSizeXY();
		if (SourceSize.X < RequiredSize.X || SourceSize.Y < RequiredSize.Y)
		{
			return nullptr;
		}

		return RegisterExternalTexture(GraphBuilder, TextureRHI, Name);
	}

	FRDGTextureRef RegisterSourceResource(FRDGBuilder& GraphBuilder, FTextureRenderTargetResource* Resource,
		EPixelFormat ExpectedFormat, FIntPoint RequiredSize, const TCHAR* Name)
	{
		if (Resource == nullptr)
		{
			return nullptr;
		}
		return RegisterSourceTexture(GraphBuilder, Resource->TextureRHI, ExpectedFormat, RequiredSize, Name);
	}

	void CopySubRectIntoSlice(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Dest,
		int32 SourceSlice, int32 DestSlice, FIntPoint SourceOrigin, FIntPoint DestOrigin, int32 Extent)
	{
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size = FIntVector(Extent, Extent, 1);
		CopyInfo.SourcePosition = FIntVector(SourceOrigin.X, SourceOrigin.Y, 0);
		CopyInfo.DestPosition = FIntVector(DestOrigin.X, DestOrigin.Y, 0);
		CopyInfo.SourceSliceIndex = static_cast<uint32>(SourceSlice);
		CopyInfo.DestSliceIndex = static_cast<uint32>(DestSlice);
		CopyInfo.NumSlices = 1;

		AddCopyTexturePass(GraphBuilder, Source, Dest, CopyInfo);
	}

	FRHIBlendState* GetSingleComponentBlendState(int32 Component)
	{
		switch (Component)
		{
		case 0:
			return TStaticBlendState<CW_RED>::GetRHI();
		case 1:
			return TStaticBlendState<CW_GREEN>::GetRHI();
		case 2:
			return TStaticBlendState<CW_BLUE>::GetRHI();
		default:
			return TStaticBlendState<CW_ALPHA>::GetRHI();
		}
	}

	FLinearColor MakeBaseWeightClearColor()
	{
		FLinearColor Clear(0.0f, 0.0f, 0.0f, 0.0f);
		Clear.Component(ArcTerrain::GBaseChannelIndex % 4) = 1.0f;
		return Clear;
	}

	// Half-width ceiling on a residency window, IN THAT RING'S OWN KEY UNITS: 64 sectors for the height
	// ring, 64 weight groups for the weight ring, which is 2^GroupShift times tighter in world terms.
	// A window only has to cover the resident set and a wider one is pure upload; 64 leaves room for a
	// pin far outside the camera's radius and still costs at most 129^2 * 4 B = 66 KB.
	constexpr int32 MaxResidencyRingRadius = 64;

	const TCHAR* HeightRingName(int32 Tier)
	{
		switch (Tier)
		{
		case 0:  return TEXT("tier 0 height sector");
		case 1:  return TEXT("tier 1 height sector");
		case 2:  return TEXT("tier 2 height sector");
		default: return TEXT("tier 3 height sector");
		}
	}

	const TCHAR* WeightRingName(int32 Tier)
	{
		switch (Tier)
		{
		case 0:  return TEXT("tier 0 weight group");
		case 1:  return TEXT("tier 1 weight group");
		case 2:  return TEXT("tier 2 weight group");
		default: return TEXT("tier 3 weight group");
		}
	}

	/** Square window over the resident keys, centred on Center, every other key = FillSlice. The centre is
	 *  the caller's anchor (the camera's own key), NOT the bounding box: one pinned outlier would drag a
	 *  box centre away far enough for the clamp to exclude the camera's own sectors. */
	void BuildResidencyRing(const TArray<TPair<FIntPoint, int32>>& Entries, FIntPoint Center,
		const TCHAR* RingName, bool& bWarnedRingClamped, FArcTerrainResidencyRing& OutRing)
	{
		OutRing.Slices.Reset();
		OutRing.Center = FIntPoint::ZeroValue;
		OutRing.Radius = -1;
		if (Entries.IsEmpty())
		{
			return;
		}

		int32 WantedRadius = 0;
		for (const TPair<FIntPoint, int32>& Entry : Entries)
		{
			WantedRadius = FMath::Max(WantedRadius,
				FMath::Max(FMath::Abs(Entry.Key.X - Center.X), FMath::Abs(Entry.Key.Y - Center.Y)));
		}

		if (WantedRadius > MaxResidencyRingRadius && !bWarnedRingClamped)
		{
			bWarnedRingClamped = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[TileAtlas] The resident set spans %d %ls keys from its centre; the residency window is "
				"clamped to %d. Keys outside it read the fill slice, so that terrain draws flat.",
				WantedRadius, RingName, MaxResidencyRingRadius);
		}

		OutRing.Center = Center;
		OutRing.Radius = FMath::Min(WantedRadius, MaxResidencyRingRadius);

		const int32 Side = OutRing.GetSide();
		OutRing.Slices.Init(OutRing.FillSlice, Side * Side);
		for (const TPair<FIntPoint, int32>& Entry : Entries)
		{
			const FIntPoint Offset = Entry.Key - Center + FIntPoint(OutRing.Radius, OutRing.Radius);
			if (Offset.X < 0 || Offset.Y < 0 || Offset.X >= Side || Offset.Y >= Side)
			{
				continue;
			}
			OutRing.Slices[Offset.Y * Side + Offset.X] = static_cast<uint32>(Entry.Value);
		}
	}

	FRDGTextureRef CreateOrRegisterAtlas(FRDGBuilder& GraphBuilder, TRefCountPtr<IPooledRenderTarget>& Pooled,
		FIntPoint Extent, EPixelFormat Format, const FClearValueBinding& ClearValue, int32 ArraySize,
		const TCHAR* Name)
	{
		if (Pooled.IsValid())
		{
			return GraphBuilder.RegisterExternalTexture(Pooled);
		}

		// TargetArraySlicesIndependently is a REQUIREMENT, not an optimisation: AddClearRenderTargetPass
		// with NumSlices > 1 hits an RDG checkf without it, and binding one slice as a render target needs
		// it too. MultiFrame keeps the resource alive across the ConvertToExternalTexture boundary.
		return GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(Extent, Format, ClearValue,
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable
					| ETextureCreateFlags::TargetArraySlicesIndependently,
				ArraySize, /*NumMips*/ 1),
			Name, ERDGTextureFlags::MultiFrame);
	}
}

int64 ArcTerrainAtlas::GetHeightBudgetBytes()
{
	return static_cast<int64>(FMath::Max(ArcTerrainTileAtlasPrivate_6b13::GHeightAtlasBudgetMB, 1)) * 1024 * 1024;
}

int64 ArcTerrainAtlas::GetWeightBudgetBytes()
{
	return static_cast<int64>(FMath::Max(ArcTerrainTileAtlasPrivate_6b13::GWeightAtlasBudgetMB, 1)) * 1024 * 1024;
}

FArcTerrainTileAtlasLayout ArcTerrainAtlas::ResolveTileAtlasLayout(int32 HeightQuads, int32 WeightQuads,
	int32 NumChannels, int32 MaxResidentSectors, int32 MaxResidentWeightCells, int32 DesiredSectorShift,
	int32 DesiredWeightSliceShift, float BaseHeightCm, bool bEmitWarnings)
{
	using namespace ArcTerrainTileAtlasPrivate_6b13;

	FArcTerrainTileAtlasLayout Layout;
	Layout.HeightQuads = FMath::Max(HeightQuads, 1);
	Layout.WeightQuads = FMath::Max(WeightQuads, Layout.HeightQuads);
	Layout.NumWeightArrays = FMath::Max(FMath::DivideAndRoundUp(NumChannels, 4), 1);
	const int32 BaseChannelIndex = ArcTerrain::ResolveBaseChannelIndex(NumChannels);
	Layout.BaseWeightArrayIndex = (BaseChannelIndex != INDEX_NONE) ? (BaseChannelIndex / 4) : INDEX_NONE;
	Layout.MaxResidentSectors = FMath::Max(MaxResidentSectors, 1);
	Layout.MaxResidentWeightCells = FMath::Max(MaxResidentWeightCells, 1);
	Layout.SectorShift = FMath::Clamp(DesiredSectorShift, 0, 6);
	Layout.WeightSliceShift = FMath::Clamp(DesiredWeightSliceShift, 0, Layout.SectorShift);
	Layout.BaseHeightCm = BaseHeightCm;

	const int32 MaxDimension = ResolveMaxTextureDimension();

	// The dimension check MUST span both arrays: ResolveWeightQuads floors WeightQuads at HeightQuads, so
	// the weight side is the larger one whenever WeightSliceShift == SectorShift, and its 4093 ceiling
	// stops binding entirely once HeightQuads exceeds it (FMath::Clamp is Max(Min(X, Max), Min), so a
	// Min > Max pair returns Min — the lower bound wins).
	static bool bWarnedSectorShiftLowered = false;
	while (Layout.SectorShift > 0 && Layout.GetHeightSliceSide() > MaxDimension)
	{
		--Layout.SectorShift;
		Layout.WeightSliceShift = FMath::Min(Layout.WeightSliceShift, Layout.SectorShift);
		if (bEmitWarnings && !bWarnedSectorShiftLowered)
		{
			bWarnedSectorShiftLowered = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[TileAtlas] Height slice side exceeds the %d texel device limit at QuadsPerCell %d; "
				"SectorShift lowered to %d. Raise the limit or lower QuadsPerCell to keep 4x4 sectors.",
				MaxDimension, Layout.HeightQuads, Layout.SectorShift);
		}
	}

	static bool bWarnedWeightShiftLowered = false;
	while (Layout.WeightSliceShift > 0 && Layout.GetWeightSliceSide() > MaxDimension)
	{
		--Layout.WeightSliceShift;
		if (bEmitWarnings && !bWarnedWeightShiftLowered)
		{
			bWarnedWeightShiftLowered = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[TileAtlas] Weight slice side exceeds the %d texel device limit at WeightQuads %d; "
				"WeightSliceShift lowered to %d.",
				MaxDimension, Layout.WeightQuads, Layout.WeightSliceShift);
		}
	}

	// Both sides are still over the limit at shift 0 == the RASTER itself does not fit. Nothing the atlas
	// can lower; the layout reads invalid and no sector is ever admitted.
	static bool bWarnedRasterTooLarge = false;
	if (Layout.GetHeightSliceSide() > MaxDimension || Layout.GetWeightSliceSide() > MaxDimension)
	{
		if (bEmitWarnings && !bWarnedRasterTooLarge)
		{
			bWarnedRasterTooLarge = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[TileAtlas] One cell's raster (height %d, weight %d texels) already exceeds the %d "
				"texel device limit — the terrain draws nothing until QuadsPerCell / the weight texel "
				"size come down.",
				Layout.GetHeightSliceSide(), Layout.GetWeightSliceSide(), MaxDimension);
		}
		Layout.MaxResidentSectors = 0;
		Layout.MaxResidentWeightCells = 0;
		return Layout;
	}

	// Array-layer ceiling, applied to EACH budget separately. The NumWeightArrays weight arrays are
	// PARALLEL — one array per four channels, each of GetWeightArraySize() slices — so the device limit is
	// per array and dividing by the array count would invent a limit no device has. Both arrays spend one
	// layer on their reserved fill slice, so the budget is MaxLayers - 1.
	const int32 MaxLayers = ResolveMaxArrayLayers();
	const int32 AllocatableLayers = FMath::Max(MaxLayers - 1, 1);
	static bool bWarnedSectorLayerCapped = false;
	if (Layout.MaxResidentSectors > AllocatableLayers)
	{
		if (bEmitWarnings && !bWarnedSectorLayerCapped)
		{
			bWarnedSectorLayerCapped = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[TileAtlas] MaxResidentSectors %d exceeds the %d height array layers the device allows; "
				"lowered to %d.",
				Layout.MaxResidentSectors, MaxLayers, AllocatableLayers);
		}
		Layout.MaxResidentSectors = AllocatableLayers;
	}
	static bool bWarnedWeightLayerCapped = false;
	if (Layout.MaxResidentWeightCells > AllocatableLayers)
	{
		if (bEmitWarnings && !bWarnedWeightLayerCapped)
		{
			bWarnedWeightLayerCapped = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[TileAtlas] MaxResidentWeightCells %d exceeds the %d weight array layers the device "
				"allows; lowered to %d.",
				Layout.MaxResidentWeightCells, MaxLayers, AllocatableLayers);
		}
		Layout.MaxResidentWeightCells = AllocatableLayers;
	}

	// BYTE ceiling on each array, which the layer ceiling does not imply: an array is ONE committed
	// resource, so the driver fails the whole allocation thousands of layers before the layer limit. The
	// trim lives here — per session, against the resolved slice side — and never touches the authored
	// property.
	const int32 RequestedSectors = Layout.MaxResidentSectors;
	const int64 HeightSliceCost = HeightSliceBytes(Layout.GetHeightSliceSide());
	const int64 HeightBudget = GetHeightBudgetBytes();
	const int32 HeightSlicesInBudget = MaxAllocatableSlicesWithinBudget(HeightSliceCost, HeightBudget);
	static bool bWarnedHeightSliceOverBudget = false;
	if (HeightSlicesInBudget < 1)
	{
		if (bEmitWarnings && !bWarnedHeightSliceOverBudget)
		{
			bWarnedHeightSliceOverBudget = true;
			UE_LOGF(LogArcTerrain, Error,
				"[TileAtlas] One height slice is %d^2 x 4 B = %.1f MB, so not even one slice plus the "
				"reserved fill slice fits arc.Terrain.Atlas.HeightBudgetMB (%d MB). MaxResidentSectors "
				"%d cannot be honoured — the terrain allocates nothing and draws nothing. Lower "
				"CellExtent / SectorShift or coarsen TexelSize, or raise the budget.",
				Layout.GetHeightSliceSide(), Megabytes(HeightSliceCost), GHeightAtlasBudgetMB,
				RequestedSectors);
		}
		Layout.MaxResidentSectors = 0;
		Layout.MaxResidentWeightCells = 0;
		return Layout;
	}
	static bool bWarnedSectorsOverBudget = false;
	if (Layout.MaxResidentSectors > HeightSlicesInBudget)
	{
		if (bEmitWarnings && !bWarnedSectorsOverBudget)
		{
			bWarnedSectorsOverBudget = true;
			UE_LOGF(LogArcTerrain, Error,
				"[TileAtlas] MaxResidentSectors %d asks for %.1f MB of height atlas in ONE allocation "
				"(%d slices + 1 fill slice at %d^2 x 4 B = %.1f MB each), past "
				"arc.Terrain.Atlas.HeightBudgetMB (%d MB). TRIMMED to %d for this session = %.1f MB. "
				"Nothing is written to the asset — this %d is the SUM of the actor's own "
				"MaxResidentSectors and the sectors each HeightTiers entry's band covers, so "
				"lower MaxResidentSectors, shorten an entry's Extent or remove an entry (to %d total), or "
				"lower CellExtent / SectorShift, or raise the budget.",
				RequestedSectors, Megabytes(HeightSliceCost * (static_cast<int64>(RequestedSectors) + 1)),
				RequestedSectors, Layout.GetHeightSliceSide(), Megabytes(HeightSliceCost),
				GHeightAtlasBudgetMB, HeightSlicesInBudget,
				Megabytes(HeightSliceCost * (static_cast<int64>(HeightSlicesInBudget) + 1)),
				RequestedSectors, HeightSlicesInBudget);
		}
		Layout.MaxResidentSectors = HeightSlicesInBudget;
	}

	const int32 RequestedWeightCells = Layout.MaxResidentWeightCells;
	const int64 WeightSliceCost = WeightSliceBytes(Layout.GetWeightSliceSide(), Layout.NumWeightArrays);
	const int64 WeightBudget = GetWeightBudgetBytes();
	const int32 WeightSlicesInBudget = MaxAllocatableSlicesWithinBudget(WeightSliceCost, WeightBudget);
	static bool bWarnedWeightSliceOverBudget = false;
	if (WeightSlicesInBudget < 1)
	{
		if (bEmitWarnings && !bWarnedWeightSliceOverBudget)
		{
			bWarnedWeightSliceOverBudget = true;
			UE_LOGF(LogArcTerrain, Error,
				"[TileAtlas] One weight slice is %d^2 x 4 B x %d arrays = %.1f MB, so not even one "
				"slice plus the reserved fill slice fits arc.Terrain.Atlas.WeightBudgetMB (%d MB). "
				"MaxResidentWeightCells %d cannot be honoured — the terrain allocates nothing and draws "
				"nothing. Lower WeightSliceShift or coarsen Weight Texel Size, or raise the budget.",
				Layout.GetWeightSliceSide(), Layout.NumWeightArrays, Megabytes(WeightSliceCost),
				GWeightAtlasBudgetMB, RequestedWeightCells);
		}
		Layout.MaxResidentSectors = 0;
		Layout.MaxResidentWeightCells = 0;
		return Layout;
	}
	static bool bWarnedWeightCellsOverBudget = false;
	if (Layout.MaxResidentWeightCells > WeightSlicesInBudget)
	{
		if (bEmitWarnings && !bWarnedWeightCellsOverBudget)
		{
			bWarnedWeightCellsOverBudget = true;
			UE_LOGF(LogArcTerrain, Error,
				"[TileAtlas] MaxResidentWeightCells %d asks for %.1f MB of weight atlas (%d slices + 1 "
				"fill slice at %d^2 x 4 B x %d arrays = %.1f MB each), past "
				"arc.Terrain.Atlas.WeightBudgetMB (%d MB). TRIMMED to %d for this session = %.1f MB. "
				"Nothing is written to the asset — set MaxResidentWeightCells on the terrain actor to "
				"%d, or lower WeightSliceShift, or raise the budget.",
				RequestedWeightCells,
				Megabytes(WeightSliceCost * (static_cast<int64>(RequestedWeightCells) + 1)),
				RequestedWeightCells, Layout.GetWeightSliceSide(), Layout.NumWeightArrays,
				Megabytes(WeightSliceCost), GWeightAtlasBudgetMB, WeightSlicesInBudget,
				Megabytes(WeightSliceCost * (static_cast<int64>(WeightSlicesInBudget) + 1)),
				WeightSlicesInBudget);
		}
		Layout.MaxResidentWeightCells = WeightSlicesInBudget;
	}

	return Layout;
}

namespace ArcTerrainTileAtlasHeightBoundsPrivate_4f2a
{
	FVector2f NormalizeRange(FVector2f Range)
	{
		return FMath::IsFinite(Range.X) && FMath::IsFinite(Range.Y) && Range.X <= Range.Y
			? Range
			: ArcTerrainTileAtlasRender::InvalidHeightRange();
	}

	int32 GetTier0CellsPerSlice(const FArcTerrainTileAtlasLayout& Layout)
	{
		const int32 CellsPerEdge = 1 << Layout.SectorShift;
		return CellsPerEdge * CellsPerEdge;
	}

	int32 GetTier0ContributorIndex(const FArcTerrainTileAtlasResources& Resources, int32 SliceIndex,
		FArcTerrainCellKey CellKey)
	{
		if (SliceIndex < 0 || SliceIndex >= Resources.Layout.GetHeightAllocatableSlices())
		{
			return INDEX_NONE;
		}

		const int32 CellsPerEdge = 1 << Resources.Layout.SectorShift;
		const int32 CellMask = CellsPerEdge - 1;
		const int32 LocalX = CellKey.X & CellMask;
		const int32 LocalY = CellKey.Y & CellMask;
		return SliceIndex * CellsPerEdge * CellsPerEdge + LocalY * CellsPerEdge + LocalX;
	}

	void Initialize(FArcTerrainTileAtlasResources& Resources)
	{
		const int32 NumSlices = Resources.Layout.GetHeightArraySize();
		const int32 CellsPerSlice = GetTier0CellsPerSlice(Resources.Layout);
		const FVector2f BaseRange(Resources.Layout.BaseHeightCm, Resources.Layout.BaseHeightCm);

		Resources.HeightSliceBounds.Init(BaseRange, NumSlices);
		Resources.HeightSliceBoundsModes.Init(
			FArcTerrainTileAtlasResources::EHeightBoundsMode::Tier0Contributors, NumSlices);
		Resources.Tier0HeightCellBounds.Init(BaseRange, NumSlices * CellsPerSlice);
		Resources.DirtyHeightSliceBounds.Init(false, NumSlices);
	}
}

FVector2f ArcTerrainTileAtlasRender::InvalidHeightRange()
{
	const float MaxFinite = TNumericLimits<float>::Max();
	return FVector2f(MaxFinite, -MaxFinite);
}

void ArcTerrainTileAtlasRender::ResetHeightSliceBounds(FArcTerrainTileAtlasResources& Resources,
	int32 SliceIndex)
{
	using namespace ArcTerrainTileAtlasHeightBoundsPrivate_4f2a;

	if (!Resources.HeightSliceBounds.IsValidIndex(SliceIndex)
		|| !Resources.HeightSliceBoundsModes.IsValidIndex(SliceIndex)
		|| !Resources.DirtyHeightSliceBounds.IsValidIndex(SliceIndex))
	{
		return;
	}

	const FVector2f BaseRange(Resources.Layout.BaseHeightCm, Resources.Layout.BaseHeightCm);
	const int32 CellsPerSlice = GetTier0CellsPerSlice(Resources.Layout);
	const int32 ContributorOffset = SliceIndex * CellsPerSlice;
	if (ContributorOffset < 0 || ContributorOffset + CellsPerSlice > Resources.Tier0HeightCellBounds.Num())
	{
		return;
	}

	Resources.HeightSliceBoundsModes[SliceIndex] =
		FArcTerrainTileAtlasResources::EHeightBoundsMode::Tier0Contributors;
	for (int32 CellIndex = 0; CellIndex < CellsPerSlice; ++CellIndex)
	{
		Resources.Tier0HeightCellBounds[ContributorOffset + CellIndex] = BaseRange;
	}
	Resources.DirtyHeightSliceBounds[SliceIndex] = true;
}

void ArcTerrainTileAtlasRender::ReplaceTier0HeightCellBounds(FArcTerrainTileAtlasResources& Resources,
	int32 SliceIndex, FArcTerrainCellKey CellKey, FVector2f HeightRange)
{
	using namespace ArcTerrainTileAtlasHeightBoundsPrivate_4f2a;

	const int32 ContributorIndex = GetTier0ContributorIndex(Resources, SliceIndex, CellKey);
	if (ContributorIndex == INDEX_NONE || !Resources.Tier0HeightCellBounds.IsValidIndex(ContributorIndex)
		|| !Resources.HeightSliceBoundsModes.IsValidIndex(SliceIndex)
		|| !Resources.DirtyHeightSliceBounds.IsValidIndex(SliceIndex))
	{
		return;
	}

	Resources.HeightSliceBoundsModes[SliceIndex] =
		FArcTerrainTileAtlasResources::EHeightBoundsMode::Tier0Contributors;
	Resources.Tier0HeightCellBounds[ContributorIndex] = NormalizeRange(HeightRange);
	Resources.DirtyHeightSliceBounds[SliceIndex] = true;
}

void ArcTerrainTileAtlasRender::ReplaceCoarseHeightSliceBounds(FArcTerrainTileAtlasResources& Resources,
	int32 SliceIndex, FVector2f HeightRange)
{
	using namespace ArcTerrainTileAtlasHeightBoundsPrivate_4f2a;

	if (!Resources.HeightSliceBounds.IsValidIndex(SliceIndex)
		|| !Resources.HeightSliceBoundsModes.IsValidIndex(SliceIndex)
		|| !Resources.DirtyHeightSliceBounds.IsValidIndex(SliceIndex))
	{
		return;
	}

	Resources.HeightSliceBoundsModes[SliceIndex] =
		FArcTerrainTileAtlasResources::EHeightBoundsMode::CoarseWholeSlice;
	Resources.HeightSliceBounds[SliceIndex] = NormalizeRange(HeightRange);
	Resources.DirtyHeightSliceBounds[SliceIndex] = true;
}

void ArcTerrainTileAtlasRender::InvalidateHeightCellBounds(FArcTerrainTileAtlasResources& Resources,
	int32 SliceIndex, FArcTerrainCellKey CellKey)
{
	using namespace ArcTerrainTileAtlasHeightBoundsPrivate_4f2a;

	if (!Resources.HeightSliceBoundsModes.IsValidIndex(SliceIndex)
		|| !Resources.HeightSliceBounds.IsValidIndex(SliceIndex)
		|| !Resources.DirtyHeightSliceBounds.IsValidIndex(SliceIndex))
	{
		return;
	}

	if (Resources.HeightSliceBoundsModes[SliceIndex]
		== FArcTerrainTileAtlasResources::EHeightBoundsMode::CoarseWholeSlice)
	{
		Resources.HeightSliceBounds[SliceIndex] = InvalidHeightRange();
		Resources.DirtyHeightSliceBounds[SliceIndex] = true;
		return;
	}

	const int32 ContributorIndex = GetTier0ContributorIndex(Resources, SliceIndex, CellKey);
	if (ContributorIndex != INDEX_NONE && Resources.Tier0HeightCellBounds.IsValidIndex(ContributorIndex))
	{
		Resources.Tier0HeightCellBounds[ContributorIndex] = InvalidHeightRange();
		Resources.DirtyHeightSliceBounds[SliceIndex] = true;
	}
}

void ArcTerrainTileAtlasRender::RecomputeHeightSliceBounds(FArcTerrainTileAtlasResources& Resources)
{
	using namespace ArcTerrainTileAtlasHeightBoundsPrivate_4f2a;

	const int32 CellsPerSlice = GetTier0CellsPerSlice(Resources.Layout);
	for (TConstSetBitIterator<> It(Resources.DirtyHeightSliceBounds); It; ++It)
	{
		const int32 SliceIndex = It.GetIndex();
		if (Resources.HeightSliceBoundsModes[SliceIndex]
			== FArcTerrainTileAtlasResources::EHeightBoundsMode::CoarseWholeSlice)
		{
			continue;
		}

		FVector2f Aggregate(TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
		const int32 ContributorOffset = SliceIndex * CellsPerSlice;
		for (int32 CellIndex = 0; CellIndex < CellsPerSlice; ++CellIndex)
		{
			const FVector2f Contributor =
				Resources.Tier0HeightCellBounds[ContributorOffset + CellIndex];
			if (!FMath::IsFinite(Contributor.X) || !FMath::IsFinite(Contributor.Y)
				|| Contributor.X > Contributor.Y)
			{
				Aggregate = InvalidHeightRange();
				break;
			}
			Aggregate.X = FMath::Min(Aggregate.X, Contributor.X);
			Aggregate.Y = FMath::Max(Aggregate.Y, Contributor.Y);
		}
		Resources.HeightSliceBounds[SliceIndex] = Aggregate;
	}
	Resources.DirtyHeightSliceBounds.Init(false, Resources.HeightSliceBounds.Num());
}

void ArcTerrainTileAtlasRender::ResolveStagingTextures(FRHICommandListImmediate& RHICmdList,
	FArcTerrainAtlasFillJob& Job)
{
	check(IsInRenderingThread());

	for (FArcTerrainAtlasCellFill& Fill : Job.Cells)
	{
		const int64 HeightBytes =
			static_cast<int64>(Fill.HeightTileRes) * Fill.HeightTileRes * sizeof(float);

		if (Fill.HeightStagingBytes.Num() == HeightBytes)
		{
			FTextureRHIRef Texture = ArcTerrainTileUpload::AcquireStaging2D(RHICmdList, PF_R32_FLOAT,
				Fill.HeightTileRes);
			if (Texture.IsValid())
			{
				ArcTerrainTileUpload::FillStaging2D(RHICmdList, Texture, Fill.HeightStagingBytes,
					Fill.HeightTileRes * sizeof(float));
				Fill.BakedHeightRHI = Texture;
				Job.Staging.Adopt(MoveTemp(Texture));
			}
		}

		if (!Fill.WeightStagingPlanes.IsEmpty())
		{
			const int64 PlaneBytes = static_cast<int64>(Fill.WeightTileRes) * Fill.WeightTileRes;
			FTextureRHIRef Texture = ArcTerrainTileUpload::AcquireStagingArray(RHICmdList, PF_G8,
				Fill.WeightTileRes, Fill.WeightStagingPlanes.Num());
			if (Texture.IsValid())
			{
				for (int32 PlaneIndex = 0; PlaneIndex < Fill.WeightStagingPlanes.Num(); ++PlaneIndex)
				{
					if (Fill.WeightStagingPlanes[PlaneIndex].Num() == PlaneBytes)
					{
						ArcTerrainTileUpload::FillStagingSlice(RHICmdList, Texture, PlaneIndex,
							Fill.WeightStagingPlanes[PlaneIndex], Fill.WeightTileRes);
					}
				}
				Fill.BakedWeightRHI = Texture;
				Job.Staging.Adopt(MoveTemp(Texture));
			}
		}
	}
}

void ArcTerrainTileAtlasRender::ExecuteFill(FRHICommandListImmediate& RHICmdList,
	const TSharedRef<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe>& Resources,
	const FArcTerrainAtlasFillJob& Job, bool bLogVerbose)
{
	using namespace ArcTerrainTileAtlasPrivate_6b13;

	check(IsInRenderingThread());

	if (!Job.Layout.IsValid())
	{
		return;
	}

	// A layout change invalidates every slice index, so the arrays are rebuilt from scratch. The game
	// thread has already dropped its registrations by the time such a job arrives.
	if (Resources->Layout != Job.Layout)
	{
		Resources->Layout = Job.Layout;
		Resources->HeightAtlas.SafeRelease();
		Resources->WeightAtlases.Reset();
		ArcTerrainTileAtlasHeightBoundsPrivate_4f2a::Initialize(*Resources);
	}
	Resources->WeightAtlases.SetNum(Job.Layout.NumWeightArrays);

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ArcTerrainTileAtlasFill"));

	const int32 HeightSide = Job.Layout.GetHeightSliceSide();
	const int32 WeightSide = Job.Layout.GetWeightSliceSide();

	// The base level is the FAST-CLEAR value, so the whole-array and per-slice clears below take the
	// render-pass clear action instead of a full-screen draw.
	const FLinearColor BaseHeightClear(Job.Layout.BaseHeightCm, 0.0f, 0.0f, 0.0f);

	// Absent weight data means global channel GBaseChannelIndex at 1, unless the set cannot address it.
	const FLinearColor BaseWeightClear = MakeBaseWeightClearColor();
	const int32 BaseWeightArrayIndex = Job.Layout.BaseWeightArrayIndex;

	const bool bCreatedHeightAtlas = !Resources->HeightAtlas.IsValid();
	FRDGTextureRef HeightAtlas = CreateOrRegisterAtlas(GraphBuilder, Resources->HeightAtlas,
		FIntPoint(HeightSide, HeightSide), PF_R32_FLOAT, FClearValueBinding(BaseHeightClear),
		Job.Layout.GetHeightArraySize(), TEXT("ArcTerrain.HeightAtlas"));

	TArray<FRDGTextureRef, TInlineAllocator<4>> WeightAtlases;
	TArray<bool, TInlineAllocator<4>> bCreatedWeightAtlas;
	WeightAtlases.Reserve(Job.Layout.NumWeightArrays);
	for (int32 ArrayIndex = 0; ArrayIndex < Job.Layout.NumWeightArrays; ++ArrayIndex)
	{
		bCreatedWeightAtlas.Add(!Resources->WeightAtlases[ArrayIndex].IsValid());
		const FClearValueBinding WeightClearBinding = (ArrayIndex == BaseWeightArrayIndex)
			? FClearValueBinding(BaseWeightClear)
			: FClearValueBinding::Transparent;
		WeightAtlases.Add(CreateOrRegisterAtlas(GraphBuilder, Resources->WeightAtlases[ArrayIndex],
			FIntPoint(WeightSide, WeightSide), PF_B8G8R8A8, WeightClearBinding,
			Job.Layout.GetWeightArraySize(), TEXT("ArcTerrain.WeightAtlas")));
	}

	// Every write below goes through a RENDER TARGET (a slice binding or a copy), never a typed UAV
	// store, so the typed-UAV-store capability does not gate this path. TextureFilterable is what the
	// tessellation predicate's bilinear sample needs and is a per-adapter runtime result, not a static
	// table — both are logged once because a later compute-side writer would have to branch on them.
	static bool bLoggedAtlasCreation = false;
	if (bCreatedHeightAtlas && !bLoggedAtlasCreation)
	{
		bLoggedAtlasCreation = true;
		UE_LOGF(LogArcTerrain, Log,
			"[TileAtlas] height %d^2 x%d PF_R32_FLOAT, weight %d^2 x%d x%d PF_B8G8R8A8; "
			"PF_R32_FLOAT capabilities 0x%08X (TextureFilterable = %ls); "
			"RHIIsTypedUAVStoreSupported(PF_R32_FLOAT) = %ls (unused — slice writes are render "
			"targets here and a copy pass from the composite).",
			HeightSide, Job.Layout.GetHeightArraySize(), WeightSide, Job.Layout.GetWeightArraySize(),
			Job.Layout.NumWeightArrays,
			static_cast<uint32>(GPixelFormats[PF_R32_FLOAT].Capabilities),
			EnumHasAllFlags(GPixelFormats[PF_R32_FLOAT].Capabilities,
				EPixelFormatCapabilities::TextureFilterable) ? TEXT("true") : TEXT("false"),
			RHIIsTypedUAVStoreSupported(PF_R32_FLOAT) ? TEXT("true") : TEXT("false"));
	}

	// A brand-new array is cleared WHOLE: RDG requires every slice to be written before it is read. The
	// clear value is the AUTHORED base level, which is what an unstamped cell wants.
	if (bCreatedHeightAtlas)
	{
		FRDGTextureClearInfo ClearInfo;
		ClearInfo.ClearColor = BaseHeightClear;
		ClearInfo.FirstSliceIndex = 0;
		ClearInfo.NumSlices = static_cast<uint32>(Job.Layout.GetHeightArraySize());
		AddClearRenderTargetPass(GraphBuilder, HeightAtlas, ClearInfo);
	}
	else
	{
		for (int32 SliceIndex : Job.HeightSlicesToClear)
		{
			if (SliceIndex < 0 || SliceIndex >= Job.Layout.GetHeightAllocatableSlices())
			{
				continue;
			}
			FRDGTextureClearInfo ClearInfo;
			ClearInfo.ClearColor = BaseHeightClear;
			ClearInfo.FirstSliceIndex = static_cast<uint32>(SliceIndex);
			ClearInfo.NumSlices = 1;
			AddClearRenderTargetPass(GraphBuilder, HeightAtlas, ClearInfo);
			ResetHeightSliceBounds(*Resources, SliceIndex);
		}
	}

	for (int32 ArrayIndex = 0; ArrayIndex < WeightAtlases.Num(); ++ArrayIndex)
	{
		// Absent weight data must be zero on every channel but the base.
		const FLinearColor WeightClear =
			(ArrayIndex == BaseWeightArrayIndex) ? BaseWeightClear : FLinearColor::Transparent;
		if (bCreatedWeightAtlas[ArrayIndex])
		{
			FRDGTextureClearInfo ClearInfo;
			ClearInfo.ClearColor = WeightClear;
			ClearInfo.FirstSliceIndex = 0;
			ClearInfo.NumSlices = static_cast<uint32>(Job.Layout.GetWeightArraySize());
			AddClearRenderTargetPass(GraphBuilder, WeightAtlases[ArrayIndex], ClearInfo);

			if (ArrayIndex == BaseWeightArrayIndex)
			{
				FRDGTextureClearInfo FillClearInfo;
				FillClearInfo.ClearColor = BaseWeightClear;
				FillClearInfo.FirstSliceIndex = static_cast<uint32>(Job.Layout.GetWeightFillSlice());
				FillClearInfo.NumSlices = 1;
				AddClearRenderTargetPass(GraphBuilder, WeightAtlases[ArrayIndex], FillClearInfo);
			}
			continue;
		}
		for (int32 SliceIndex : Job.WeightSlicesToClear)
		{
			if (SliceIndex < 0 || SliceIndex >= Job.Layout.GetWeightAllocatableSlices())
			{
				continue;
			}
			FRDGTextureClearInfo ClearInfo;
			ClearInfo.ClearColor = WeightClear;
			ClearInfo.FirstSliceIndex = static_cast<uint32>(SliceIndex);
			ClearInfo.NumSlices = 1;
			AddClearRenderTargetPass(GraphBuilder, WeightAtlases[ArrayIndex], ClearInfo);
		}
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIShaderPlatform);

	for (const FArcTerrainAtlasCellFill& Cell : Job.Cells)
	{
		// ---- Height.
		if (Cell.HeightSlice >= 0 && Cell.HeightSlice < Job.Layout.GetHeightAllocatableSlices()
			&& Cell.HeightTileRes > 0
			&& Cell.HeightDestOrigin.X >= 0 && Cell.HeightDestOrigin.Y >= 0
			&& Cell.HeightDestOrigin.X + Cell.HeightTileRes <= HeightSide
			&& Cell.HeightDestOrigin.Y + Cell.HeightTileRes <= HeightSide)
		{
			const FIntPoint Required(Cell.HeightTileRes, Cell.HeightTileRes);
			FRDGTextureRef BakedHeight = RegisterSourceTexture(GraphBuilder, Cell.BakedHeightRHI,
				PF_R32_FLOAT, Required, TEXT("ArcTerrain.AtlasSrcBakedHeight"));
			if (BakedHeight != nullptr)
			{
				CopySubRectIntoSlice(GraphBuilder, BakedHeight, HeightAtlas, 0, Cell.HeightSlice,
					FIntPoint::ZeroValue, Cell.HeightDestOrigin, Cell.HeightTileRes);
				if (Cell.HeightWrite == FArcTerrainAtlasCellFill::EHeightWrite::Tier0Cell)
				{
					ReplaceTier0HeightCellBounds(*Resources, Cell.HeightSlice, Cell.Key, Cell.HeightRange);
				}
				else if (Cell.HeightWrite == FArcTerrainAtlasCellFill::EHeightWrite::CoarseWholeSlice)
				{
					ReplaceCoarseHeightSliceBounds(*Resources, Cell.HeightSlice, Cell.HeightRange);
				}
			}
		}

		// ---- Weights.
		if (Cell.WeightSlice < 0 || Cell.WeightSlice >= Job.Layout.GetWeightAllocatableSlices()
			|| Cell.WeightTileRes <= 0
			|| Cell.WeightDestOrigin.X < 0 || Cell.WeightDestOrigin.Y < 0
			|| Cell.WeightDestOrigin.X + Cell.WeightTileRes > WeightSide
			|| Cell.WeightDestOrigin.Y + Cell.WeightTileRes > WeightSide)
		{
			if (bLogVerbose && Cell.WeightSlice != INDEX_NONE)
			{
				UE_LOGF(LogArcTerrain, Warning,
					"ArcTerrainTileAtlas weight cell (%d,%d): SKIPPED — bad rect (slice=%d, "
					"tileRes=%d, destOrigin=(%d,%d), weightSide=%d)",
					Cell.Key.X, Cell.Key.Y, Cell.WeightSlice, Cell.WeightTileRes, Cell.WeightDestOrigin.X,
					Cell.WeightDestOrigin.Y, WeightSide);
			}
			continue;
		}

		const FIntPoint WeightRequired(Cell.WeightTileRes, Cell.WeightTileRes);
		const FIntRect WeightViewport(Cell.WeightDestOrigin,
			Cell.WeightDestOrigin + FIntPoint(Cell.WeightTileRes, Cell.WeightTileRes));

		// Live tiles already carry the terrain-global (channel/4, channel%4) packing, so the whole RGBA
		// tile copies straight into the matching atlas array — no routing, no shader.
		bool bFilledFromLive = false;
		int32 LiveArraysCopied = 0;
		for (int32 ArrayIndex = 0; ArrayIndex < Cell.LiveWeightResources.Num()
			&& ArrayIndex < WeightAtlases.Num(); ++ArrayIndex)
		{
			FRDGTextureRef LiveWeight = RegisterSourceResource(GraphBuilder,
				Cell.LiveWeightResources[ArrayIndex], PF_B8G8R8A8, WeightRequired,
				TEXT("ArcTerrain.AtlasSrcLiveWeight"));
			if (LiveWeight == nullptr)
			{
				continue;
			}
			CopySubRectIntoSlice(GraphBuilder, LiveWeight, WeightAtlases[ArrayIndex], 0, Cell.WeightSlice,
				FIntPoint::ZeroValue, Cell.WeightDestOrigin, Cell.WeightTileRes);
			bFilledFromLive = true;
			++LiveArraysCopied;
		}

		if (bFilledFromLive)
		{
			if (bLogVerbose)
			{
				UE_LOGF(LogArcTerrain, Warning,"ArcTerrainTileAtlas weight cell (%d,%d) slice=%d: LIVE path, %d array(s) copied",
					Cell.Key.X, Cell.Key.Y, Cell.WeightSlice, LiveArraysCopied);
			}
			continue;
		}

		// Baked payload: PF_G8, ONE SLICE PER CHANNEL PRESENT IN THIS CELL, with a per-cell slice ->
		// global-channel map. The atlas is keyed on the GLOBAL channel, so each source slice is routed
		// to component (Channel % 4) of array (Channel / 4).
		FRDGTextureRef BakedWeight = RegisterSourceTexture(GraphBuilder, Cell.BakedWeightRHI, PF_G8,
			WeightRequired, TEXT("ArcTerrain.AtlasSrcBakedWeight"));
		if (BakedWeight == nullptr)
		{
			if (bLogVerbose)
			{
				UE_LOGF(LogArcTerrain, Warning,
					"ArcTerrainTileAtlas weight cell (%d,%d) slice=%d: BAKED path, source texture "
					"unresolved — no slices routed",
					Cell.Key.X, Cell.Key.Y, Cell.WeightSlice);
			}
			continue;
		}

		if (bLogVerbose)
		{
			UE_LOGF(LogArcTerrain, Warning,"ArcTerrainTileAtlas weight cell (%d,%d) slice=%d: BAKED path, %d source slice(s)",
				Cell.Key.X, Cell.Key.Y, Cell.WeightSlice, Cell.BakedWeightSliceChannels.Num());
		}

		TShaderMapRef<FArcTerrainAtlasRouteWeightPS> RouteShader(ShaderMap);
		for (int32 SourceSlice = 0; SourceSlice < Cell.BakedWeightSliceChannels.Num(); ++SourceSlice)
		{
			const int32 ChannelIndex = Cell.BakedWeightSliceChannels[SourceSlice];
			const int32 ArrayIndex = ChannelIndex / 4;
			if (ChannelIndex < 0 || ArrayIndex >= WeightAtlases.Num()
				|| SourceSlice >= static_cast<int32>(BakedWeight->Desc.ArraySize))
			{
				if (bLogVerbose)
				{
					UE_LOGF(LogArcTerrain, Warning,
						"ArcTerrainTileAtlas weight cell (%d,%d): DROPPED source slice %d (channel=%d) "
						"— channelNegative=%d, arrayIndex %d>=NumArrays %d is %d, sourceSlice %d>="
						"BakedArraySize %d is %d",
						Cell.Key.X, Cell.Key.Y, SourceSlice, ChannelIndex, ChannelIndex < 0, ArrayIndex,
						WeightAtlases.Num(), ArrayIndex >= WeightAtlases.Num(), SourceSlice,
						static_cast<int32>(BakedWeight->Desc.ArraySize),
						SourceSlice >= static_cast<int32>(BakedWeight->Desc.ArraySize));
				}
				continue;
			}

			const int32 ComponentIndex = ChannelIndex % 4;
			if (bLogVerbose)
			{
				UE_LOGF(LogArcTerrain, Warning,
					"ArcTerrainTileAtlas weight cell (%d,%d): routed source slice %d channel=%d -> "
					"array=%d component=%d destSlice=%d",
					Cell.Key.X, Cell.Key.Y, SourceSlice, ChannelIndex, ArrayIndex, ComponentIndex,
					Cell.WeightSlice);
			}

			FArcTerrainAtlasRouteWeightPS::FParameters* Parameters =
				GraphBuilder.AllocParameters<FArcTerrainAtlasRouteWeightPS::FParameters>();
			Parameters->InDestOrigin = Cell.WeightDestOrigin;
			Parameters->InSourceSlice = static_cast<uint32>(SourceSlice);
			Parameters->InSourceWeight = BakedWeight;
			Parameters->RenderTargets[0] = FRenderTargetBinding(WeightAtlases[ArrayIndex],
				ERenderTargetLoadAction::ELoad, 0, static_cast<int16>(Cell.WeightSlice));

			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap,
				RDG_EVENT_NAME("ArcTerrainAtlasWeight (baked)"), RouteShader, Parameters, WeightViewport,
				GetSingleComponentBlendState(ComponentIndex));
		}
	}
	RecomputeHeightSliceBounds(*Resources);

	// SetTextureAccessFinal requires bExternal (RDG validation), so a texture created this frame must be
	// converted before its access-final is set below. A registered texture is already external and skips
	// the conversion.
	if (bCreatedHeightAtlas)
	{
		Resources->HeightAtlas = GraphBuilder.ConvertToExternalTexture(HeightAtlas);
	}
	for (int32 ArrayIndex = 0; ArrayIndex < WeightAtlases.Num(); ++ArrayIndex)
	{
		if (bCreatedWeightAtlas[ArrayIndex])
		{
			Resources->WeightAtlases[ArrayIndex] = GraphBuilder.ConvertToExternalTexture(WeightAtlases[ArrayIndex]);
		}
	}

	GraphBuilder.SetTextureAccessFinal(HeightAtlas, ERHIAccess::SRVMask);
	for (FRDGTextureRef WeightAtlas : WeightAtlases)
	{
		GraphBuilder.SetTextureAccessFinal(WeightAtlas, ERHIAccess::SRVMask);
	}

	GraphBuilder.Execute();
}

FArcTerrainTileAtlas::FArcTerrainTileAtlas() = default;

FArcTerrainTileAtlas::~FArcTerrainTileAtlas()
{
	ReleaseAll();
}

bool FArcTerrainTileAtlas::SetLayout(const FArcTerrainTileAtlasLayout& NewLayout)
{
	if (Layout == NewLayout)
	{
		return false;
	}

	ReleaseAll();
	Layout = NewLayout;
	return true;
}

uint64 FArcTerrainTileAtlas::BumpHeightSliceGeneration(int32 Slice)
{
	if (Slice < 0)
	{
		return 0;
	}
	if (!HeightSliceGenerations.IsValidIndex(Slice))
	{
		HeightSliceGenerations.SetNumZeroed(Slice + 1);
	}
	return ++HeightSliceGenerations[Slice];
}

uint64 FArcTerrainTileAtlas::BumpWeightSliceGeneration(int32 Slice)
{
	if (Slice < 0)
	{
		return 0;
	}
	if (!WeightSliceGenerations.IsValidIndex(Slice))
	{
		WeightSliceGenerations.SetNumZeroed(Slice + 1);
	}
	return ++WeightSliceGenerations[Slice];
}

uint64 FArcTerrainTileAtlas::GetHeightSliceGeneration(FArcTerrainSectorId SectorId) const
{
	const FSectorRegistration* Registration = Sectors.Find(SectorId);
	return Registration != nullptr ? Registration->SliceGeneration : 0;
}

uint64 FArcTerrainTileAtlas::GetWeightSliceGeneration(FArcTerrainWeightGroupId GroupId) const
{
	const FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId);
	return Registration != nullptr ? Registration->SliceGeneration : 0;
}

int32 FArcTerrainTileAtlas::FindHeightSlice(FArcTerrainSectorId SectorId) const
{
	const FSectorRegistration* Registration = Sectors.Find(SectorId);
	return Registration != nullptr ? Registration->HeightSlice : INDEX_NONE;
}

bool FArcTerrainTileAtlas::TryGetReadyHeightSlice(FArcTerrainSectorId SectorId, int32& OutSlice) const
{
	const FSectorRegistration* Registration = Sectors.Find(SectorId);
	if (Registration == nullptr)
	{
		OutSlice = INDEX_NONE;
		return false;
	}
	OutSlice = Registration->HeightSlice;
	return OutSlice >= 0 && Registration->bRingReady;
}

uint32 FArcTerrainTileAtlas::GetSectorHeightTileFingerprint(FArcTerrainSectorId SectorId) const
{
	const FSectorRegistration* Registration = Sectors.Find(SectorId);
	return Registration != nullptr ? Registration->HeightTileFingerprint : 0;
}

void FArcTerrainTileAtlas::SetSectorHeightTileFingerprint(FArcTerrainSectorId SectorId, uint32 Fingerprint)
{
	if (FSectorRegistration* Registration = Sectors.Find(SectorId))
	{
		Registration->HeightTileFingerprint = Fingerprint;
	}
}

bool FArcTerrainTileAtlas::MarkSectorRingReady(FArcTerrainSectorId SectorId)
{
	FSectorRegistration* Registration = Sectors.Find(SectorId);
	if (Registration == nullptr || Registration->bRingReady)
	{
		return false;
	}
	Registration->bRingReady = true;
	return true;
}

bool FArcTerrainTileAtlas::IsSectorRingReady(FArcTerrainSectorId SectorId) const
{
	const FSectorRegistration* Registration = Sectors.Find(SectorId);
	return Registration != nullptr && Registration->bRingReady;
}

bool FArcTerrainTileAtlas::IsWeightGroupRingReady(FArcTerrainWeightGroupId GroupId) const
{
	const FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId);
	return Registration != nullptr && Registration->bRingReady;
}

bool FArcTerrainTileAtlas::IsSectorAwaitingTile(FArcTerrainSectorId SectorId) const
{
	const FSectorRegistration* Registration = Sectors.Find(SectorId);
	return Registration != nullptr && Registration->bAwaitingTile;
}

bool FArcTerrainTileAtlas::IsWeightGroupAwaitingTile(FArcTerrainWeightGroupId GroupId) const
{
	const FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId);
	return Registration != nullptr && Registration->bAwaitingTile;
}

void FArcTerrainTileAtlas::SetSectorAwaitingTile(FArcTerrainSectorId SectorId, bool bAwaiting)
{
	if (FSectorRegistration* Registration = Sectors.Find(SectorId))
	{
		Registration->bAwaitingTile = bAwaiting;
	}
}

void FArcTerrainTileAtlas::SetWeightGroupAwaitingTile(FArcTerrainWeightGroupId GroupId, bool bAwaiting)
{
	if (FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId))
	{
		Registration->bAwaitingTile = bAwaiting;
	}
}

uint32 FArcTerrainTileAtlas::GetWeightGroupTileFingerprint(FArcTerrainWeightGroupId GroupId) const
{
	const FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId);
	return Registration != nullptr ? Registration->WeightTileFingerprint : 0;
}

void FArcTerrainTileAtlas::SetWeightGroupTileFingerprint(FArcTerrainWeightGroupId GroupId, uint32 Fingerprint)
{
	if (FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId))
	{
		Registration->WeightTileFingerprint = Fingerprint;
	}
}

bool FArcTerrainTileAtlas::MarkWeightGroupRingReady(FArcTerrainWeightGroupId GroupId)
{
	FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId);
	if (Registration == nullptr || Registration->bRingReady)
	{
		return false;
	}
	Registration->bRingReady = true;
	return true;
}

int32 FArcTerrainTileAtlas::FindWeightSlice(FArcTerrainWeightGroupId GroupId) const
{
	const FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId);
	return Registration != nullptr ? Registration->Slice : INDEX_NONE;
}

void FArcTerrainTileAtlas::BuildResidencyRings(const FArcTerrainTierLadder& Ladder,
	const FArcTerrainWeightLadder& WeightLadder, FArcTerrainResidencyRings& OutRings) const
{
	using namespace ArcTerrainTileAtlasPrivate_6b13;

	const int32 NumTiers = FMath::Clamp(Ladder.Tiers.Num(), 1, ArcTerrain::MaxTiers);
	const int32 NumWeightTiers = FMath::Clamp(WeightLadder.Tiers.Num(), 1, ArcTerrain::MaxTiers);
	const uint32 HeightFillSlice = static_cast<uint32>(Layout.GetHeightFillSlice());
	const uint32 WeightFillSlice = static_cast<uint32>(Layout.GetWeightFillSlice());

	OutRings.HeightTiers.Reset();
	OutRings.HeightTiers.SetNum(NumTiers);
	OutRings.Weight.Reset();
	OutRings.Weight.SetNum(NumWeightTiers);

	TArray<TPair<FIntPoint, int32>> EntriesByTier[ArcTerrain::MaxTiers];

	// The nearest admitted sector OF THAT TIER stands in for the camera: UpdateSectorPriority already ranks
	// by squared distance to it, and the atlas is never told the camera position directly.
	FIntPoint AnchorByTier[ArcTerrain::MaxTiers] = {};
	double AnchorDistSqByTier[ArcTerrain::MaxTiers] = {};
	bool bFoundAnchorByTier[ArcTerrain::MaxTiers] = {};

	for (int32 Tier = 0; Tier < NumTiers; ++Tier)
	{
		EntriesByTier[Tier].Reserve(Sectors.Num());
	}

	for (const TPair<FArcTerrainSectorId, FSectorRegistration>& Pair : Sectors)
	{
		const int32 Tier = Pair.Key.Tier;
		int32 ReadyHeightSlice = INDEX_NONE;
		const bool bReady = TryGetReadyHeightSlice(Pair.Key, ReadyHeightSlice);
		if (Pair.Value.HeightSlice < 0 || Tier < 0 || Tier >= NumTiers)
		{
			continue;
		}
		const int32 PublishedHeightSlice = bReady
			? Pair.Value.HeightSlice
			: static_cast<int32>(HeightFillSlice);
		EntriesByTier[Tier].Emplace(Pair.Key.Key, PublishedHeightSlice);
		// Key tie-break: DistSq is a min over quantised invoker positions, so equal distances are
		// reachable, and letting TMap iteration order pick the anchor moves the whole window.
		const bool bBetterAnchor = !bFoundAnchorByTier[Tier]
			|| Pair.Value.DistSq < AnchorDistSqByTier[Tier]
			|| (Pair.Value.DistSq == AnchorDistSqByTier[Tier]
				&& (Pair.Key.Key.X < AnchorByTier[Tier].X
					|| (Pair.Key.Key.X == AnchorByTier[Tier].X && Pair.Key.Key.Y < AnchorByTier[Tier].Y)));
		if (bBetterAnchor)
		{
			bFoundAnchorByTier[Tier] = true;
			AnchorDistSqByTier[Tier] = Pair.Value.DistSq;
			AnchorByTier[Tier] = Pair.Key.Key;
		}
	}

	for (int32 Tier = 0; Tier < NumTiers; ++Tier)
	{
		FArcTerrainResidencyRing& Ring = OutRings.HeightTiers[Tier];
		Ring.FillSlice = HeightFillSlice;
		Ring.CumulativeShift = Ladder.Tiers.IsValidIndex(Tier) ? Ladder.Tiers[Tier].CumulativeShift : 0;
		BuildResidencyRing(EntriesByTier[Tier], AnchorByTier[Tier], HeightRingName(Tier),
			bWarnedHeightRingClamped[Tier], Ring);
	}

	TArray<TPair<FIntPoint, int32>> WeightEntriesByTier[ArcTerrain::MaxTiers];

	// Each weight tier has its OWN centre — the nearest admitted group OF THAT TIER; the sets stream on
	// separate radii and need not overlap, and they do not overlap the height ones either.
	FIntPoint AnchorGroupByTier[ArcTerrain::MaxTiers] = {};
	double AnchorGroupDistSqByTier[ArcTerrain::MaxTiers] = {};
	bool bFoundAnchorGroupByTier[ArcTerrain::MaxTiers] = {};

	for (int32 Tier = 0; Tier < NumWeightTiers; ++Tier)
	{
		WeightEntriesByTier[Tier].Reserve(WeightSliceByGroup.Num());
	}

	for (const TPair<FArcTerrainWeightGroupId, FWeightGroupRegistration>& Pair : WeightSliceByGroup)
	{
		const int32 Tier = Pair.Key.Tier;
		if (Pair.Value.Slice < 0 || Tier < 0 || Tier >= NumWeightTiers)
		{
			continue;
		}
		const int32 PublishedWeightSlice = Pair.Value.bRingReady
			? Pair.Value.Slice
			: static_cast<int32>(WeightFillSlice);
		WeightEntriesByTier[Tier].Emplace(Pair.Key.Key, PublishedWeightSlice);
		const bool bBetterAnchor = !bFoundAnchorGroupByTier[Tier]
			|| Pair.Value.DistSq < AnchorGroupDistSqByTier[Tier]
			|| (Pair.Value.DistSq == AnchorGroupDistSqByTier[Tier]
				&& (Pair.Key.Key.X < AnchorGroupByTier[Tier].X
					|| (Pair.Key.Key.X == AnchorGroupByTier[Tier].X
						&& Pair.Key.Key.Y < AnchorGroupByTier[Tier].Y)));
		if (bBetterAnchor)
		{
			bFoundAnchorGroupByTier[Tier] = true;
			AnchorGroupDistSqByTier[Tier] = Pair.Value.DistSq;
			AnchorGroupByTier[Tier] = Pair.Key.Key;
		}
	}

	for (int32 Tier = 0; Tier < NumWeightTiers; ++Tier)
	{
		FArcTerrainResidencyRing& Ring = OutRings.Weight[Tier];
		Ring.FillSlice = WeightFillSlice;
		Ring.CumulativeShift = WeightLadder.Tiers.IsValidIndex(Tier)
			? WeightLadder.Tiers[Tier].CumulativeShift
			: 0;
		BuildResidencyRing(WeightEntriesByTier[Tier], AnchorGroupByTier[Tier], WeightRingName(Tier),
			bWarnedWeightRingClamped[Tier], Ring);
	}
}

void ArcTerrainAtlas::PackWeightRings(TConstArrayView<FArcTerrainResidencyRing> Rings,
	TArray<uint32>& OutSlices, FArcTerrainWeightRingScalars& OutScalars)
{
	OutSlices.Reset();
	OutScalars = FArcTerrainWeightRingScalars();

	const int32 NumTiers = FMath::Min(Rings.Num(), ArcTerrain::MaxTiers);
	for (int32 Tier = 0; Tier < NumTiers; ++Tier)
	{
		const FArcTerrainResidencyRing& Ring = Rings[Tier];
		OutScalars.Offset[Tier] = static_cast<uint32>(OutSlices.Num());
		OutScalars.Radius[Tier] = Ring.Slices.IsEmpty() ? -1 : Ring.Radius;
		OutScalars.CenterX[Tier] = Ring.Center.X;
		OutScalars.CenterY[Tier] = Ring.Center.Y;
		OutScalars.CumulativeShift[Tier] = Ring.CumulativeShift;
		OutSlices.Append(Ring.Slices);
	}
	OutScalars.NumTiers = static_cast<uint32>(NumTiers);
	OutScalars.FillSlice = NumTiers > 0 ? Rings[0].FillSlice : 0u;
}

void ArcTerrainAtlas::PackHeightRings(TConstArrayView<FArcTerrainResidencyRing> Rings,
	TArray<uint32>& OutSlices, FArcTerrainHeightRingScalars& OutScalars)
{
	OutSlices.Reset();
	OutScalars = FArcTerrainHeightRingScalars();

	const int32 NumTiers = FMath::Min(Rings.Num(), ArcTerrain::MaxTiers);
	for (int32 Tier = 0; Tier < NumTiers; ++Tier)
	{
		const FArcTerrainResidencyRing& Ring = Rings[Tier];
		OutScalars.Offset[Tier] = static_cast<uint32>(OutSlices.Num());
		OutScalars.Radius[Tier] = Ring.Slices.IsEmpty() ? -1 : Ring.Radius;
		OutScalars.CenterX[Tier] = Ring.Center.X;
		OutScalars.CenterY[Tier] = Ring.Center.Y;
		OutScalars.CumulativeShift[Tier] = Ring.CumulativeShift;
		OutSlices.Append(Ring.Slices);
	}
	OutScalars.NumTiers = static_cast<uint32>(NumTiers);
	OutScalars.FillSlice = NumTiers > 0 ? Rings[0].FillSlice : 0u;
}

void FArcTerrainTileAtlas::UpdateSectorPriority(FArcTerrainSectorId SectorId, double DistSq, bool bPinned)
{
	if (FSectorRegistration* Registration = Sectors.Find(SectorId))
	{
		Registration->DistSq = DistSq;
		Registration->bPinned = bPinned;
	}
}

void FArcTerrainTileAtlas::UpdateWeightGroupPriority(FArcTerrainWeightGroupId GroupId, double DistSq,
	bool bPinned)
{
	if (FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId))
	{
		Registration->DistSq = DistSq;
		Registration->bPinned = bPinned;
	}
}

bool FArcTerrainTileAtlas::FindWeightEvictionVictim(int32 CandidateTier, double CandidateDistSq,
	FArcTerrainWeightGroupId& OutVictim) const
{
	bool bFound = false;
	double WorstDistSq = -1.0;
	for (const TPair<FArcTerrainWeightGroupId, FWeightGroupRegistration>& Pair : WeightSliceByGroup)
	{
		if (Pair.Value.bPinned || Pair.Value.bRingReady)
		{
			continue;
		}
		// Refuse rather than swap: a victim no farther than the candidate is the backwards LRU that
		// re-clears and refills a slice every frame.
		if (Pair.Value.DistSq <= CandidateDistSq)
		{
			continue;
		}
		// Fine data never yields to coarse. A coarse group CONTAINING the camera has DistSq 0, so the
		// distance test above cannot express this and it has to be its own clause.
		if (Pair.Key.Tier < CandidateTier)
		{
			continue;
		}
		// Tie-break so a same-distance pair evicts deterministically, COARSEST first.
		const bool bBetter = !bFound || Pair.Value.DistSq > WorstDistSq
			|| (Pair.Value.DistSq == WorstDistSq
				&& (Pair.Key.Tier > OutVictim.Tier
					|| (Pair.Key.Tier == OutVictim.Tier
						&& (Pair.Key.Key.X < OutVictim.Key.X
							|| (Pair.Key.Key.X == OutVictim.Key.X && Pair.Key.Key.Y < OutVictim.Key.Y)))));
		if (bBetter)
		{
			bFound = true;
			WorstDistSq = Pair.Value.DistSq;
			OutVictim = Pair.Key;
		}
	}
	return bFound;
}

FArcTerrainTileAtlas::FWeightAdmissionResult FArcTerrainTileAtlas::AdmitWeightGroupTracked(
	FArcTerrainWeightGroupId Id, double DistSq, bool bPinned)
{
	FWeightAdmissionResult Result;
	if (!Layout.IsValid())
	{
		return Result;
	}

	if (const FWeightGroupRegistration* Existing = WeightSliceByGroup.Find(Id))
	{
		UpdateWeightGroupPriority(Id, DistSq, bPinned);
		Result.bAdmitted = true;
		Result.AssignedSlice = Existing->Slice;
		Result.SliceGeneration = Existing->SliceGeneration;
		return Result;
	}

	const int32 WeightAllocatable = Layout.GetWeightAllocatableSlices();

	while (WeightSliceAllocator.GetSparselyAllocatedSize() + 1 > WeightAllocatable)
	{
		FArcTerrainWeightGroupId Victim;
		if (!FindWeightEvictionVictim(Id.Tier, DistSq, Victim))
		{
			if (!bWarnedWeightGroupRefused)
			{
				bWarnedWeightGroupRefused = true;
				UE_LOGF(LogArcTerrain, Warning,
					"[TileAtlas] Weight group (tier %d, %d,%d) refused: none of the %d weight slots holds "
					"a registration that is unpinned, FARTHER away and not of a FINER tier. An admission "
					"never displaces something nearer, and fine data never yields to coarse. Those cells "
					"shade with the base layer; raise arc.Terrain.MaxResidentWeightCells.",
					Id.Tier, Id.Key.X, Id.Key.Y, Layout.MaxResidentWeightCells);
			}
			return Result;
		}
		const FWeightGroupRegistration* VictimRegistration = WeightSliceByGroup.Find(Victim);
		const int32 VictimSlice = VictimRegistration != nullptr ? VictimRegistration->Slice : INDEX_NONE;
		Result.EvictedId = Victim;
		EvictWeightGroup(Victim);
		// Numeric slice is not identity across implicit eviction.
		Result.EvictedSliceGeneration = WeightSliceGenerations.IsValidIndex(VictimSlice)
			? WeightSliceGenerations[VictimSlice] : 0;
	}

	FWeightGroupRegistration Registration;
	Registration.DistSq = DistSq;
	Registration.bPinned = bPinned;
	Registration.bRingReady = false;
	Registration.Slice = WeightSliceAllocator.Allocate(1);
	if (Registration.Slice < 0 || Registration.Slice >= WeightAllocatable)
	{
		if (Registration.Slice >= 0)
		{
			WeightSliceAllocator.Free(Registration.Slice, 1);
		}
		return Result;
	}

	Registration.SliceGeneration = BumpWeightSliceGeneration(Registration.Slice);
	Result.bAdmitted = true;
	Result.AssignedSlice = Registration.Slice;
	Result.SliceGeneration = Registration.SliceGeneration;
	WeightSliceByGroup.Add(Id, Registration);
	return Result;
}

bool FArcTerrainTileAtlas::AdmitWeightGroup(FArcTerrainWeightGroupId GroupId, double DistSq, bool bPinned)
{
	return AdmitWeightGroupTracked(GroupId, DistSq, bPinned).bAdmitted;
}

void FArcTerrainTileAtlas::EvictWeightGroup(FArcTerrainWeightGroupId GroupId)
{
	FWeightGroupRegistration Registration;
	if (!WeightSliceByGroup.RemoveAndCopyValue(GroupId, Registration))
	{
		return;
	}
	ArcTerrainDiag::NoteSlicesEvicted(/*bWeight=*/true, 1);
	if (Registration.Slice != INDEX_NONE)
	{
		BumpWeightSliceGeneration(Registration.Slice);
		WeightSliceAllocator.Free(Registration.Slice, 1);
	}
}

void FArcTerrainTileAtlas::GetAdmittedWeightGroups(TArray<FArcTerrainWeightGroupId>& OutGroups) const
{
	OutGroups.Reset(WeightSliceByGroup.Num());
	for (const TPair<FArcTerrainWeightGroupId, FWeightGroupRegistration>& Pair : WeightSliceByGroup)
	{
		OutGroups.Add(Pair.Key);
	}
}

FString FArcTerrainTileAtlas::DescribeWeightAdmission(FArcTerrainWeightGroupId GroupId,
	double CandidateDistSq) const
{
	if (!Layout.IsValid())
	{
		return FString::Printf(
			TEXT("REFUSED: layout invalid (heightquads %d, weightquads %d, maxresidentsectors %d, ")
			TEXT("maxresidentweightcells %d)"),
			Layout.HeightQuads, Layout.WeightQuads, Layout.MaxResidentSectors, Layout.MaxResidentWeightCells);
	}

	const int32 Allocatable = Layout.GetWeightAllocatableSlices();
	if (const FWeightGroupRegistration* Registration = WeightSliceByGroup.Find(GroupId))
	{
		return FString::Printf(TEXT("GRANTED: slice %d of %d allocatable (pinned=%s)"),
			Registration->Slice, Allocatable, Registration->bPinned ? TEXT("yes") : TEXT("no"));
	}

	int32 NumPinned = 0;
	for (const TPair<FArcTerrainWeightGroupId, FWeightGroupRegistration>& Pair : WeightSliceByGroup)
	{
		NumPinned += Pair.Value.bPinned ? 1 : 0;
	}
	const int32 Live = WeightSliceAllocator.GetSparselyAllocatedSize();
	if (Live + 1 > Allocatable)
	{
		FArcTerrainWeightGroupId Victim;
		if (!FindWeightEvictionVictim(GroupId.Tier, CandidateDistSq, Victim))
		{
			return FString::Printf(
				TEXT("REFUSED: all %d weight slots are taken (live %d, admitted %d, pinned %d) and none holds ")
				TEXT("a group that is unpinned, FARTHER than %.1f m and not of a FINER tier than %d"),
				Allocatable, Live, WeightSliceByGroup.Num(), NumPinned,
				FMath::Sqrt(FMath::Max(CandidateDistSq, 0.0)) * 0.01, GroupId.Tier);
		}
		return FString::Printf(
			TEXT("EVICTS: slot freed by group (tier %d, %d,%d) (live %d of %d allocatable, pinned %d)"),
			Victim.Tier, Victim.Key.X, Victim.Key.Y, Live, Allocatable, NumPinned);
	}
	return FString::Printf(
		TEXT("NOT ADMITTED: no slice held (live %d, allocatable %d, pinned %d, admitted groups %d)"),
		Live, Allocatable, NumPinned, WeightSliceByGroup.Num());
}

bool FArcTerrainTileAtlas::FindEvictionVictim(int32 CandidateTier, double CandidateDistSq,
	FArcTerrainSectorId& OutVictim) const
{
	bool bFound = false;
	double WorstDistSq = -1.0;
	for (const TPair<FArcTerrainSectorId, FSectorRegistration>& Pair : Sectors)
	{
		if (Pair.Value.bPinned || Pair.Value.bRingReady)
		{
			continue;
		}
		// Refuse rather than swap: a victim no farther than the candidate is the backwards LRU that
		// re-clears and refills a 4 MB slice every frame.
		if (Pair.Value.DistSq <= CandidateDistSq)
		{
			continue;
		}
		// Fine data never yields to coarse. A coarse sector CONTAINING the camera has DistSq 0, so the
		// distance test above cannot express this and it has to be its own clause.
		if (Pair.Key.Tier < CandidateTier)
		{
			continue;
		}
		// Tie-break so a same-distance pair evicts deterministically, COARSEST first.
		const bool bBetter = !bFound || Pair.Value.DistSq > WorstDistSq
			|| (Pair.Value.DistSq == WorstDistSq
				&& (Pair.Key.Tier > OutVictim.Tier
					|| (Pair.Key.Tier == OutVictim.Tier
						&& (Pair.Key.Key.X < OutVictim.Key.X
							|| (Pair.Key.Key.X == OutVictim.Key.X && Pair.Key.Key.Y < OutVictim.Key.Y)))));
		if (bBetter)
		{
			bFound = true;
			WorstDistSq = Pair.Value.DistSq;
			OutVictim = Pair.Key;
		}
	}
	return bFound;
}

FArcTerrainTileAtlas::FHeightAdmissionResult FArcTerrainTileAtlas::AdmitSectorTracked(
	FArcTerrainSectorId Id, double DistSq, bool bPinned)
{
	FHeightAdmissionResult Result;
	if (!Layout.IsValid())
	{
		return Result;
	}

	if (const FSectorRegistration* Existing = Sectors.Find(Id))
	{
		UpdateSectorPriority(Id, DistSq, bPinned);
		Result.bAdmitted = true;
		Result.AssignedSlice = Existing->HeightSlice;
		Result.SliceGeneration = Existing->SliceGeneration;
		return Result;
	}

	const int32 HeightArraySize = Layout.GetHeightAllocatableSlices();

	while (HeightSliceAllocator.GetSparselyAllocatedSize() + 1 > HeightArraySize)
	{
		FArcTerrainSectorId Victim;
		if (!FindEvictionVictim(Id.Tier, DistSq, Victim))
		{
			if (!bWarnedSectorRefused)
			{
				bWarnedSectorRefused = true;
				UE_LOGF(LogArcTerrain, Warning,
					"[TileAtlas] Sector (tier %d, %d,%d) refused: none of the %d atlas slots holds a "
					"registration that is unpinned, FARTHER away and not of a FINER tier. An admission "
					"never displaces something nearer, and fine data never yields to coarse. The sector "
					"is not drawn this frame.",
					Id.Tier, Id.Key.X, Id.Key.Y, Layout.MaxResidentSectors);
			}
			return Result;
		}
		const FSectorRegistration* VictimRegistration = Sectors.Find(Victim);
		const int32 VictimSlice = VictimRegistration != nullptr ? VictimRegistration->HeightSlice : INDEX_NONE;
		Result.EvictedId = Victim;
		EvictSector(Victim);
		// Numeric slice is not identity across implicit eviction.
		Result.EvictedSliceGeneration = HeightSliceGenerations.IsValidIndex(VictimSlice)
			? HeightSliceGenerations[VictimSlice] : 0;
	}

	FSectorRegistration Registration;
	Registration.DistSq = DistSq;
	Registration.bPinned = bPinned;
	Registration.bRingReady = false;
	Registration.HeightSlice = HeightSliceAllocator.Allocate(1);
	if (Registration.HeightSlice < 0 || Registration.HeightSlice >= HeightArraySize)
	{
		if (Registration.HeightSlice >= 0)
		{
			HeightSliceAllocator.Free(Registration.HeightSlice, 1);
		}
		return Result;
	}

	Registration.SliceGeneration = BumpHeightSliceGeneration(Registration.HeightSlice);
	Result.bAdmitted = true;
	Result.AssignedSlice = Registration.HeightSlice;
	Result.SliceGeneration = Registration.SliceGeneration;
	Sectors.Add(Id, MoveTemp(Registration));
	return Result;
}

bool FArcTerrainTileAtlas::AdmitSector(FArcTerrainSectorId SectorId, double DistSq, bool bPinned)
{
	return AdmitSectorTracked(SectorId, DistSq, bPinned).bAdmitted;
}

void FArcTerrainTileAtlas::EvictSector(FArcTerrainSectorId SectorId)
{
	FSectorRegistration Registration;
	if (!Sectors.RemoveAndCopyValue(SectorId, Registration))
	{
		return;
	}
	ArcTerrainDiag::NoteSlicesEvicted(/*bWeight=*/false, 1);

	if (Registration.HeightSlice != INDEX_NONE)
	{
		BumpHeightSliceGeneration(Registration.HeightSlice);
		HeightSliceAllocator.Free(Registration.HeightSlice, 1);
	}
}

void FArcTerrainTileAtlas::GetAdmittedSectors(TArray<FArcTerrainSectorId>& OutSectors) const
{
	OutSectors.Reset(Sectors.Num());
	for (const TPair<FArcTerrainSectorId, FSectorRegistration>& Pair : Sectors)
	{
		OutSectors.Add(Pair.Key);
	}
}

void FArcTerrainTileAtlas::ReleaseAll()
{
	ArcTerrainDiag::NoteSlicesEvicted(/*bWeight=*/false, Sectors.Num());
	ArcTerrainDiag::NoteSlicesEvicted(/*bWeight=*/true, WeightSliceByGroup.Num());

	Sectors.Reset();
	WeightSliceByGroup.Reset();
	HeightSliceAllocator.Empty();
	WeightSliceAllocator.Empty();
	for (uint64& Generation : HeightSliceGenerations)
	{
		++Generation;
	}
	for (uint64& Generation : WeightSliceGenerations)
	{
		++Generation;
	}

	if (Resources.IsValid())
	{
		++ResourceGeneration;
		ENQUEUE_RENDER_COMMAND(ArcTerrainReleaseTileAtlas)(
			[Released = MoveTemp(Resources)](FRHICommandListImmediate&) mutable
			{
				Released->HeightAtlas.SafeRelease();
				Released->WeightAtlases.Reset();
			});
		Resources.Reset();
	}
}

void FArcTerrainTileAtlas::EnqueueFill(FArcTerrainAtlasFillJob&& Job)
{
	if (!Job.Layout.IsValid())
	{
		return;
	}

	// THE ARRAYS COME FROM THE LAYOUT, NOT FROM THE WORK. They carry the fill slices every unresident
	// lookup lands on, so a camera far from any stamp — no admitted sector, empty job — must still get
	// them. Resources on the game thread is the latch: the render thread's own creation is invisible here.
	if (Resources.IsValid() && Job.IsEmpty())
	{
		return;
	}

	if (!Resources.IsValid())
	{
		Resources = MakeShared<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe>();
		++ResourceGeneration;
	}

	const bool bLogVerbose = ArcTerrainDiag::IsWeightCompositeVerbose();
	ENQUEUE_RENDER_COMMAND(ArcTerrainFillTileAtlas)(
		[TargetResources = Resources.ToSharedRef(), FillJob = MoveTemp(Job), bLogVerbose]
		(FRHICommandListImmediate& RHICmdList) mutable
		{
			ArcTerrainTileAtlasRender::ResolveStagingTextures(RHICmdList, FillJob);
			ArcTerrainTileAtlasRender::ExecuteFill(RHICmdList, TargetResources, FillJob, bLogVerbose);
			// Reservation tokens must outlive this command: staging arrays stay live until ExecuteFill.
			FillJob.ReservationTokens.Reset();
		});
}
