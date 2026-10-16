// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainGPU/ArcTerrainDataInterface.h"

#include "ArcPCGExtensionModule.h"
#include "TerrainGPU/ArcTerrainScatterKernel.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainTypes.h"
#include "ArcTerrainTileAccess.h"

#include "PCGContext.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGSettings.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGDataBinding.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "EngineUtils.h"
#include "GlobalRenderResources.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SystemTextures.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTerrainDataInterface)

#define LOCTEXT_NAMESPACE "ArcTerrainDataInterface"

/** File-scoped, uniquely named (CLAUDE.md C++ Code Style). */
namespace ArcPCGExtensionTerrainDataInterfacePrivate
{
	TAutoConsoleVariable<int32> CVarMaxCells(
		TEXT("arc.PCG.TerrainGPU.MaxCells"),
		4,
		TEXT("Maximum ArcTerrain cells one GPU scatter execution may atlas. Hard-clamped to 16 (the ")
		TEXT("residency bitmask and the per-slot origin arrays are fixed at that size)."),
		ECVF_Default);

	/** The whole point of the LWC split (PCGLandscapeDataInterface.cpp:134-141): the matrices handed
	 *  to the shader are relative to LWCTile, so the shader's LWCMultiply never multiplies a world
	 *  position by a matrix carrying a multi-kilometre translation. */
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

		// RotateLocalToWorld's InvScale renormalizes the LocalToWorld basis ROWS (each row is an axis
		// scaled by that axis' scale), so it is the reciprocal MAGNITUDE — not the signed reciprocal.
		const FVector Scale = InTerrainTransform.GetScale3D();
		OutInvNonUniformScale = FVector3f(
			FMath::IsNearlyZero(Scale.X) ? 1.0f : static_cast<float>(1.0 / FMath::Abs(Scale.X)),
			FMath::IsNearlyZero(Scale.Y) ? 1.0f : static_cast<float>(1.0 / FMath::Abs(Scale.Y)),
			FMath::IsNearlyZero(Scale.Z) ? 1.0f : static_cast<float>(1.0 / FMath::Abs(Scale.Z)));
	}

	/** Registers one uploaded tile texture with RDG. The proxy owns a COUNTED reference to the
	 *  FRHITexture, so by the time this runs the texture is guaranteed alive; a null ref just means the
	 *  cell contributed nothing. Rejects a texture that cannot hold the sub-rect this execution planned
	 *  — a cell cut at a different raster would make the copy read out of bounds.
	 *
	 *  THE FORMAT CHECK STAYS. AddCopyTexturePass and CopyTextureRegionChecked compare block alignment
	 *  and bounds, never the format, so a same-footprint copy of the wrong format is one a driver may
	 *  simply perform — reinterpreting the bytes, i.e. a smoothly wrong surface rather than an error.
	 *  A mismatch is a legal hole: the slot stays non-resident and the residency mask gates it out. */
	FRDGTextureRef RegisterTileSource(FRDGBuilder& GraphBuilder, const FArcTerrainDITileTexturePtr& Tile,
		FIntPoint SrcOrigin, FIntPoint SubRect, EPixelFormat ExpectedFormat, const TCHAR* Name)
	{
		if (!Tile.IsValid() || !Tile->TextureRHI.IsValid())
		{
			return nullptr;
		}
		const FTextureRHIRef& TextureRHI = Tile->TextureRHI;

		if (TextureRHI->GetFormat() != ExpectedFormat)
		{
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

	/** ArcTerrain hands out BYTES, never a texture, so the GPU copy source is built here. */
	FArcTerrainDITileTexturePtr UploadTileTexture(TArray<uint8>&& Bytes, int32 Resolution,
		int32 BytesPerTexel, EPixelFormat Format, const TCHAR* DebugName)
	{
		if (Resolution < 4 || Bytes.Num() != Resolution * Resolution * BytesPerTexel)
		{
			return FArcTerrainDITileTexturePtr();
		}

		FArcTerrainDITileTexturePtr Tile = MakeShared<FArcTerrainDITileTexture, ESPMode::ThreadSafe>();

		ENQUEUE_RENDER_COMMAND(ArcPCGUploadTerrainTile)(
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

	/** LoadTileSync, the same trade as FPCGGetTerrainElement::ProcessActors: a PCG generation is a
	 *  one-shot bake, not the render residency queue. Null = no tile covers the cell, or the tile could
	 *  not be read — both leave a legal hole. */
	FArcTerrainTilePayloadPtr ResolveTile(const AArcTerrain& Terrain, EArcTerrainRail Rail,
		FArcTerrainCellKey Key)
	{
		FIntPoint TileKey = FIntPoint::ZeroValue;
		if (!ArcTerrainTileAccess::TileKeyFromCell(Terrain, Rail, 0, Key, TileKey))
		{
			return FArcTerrainTilePayloadPtr();
		}
		return ArcTerrainTileAccess::LoadTileSync(Terrain, Rail, 0, TileKey);
	}

	FPCGContext* GetContext(UPCGDataBinding* InBinding)
	{
		if (InBinding == nullptr)
		{
			return nullptr;
		}

		const TSharedPtr<FPCGContextHandle> Handle = InBinding->GetContextHandle().Pin();
		return Handle ? Handle->GetContext() : nullptr;
	}
}

void UArcTerrainDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("IsArcTerrainResident"))
		.AddReturnType(EShaderFundamentalType::Bool)
		.AddParam(EShaderFundamentalType::Float, 3); // InWorldPos

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SampleArcTerrainHeight"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Float, 3); // InWorldPos

	// [Step 3] SampleArcTerrainHeight answers in TERRAIN-LOCAL cm. A kernel that wrote it straight
	// into a point position would be correct only for a terrain at the world origin with an identity
	// transform, and plausibly wrong everywhere else. This is the local->world half, exposed so the
	// conversion cannot be forgotten (and so it goes through the same LWC matrices).
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SampleArcTerrainSurfaceWorld"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Float, 3); // InWorldPos

	// [Step 3] The reference direction of GetArcScatterSlopeRange's cosine window. World up is NOT a
	// substitute under a rotated terrain transform.
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcTerrainUpWorld"))
		.AddReturnType(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SampleArcTerrainNormal"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Float, 3); // InWorldPos

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SampleArcTerrainLayerWeight"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Float, 3) // InWorldPos
		.AddParam(EShaderFundamentalType::Int); // InGlobalChannelIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterBoundsMin"))
		.AddReturnType(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterBoundsExtent"))
		.AddReturnType(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterPointCount"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterSeed"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterSlopeRange"))
		.AddReturnType(EShaderFundamentalType::Float, 2);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterLayerChannel"))
		.AddReturnType(EShaderFundamentalType::Int);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterMinLayerWeight"))
		.AddReturnType(EShaderFundamentalType::Float);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetArcScatterAlignToNormal"))
		.AddReturnType(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FArcTerrainDataInterfaceParameters,)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>,  HeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState,                   HeightAtlasSampler)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>,  WeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState,                   WeightAtlasSampler)
	SHADER_PARAMETER(uint32,     ResidencyMask)
	SHADER_PARAMETER(FMatrix44f, WorldToTerrainLocal)
	SHADER_PARAMETER(FMatrix44f, TerrainLocalToWorld)
	SHADER_PARAMETER(FVector3f,  TerrainInvNonUniformScale)
	SHADER_PARAMETER(FVector3f,  LWCTile)
	SHADER_PARAMETER(FIntPoint,  AtlasOriginCell)
	SHADER_PARAMETER(FIntPoint,  AtlasCellCount)
	SHADER_PARAMETER(FVector2f,  HeightSubRectInv)
	SHADER_PARAMETER(FVector2f,  WeightSubRectInv)
	// FVector4f, not FVector2f: SHADER_PARAMETER_ARRAY's element type carries
	// static_assert((sizeof(T) % 16) == 0) (ShaderParameterMacros.h:1178), so an FVector2f array does
	// not merely mis-pack - it does not compile. Only .xy is meaningful.
	SHADER_PARAMETER_ARRAY(FVector4f, HeightSrcOriginPerSlot, [GArcTerrainDIMaxSlots])
	SHADER_PARAMETER_ARRAY(FVector4f, WeightSrcOriginPerSlot, [GArcTerrainDIMaxSlots])
	SHADER_PARAMETER(float,  CellSize)
	SHADER_PARAMETER(float,  HeightTileRes)
	SHADER_PARAMETER(float,  WeightTileRes)
	SHADER_PARAMETER(float,  TexelToLocal)
	SHADER_PARAMETER(FVector3f, ScatterBoundsMin)
	SHADER_PARAMETER(FVector3f, ScatterBoundsExtent)
	SHADER_PARAMETER(uint32,    ScatterPointCount)
	SHADER_PARAMETER(uint32,    ScatterSeed)
	SHADER_PARAMETER(FVector2f, ScatterSlopeRange)
	SHADER_PARAMETER(int32,     ScatterLayerChannel)
	SHADER_PARAMETER(float,     ScatterMinLayerWeight)
	SHADER_PARAMETER(uint32,    ScatterAlignToNormal)
END_SHADER_PARAMETER_STRUCT()

void UArcTerrainDataInterface::GetShaderParameters(TCHAR const* UID,
	FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FArcTerrainDataInterfaceParameters>(UID);
}

TCHAR const* UArcTerrainDataInterface::TemplateFilePath =
	TEXT("/Plugin/ArcPCGExtension/Private/ArcTerrainDataInterface.ush");

TCHAR const* UArcTerrainDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UArcTerrainDataInterface::GetShaderHash(FString& InOutKey) const
{
	// Without this, editing the .ush does NOT retrigger a shader compile.
	GetShaderFileHashPlatformInvariant(TemplateFilePath).AppendString(InOutKey);
	GetShaderFileHashPlatformInvariant(TEXT("/Engine/Private/LargeWorldCoordinates.ush")).AppendString(InOutKey);
}

void UArcTerrainDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
		{ TEXT("MaxSlots"), FString::FromInt(GArcTerrainDIMaxSlots) },
	};

	FString TemplateFile;
	if (ensure(LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr)))
	{
		OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
	}
}

UComputeDataProvider* UArcTerrainDataInterface::CreateDataProvider() const
{
	return NewObject<UArcTerrainDataProvider>();
}

void UArcTerrainDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding,
	uint64 InInputMask, uint64 InOutputMask)
{
	using namespace ArcPCGExtensionTerrainDataInterfacePrivate;
	using namespace ArcPCGTerrainGPU;

	TRACE_CPUPROFILER_EVENT_SCOPE(UArcTerrainDataProvider::Initialize);
	check(IsInGameThread());

	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	// A re-Initialize without a Reset must not carry the previous generation's section holds (or,
	// worse, stale FTextureResource captures) forward.
	Data = FArcTerrainDataProxyData();

	const UArcTerrainDataInterface* DataInterface = CastChecked<UArcTerrainDataInterface>(InDataInterface);
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	FPCGContext* Context = GetContext(Binding);

	// NOT GetProducerSettings() alone: nothing ever sets it on an additional data interface, so every
	// message below would degrade to log-only. See the NODE-VISIBLE DIAGNOSTICS block in the header.
	const UPCGSettings* Settings = ResolveNodeSettings();

	const FArcTerrainScatterGpuParams& ScatterParams = DataInterface->GetScatterParams();
	Data.ScatterSeed = static_cast<uint32>(ScatterParams.Seed);
	Data.ScatterMinLayerWeight = ScatterParams.MinLayerWeight;
	Data.ScatterAlignToNormal = ScatterParams.bAlignToNormal ? 1u : 0u;
	// cos is DECREASING in the angle, so the acceptance interval on cos(slope) is
	// [cos(MaxSlope), cos(MinSlope)] — X is the lower bound.
	Data.ScatterSlopeRange = FVector2f(
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ScatterParams.MaxSlopeDegrees, 0.0f, 90.0f))),
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ScatterParams.MinSlopeDegrees, 0.0f, 90.0f))));

	// NOT from a pin: additional DIs never get AddDownstreamInputPin, so
	// GetDownstreamInputPinLabelAliases() is empty (PCGLandscapeDataInterface.cpp:296-300 idiom).
	const IPCGGraphExecutionSource* ExecutionSource = Binding->GetExecutionSource();
	if (ExecutionSource == nullptr)
	{
		return;
	}

	UWorld* World = ExecutionSource->GetExecutionState().GetWorld();
	const FBox WorldBounds = ExecutionSource->GetExecutionState().GetBounds();
	if (World == nullptr || !WorldBounds.IsValid)
	{
		return;
	}

	Data.ScatterBoundsMin = FVector3f(WorldBounds.Min);
	Data.ScatterBoundsExtent = FVector3f(WorldBounds.GetExtent());

	// [Step 3] Counted HERE, not baked at compilation: same function, same bounds, same cap as
	// UArcTerrainScatterKernel::ComputeOutputBindingDataDesc. If these two ever disagreed the shader
	// would lay its grid out over a different element count than the buffer holds.
	Data.ScatterPointCount = static_cast<uint32>(ComputePointCount(ScatterParams.Density, WorldBounds,
		GetArcTerrainScatterMaxPoints()));

	const int32 MaxCells = FMath::Clamp(CVarMaxCells.GetValueOnGameThread(), 1, GArcTerrainDIMaxSlots);

	// "The AArcTerrain intersecting those bounds" is the first terrain with a readable BAKED height tile
	// in range: the terrain is an infinite plane, so an actor-bounds test would answer for every terrain
	// in the world. The tile files are the persistent source; the live GPU composite is edit-time-only
	// (its stamp sources are WITH_EDITORONLY_DATA, so a packaged build's composite has no inputs).
	AArcTerrain* Terrain = nullptr;
	TArray<FArcTerrainCellKey> IntersectedCells;
	for (TActorIterator<AArcTerrain> It(World); It; ++It)
	{
		AArcTerrain* Candidate = *It;
		const FArcTerrainConfig& CandidateConfig = Candidate->Config;
		if (CandidateConfig.CellSize() <= 0.0)
		{
			continue;
		}

		// Corner transform, matching every CellsIntersectingLocalBounds call site's own pattern (a
		// conservative AABB of the transformed corners; the layout derives its cell rect from the
		// same edge-inclusive helper's rule, so the two agree bit for bit).
		const FVector CandidateLocalMin = Candidate->WorldToTerrainLocal(WorldBounds.Min);
		const FVector CandidateLocalMax = Candidate->WorldToTerrainLocal(WorldBounds.Max);
		const FBox CandidateLocalBounds(CandidateLocalMin.ComponentMin(CandidateLocalMax),
			CandidateLocalMin.ComponentMax(CandidateLocalMax));

		TArray<FArcTerrainCellKey> CandidateCells;
		ArcTerrain::CellsIntersectingLocalBounds(CandidateLocalBounds, CandidateConfig, CandidateCells);

		for (FArcTerrainCellKey Key : CandidateCells)
		{
			if (ResolveTile(*Candidate, EArcTerrainRail::Height, Key).IsValid())
			{
				Terrain = Candidate;
				break;
			}
		}

		if (Terrain != nullptr)
		{
			IntersectedCells = MoveTemp(CandidateCells);
			break;
		}
	}

	if (Terrain == nullptr)
	{
		PCG_KERNEL_VALIDATION_WARN(Context, Settings, LOCTEXT("NoTerrainCells",
			"ArcTerrain GPU sampler: no ArcTerrain cell with a baked height tile intersects the "
			"execution bounds (build the terrain first). Every point will be rejected as non-resident."));
		return;
	}

	const FArcTerrainConfig& Config = Terrain->Config;

	// The rasters come from the terrain's LATTICE, not from a reference payload: every cell of a rail
	// carries the same apron-inclusive resolution by construction.
	FArcTerrainTileGrid HeightGrid;
	if (!ArcTerrainTileAccess::ResolveGrid(*Terrain, EArcTerrainRail::Height, 0, HeightGrid))
	{
		return;
	}
	const int32 HeightTileRes = Config.QuadsPerCell() + 3;
	if (HeightTileRes < 4)
	{
		return;
	}

	// GAME THREAD, as the tri-state contract requires: the shader never sees an FName and cannot
	// distinguish "channel absent" from "weight is zero".
	bool bAuthoringError = false;
	Data.ScatterLayerChannel = ResolveLayerFilterChannel(ScatterParams.LayerChannel,
		Config.ChannelSet, bAuthoringError);

	if (bAuthoringError)
	{
		// Filter stays OFF (ResolveLayerFilterChannel returned -1) — an unresolvable channel must not
		// silently reject every point, but it must be visible on the node.
		PCG_KERNEL_VALIDATION_WARN(Context, Settings, FText::Format(
			LOCTEXT("UnresolvedLayerChannel",
				"ArcTerrain GPU sampler: layer channel '{0}' does not resolve against terrain '{1}' "
				"(no channel set, or the name is absent). The layer filter is DISABLED for this "
				"execution."),
			FText::FromName(ScatterParams.LayerChannel), FText::FromString(Terrain->GetName())));
	}

	// The tile names its planes by channel-name hash, never by the global index resolved above.
	const uint32 ScatterLayerChannelHash = ArcTerrain::ChannelNameHash(ScatterParams.LayerChannel);

	int32 WeightTileRes = 0;
	FArcTerrainTileGrid WeightGrid;
	if (Data.ScatterLayerChannel >= 0)
	{
		if (ArcTerrainTileAccess::ResolveGrid(*Terrain, EArcTerrainRail::Weight, 0, WeightGrid))
		{
			WeightTileRes = WeightGrid.TileQuads / WeightGrid.CellsPerTileEdge + 3;
		}

		// Whether any cell in range actually CARRIES the channel: the name resolved, but if no tile
		// holds a plane for it every sample would read 0 and the filter would reject EVERY point while
		// looking exactly like "no grass authored". Disable it and say so.
		bool bAnyPlane = false;
		if (WeightTileRes >= 4)
		{
			for (FArcTerrainCellKey Key : IntersectedCells)
			{
				const FArcTerrainTilePayloadPtr Payload = ResolveTile(*Terrain, EArcTerrainRail::Weight, Key);
				if (Payload.IsValid()
					&& Payload->FindWeightPlane(ScatterLayerChannelHash) != INDEX_NONE)
				{
					bAnyPlane = true;
					break;
				}
			}
		}

		if (!bAnyPlane)
		{
			PCG_KERNEL_VALIDATION_WARN(Context, Settings, FText::Format(
				LOCTEXT("LayerChannelNoWeightPlanes",
					"ArcTerrain GPU sampler: layer channel '{0}' resolves to index {1}, but no cell of "
					"terrain '{2}' in the execution bounds has a baked weight plane for it. Every sample "
					"of that channel would read 0 and reject every point, so the layer filter is DISABLED "
					"for this execution."),
				FText::FromName(ScatterParams.LayerChannel), FText::AsNumber(Data.ScatterLayerChannel),
				FText::FromString(Terrain->GetName())));

			Data.ScatterLayerChannel = INDEX_NONE;
			WeightTileRes = 0;
		}
	}

	const FVector LocalMin = Terrain->WorldToTerrainLocal(WorldBounds.Min);
	const FVector LocalMax = Terrain->WorldToTerrainLocal(WorldBounds.Max);
	const FBox LocalBounds(LocalMin.ComponentMin(LocalMax), LocalMin.ComponentMax(LocalMax));

	const FArcPCGAtlasLayout Layout = ComputeAtlasLayout(LocalBounds, Config.CellSize(), HeightTileRes,
		WeightTileRes, MaxCells);
	if (Layout.Slots.IsEmpty())
	{
		return;
	}

	// COVERAGE SHORTFALL, made node-visible. MaxCells shrank the atlas rectangle below what the
	// execution bounds actually span, so a part of the bounds is not sampled at all and every point
	// there is rejected as non-resident. Silent, that is indistinguishable from "no grass authored".
	if (Layout.bCellRectClamped)
	{
		PCG_KERNEL_VALIDATION_WARN(Context, Settings, FText::Format(
			LOCTEXT("AtlasCoverageClamped",
				"ArcTerrain GPU sampler: the execution bounds span {0}x{1} cells of terrain '{2}' but "
				"arc.PCG.TerrainGPU.MaxCells ({3}) allows only {4}x{5}. The remainder of the bounds is "
				"NOT sampled and will scatter nothing. Raise arc.PCG.TerrainGPU.MaxCells (hard cap {6}) "
				"or shrink the component's bounds."),
			FText::AsNumber(Layout.RequestedCellCount.X), FText::AsNumber(Layout.RequestedCellCount.Y),
			FText::FromString(Terrain->GetName()), FText::AsNumber(MaxCells),
			FText::AsNumber(Layout.CellCount.X), FText::AsNumber(Layout.CellCount.Y),
			FText::AsNumber(GArcTerrainDIMaxSlots)));
	}

	Data.AtlasOriginCell = Layout.OriginCell;
	Data.AtlasCellCount = Layout.CellCount;
	Data.HeightSubRect = Layout.HeightSubRect;
	Data.WeightSubRect = Layout.WeightSubRect;
	Data.CellSize = static_cast<float>(Config.CellSize());
	Data.HeightTileRes = static_cast<float>(HeightTileRes);
	Data.WeightTileRes = static_cast<float>(WeightTileRes);
	Data.TexelToLocal = static_cast<float>(Config.CellSize() / static_cast<double>(HeightTileRes - 3));

	SplitTerrainTransformLWC(Terrain->GetTerrainTransform(), Data.LWCTile, Data.TerrainLocalToWorld,
		Data.WorldToTerrainLocal, Data.TerrainInvNonUniformScale);

	Data.Slots.SetNum(Layout.Slots.Num());
	for (int32 SlotIndex = 0; SlotIndex < Layout.Slots.Num(); ++SlotIndex)
	{
		Data.Slots[SlotIndex].HeightSrcOrigin = Layout.Slots[SlotIndex].HeightSrcOrigin;
		Data.Slots[SlotIndex].WeightSrcOrigin = Layout.Slots[SlotIndex].WeightSrcOrigin;
	}

	// Slice count must fit the fixed-size per-slot origin arrays AND the uint32 residency mask, both of
	// which are indexed PER SLOT, so the slot count is the only real bound here. Defensive: the
	// arc.PCG.TerrainGPU.MaxCells clamp above already caps the layout at GArcTerrainDIMaxSlots, so this
	// cannot currently fire - it exists so a future change to either cap cannot silently produce a
	// 33-slot atlas whose residency shift is undefined behaviour.
	if (!ensure(Data.Slots.Num() <= GArcTerrainDIMaxSlots))
	{
		PCG_KERNEL_VALIDATION_WARN(Context, Settings, FText::Format(
			LOCTEXT("AtlasTooLarge",
				"ArcTerrain GPU sampler: the planned atlas needs {0} slices but only {1} exist. The "
				"terrain sampler is DISABLED for this execution (nothing will scatter). Lower "
				"arc.PCG.TerrainGPU.MaxCells."),
			FText::AsNumber(Data.Slots.Num()), FText::AsNumber(GArcTerrainDIMaxSlots)));

		// Disable the SAMPLER, do not wipe the struct: the scatter params and bounds every other early
		// return leaves intact are what the kernel still needs to lay out (and reject) its points.
		// An empty slot list means no atlas, dummy bindings, and IsArcTerrainResident() false.
		Data.Slots.Reset();
		Data.AtlasCellCount = FIntPoint::ZeroValue;
		return;
	}

	// Per-slot tile resolve. A cell with no readable tile is simply a HOLE: its residency bit stays
	// clear and the shader rejects every point on it.
	int32 NumResident = 0;
	int32 NumWeightSlots = 0;
	for (int32 SlotIndex = 0; SlotIndex < Layout.Slots.Num(); ++SlotIndex)
	{
		const FArcTerrainCellKey Key = Layout.Slots[SlotIndex].Key;
		FArcTerrainDataProxyData::FSlot& Slot = Data.Slots[SlotIndex];

		const FArcTerrainTilePayloadPtr HeightPayload = ResolveTile(*Terrain, EArcTerrainRail::Height, Key);
		if (!HeightPayload.IsValid())
		{
			continue;
		}

		TArray<float> Heights;
		int32 CellResolution = 0;
		if (!ArcTerrainTileAccess::ExtractCellFloats(HeightGrid, *HeightPayload, 0, Key, Heights,
				CellResolution)
			|| CellResolution != HeightTileRes)
		{
			continue;
		}

		TArray<uint8> HeightBytes;
		HeightBytes.SetNumUninitialized(Heights.Num() * sizeof(float));
		FMemory::Memcpy(HeightBytes.GetData(), Heights.GetData(), HeightBytes.Num());

		FArcTerrainDITileTexturePtr HeightTile = UploadTileTexture(MoveTemp(HeightBytes), HeightTileRes,
			sizeof(float), PF_R32_FLOAT, TEXT("ArcPCGTerrain_HeightTile"));
		if (!HeightTile.IsValid())
		{
			continue;
		}

		Slot.HeightTile = MoveTemp(HeightTile);
		Slot.bResident = true;
		++NumResident;

		// A tile with no plane for the channel means the cell does not carry the layer -> its weight
		// sample is 0, which is the correct per-cell answer, not an error.
		if (Data.ScatterLayerChannel >= 0)
		{
			const FArcTerrainTilePayloadPtr WeightPayload =
				ResolveTile(*Terrain, EArcTerrainRail::Weight, Key);
			if (!WeightPayload.IsValid())
			{
				continue;
			}

			const int32 PlaneIndex = WeightPayload->FindWeightPlane(ScatterLayerChannelHash);
			if (PlaneIndex == INDEX_NONE)
			{
				continue;
			}

			TArray<uint8> WeightBytes;
			int32 WeightCellResolution = 0;
			if (!ArcTerrainTileAccess::ExtractCellPlane(WeightGrid, *WeightPayload, PlaneIndex, Key,
					WeightBytes, WeightCellResolution)
				|| WeightCellResolution != WeightTileRes)
			{
				continue;
			}

			FArcTerrainDITileTexturePtr WeightTile = UploadTileTexture(MoveTemp(WeightBytes), WeightTileRes,
				1, PF_G8, TEXT("ArcPCGTerrain_WeightTile"));
			if (!WeightTile.IsValid())
			{
				continue;
			}

			Slot.WeightTile = MoveTemp(WeightTile);
			// One plane per upload, so the copy always reads slice 0 of its own texture.
			Slot.WeightSourceSlice = 0;
			++NumWeightSlots;
		}
	}

	// SUCCESS TELEMETRY, deliberately at Log and unconditional. Every other message in this function
	// fires only on a fault, which means a silent log is ambiguous in the worst possible way: "the
	// sampler ran and was happy" and "Initialize was never called at all" look identical, and the
	// second is the common case in the editor (a GenerateAtRuntime component does not generate outside
	// PIE unless the PCG world actor treats the viewport as a generation source). One line removes the
	// ambiguity and gives the point budget without attaching a debugger.
	UE_LOGF(LogArcPCGForest, Log,"ArcTerrain GPU sampler: terrain='%ls' cells=%d resident of %d slots (atlas %dx%d, "
			 "requested %dx%d) weightSlots=%d bounds=(%ls)-(%ls) points=%u layerChannel=%d density=%.3f",
		*Terrain->GetName(), NumResident, Data.Slots.Num(),
		Data.AtlasCellCount.X, Data.AtlasCellCount.Y,
		Layout.RequestedCellCount.X, Layout.RequestedCellCount.Y, NumWeightSlots,
		*WorldBounds.Min.ToCompactString(), *WorldBounds.Max.ToCompactString(),
		Data.ScatterPointCount, Data.ScatterLayerChannel, ScatterParams.Density);
}

const UPCGSettings* UArcTerrainDataProvider::ResolveNodeSettings() const
{
	// Preferred, and the only one PCG itself populates for a PIN-BOUND data interface.
	const UPCGSettings* Settings = GetProducerSettings();

	// The route that actually works for an ADDITIONAL data interface: the producing kernel. PCG copies
	// the DI's producer kernel into every provider (UPCGComputeDataProvider::Initialize), and
	// UPCGComputeKernel::GetSettings() is exported - unlike SetProducerSettings, which is not callable
	// from outside the PCG module at all (PCGComputeDataInterface.h:31, no PCG_API on a MinimalAPI class).
	if (Settings == nullptr)
	{
		if (const UPCGComputeKernel* Kernel = GetProducerKernel())
		{
			Settings = Kernel->GetSettings();
		}
	}

	// LOUD, never silent: with a null settings pointer PCGComputeCommon's LogKernelWarning/Error take
	// their log-only branch (PCGComputeCommon.cpp:198-218 requires `Context && Settings`), so every
	// diagnostic this provider raises would vanish from the node - the exact failure mode those
	// messages exist to prevent. If this ever fires, the producing kernel stopped calling
	// SetProducerKernel(this) in CreateAdditionalInputDataInterfaces.
	if (!ensureMsgf(Settings != nullptr,
		TEXT("UArcTerrainDataProvider: no producer settings and no producer kernel; node-visible "
			 "validation messages will degrade to log-only.")))
	{
		UE_LOGF(LogArcPCGForest, Warning,"UArcTerrainDataProvider: producer settings unresolved (producer kernel %ls). ArcTerrain "
				 "GPU sampler warnings will NOT appear on the node.",
			GetProducerKernel() ? TEXT("present, but its settings are null") : TEXT("absent"));
	}

	return Settings;
}

void UArcTerrainDataProvider::BeginDestroy()
{
	// The keep-alive is a plain UPROPERTY hold, so GC would drop it anyway — clearing here just
	// releases the sections at the earliest defined point instead of at the provider's finalization.

	Super::BeginDestroy();
}

FComputeDataProviderRenderProxy* UArcTerrainDataProvider::GetRenderProxy()
{
	return new FArcTerrainDataProviderProxy(Data);
}

void UArcTerrainDataProvider::Reset()
{
	Data = FArcTerrainDataProxyData();

	Super::Reset();
}

bool FArcTerrainDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return InValidationData.ParameterStructSize == sizeof(FParameters);
}

void FArcTerrainDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder,
	FAllocationData const& InAllocationData)
{
	using namespace ArcPCGExtensionTerrainDataInterfacePrivate;

	// REBUILT EVERY FRAME BY DESIGN. A section rebuild/reload between generations allocates a
	// brand-new texture object, so caching an FRHITexture across frames samples a superseded one.
	ResidencyMask = 0;
	HeightAtlasSRV = nullptr;
	WeightAtlasSRV = nullptr;

	const int32 NumSlots = Data.Slots.Num();

	if (NumSlots > 0 && Data.HeightSubRect.X > 0 && Data.HeightSubRect.Y > 0)
	{
		FRDGTextureRef HeightAtlas = GraphBuilder.CreateTexture(
			// PF_R32_FLOAT matches the uploaded height tile — copies require
			// source/destination format identity.
			//
			// TargetArraySlicesIndependently is REQUIRED, not an optimisation: AddClearRenderTargetPass
			// with NumSlices > 1 hits a checkf in RDG unless the array was created with it (or the RHI
			// happens to set GRHISupportsBindingTexArrayPerSlice, which is not guaranteed). The single-
			// cell case slips through because (FirstSliceIndex == 0 && NumSlices == 1) satisfies the
			// assert on its own — so this only fires once bounds span more than one terrain cell.
			FRDGTextureDesc::Create2DArray(Data.HeightSubRect, PF_R32_FLOAT, FClearValueBinding::Black,
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable
					| ETextureCreateFlags::TargetArraySlicesIndependently,
				NumSlots),
			TEXT("ArcTerrainDI_HeightAtlas"));

		// Clear ALL slices first so a non-resident slice is well-defined zero rather than RDG garbage
		// (the shader gates on the mask, but an unwritten resource also trips RDG validation).
		FRDGTextureClearInfo ClearInfo;
		ClearInfo.FirstSliceIndex = 0;
		ClearInfo.NumSlices = static_cast<uint32>(NumSlots);
		AddClearRenderTargetPass(GraphBuilder, HeightAtlas, ClearInfo);

		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			const FArcTerrainDataProxyData::FSlot& Slot = Data.Slots[SlotIndex];
			if (!Slot.bResident)
			{
				continue;
			}

			FRDGTextureRef Source = RegisterTileSource(GraphBuilder, Slot.HeightTile,
				Slot.HeightSrcOrigin, Data.HeightSubRect, PF_R32_FLOAT,
				TEXT("ArcTerrainDI_SrcHeightTile"));
			if (Source == nullptr)
			{
				continue;
			}

			// SLICE PER CELL, deliberately: separate slices remove cross-cell bilinear bleed as a
			// CATEGORY, which is why no per-slot UV clamp exists anywhere below.
			CopySubRect(GraphBuilder, Source, HeightAtlas, Slot.HeightSrcOrigin, Data.HeightSubRect,
				/*SrcSlice=*/0, SlotIndex);
			ResidencyMask |= 1u << static_cast<uint32>(SlotIndex);
		}

		HeightAtlasSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(HeightAtlas));
	}

	// ONE weight slice per atlas slot: the game thread already resolved which slice of each cell's
	// PF_G8 weight array holds the filter channel (WeightSourceSlice), so the atlas carries exactly
	// that slice per cell — the RGBA channel-packed scheme is gone with the live composite.
	bool bAnyWeightSlot = false;
	for (const FArcTerrainDataProxyData::FSlot& Slot : Data.Slots)
	{
		bAnyWeightSlot |= Slot.WeightTile.IsValid();
	}

	if (NumSlots > 0 && bAnyWeightSlot && Data.WeightSubRect.X > 0 && Data.WeightSubRect.Y > 0)
	{
		FRDGTextureRef WeightAtlas = GraphBuilder.CreateTexture(
			// Same per-slice clear requirement as the height atlas above; PF_G8 matches the baked
			// the uploaded weight plane so the copy is format-identical.
			FRDGTextureDesc::Create2DArray(Data.WeightSubRect, PF_G8, FClearValueBinding::Black,
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable
					| ETextureCreateFlags::TargetArraySlicesIndependently,
				NumSlots),
			TEXT("ArcTerrainDI_WeightAtlas"));

		FRDGTextureClearInfo ClearInfo;
		ClearInfo.FirstSliceIndex = 0;
		ClearInfo.NumSlices = static_cast<uint32>(NumSlots);
		AddClearRenderTargetPass(GraphBuilder, WeightAtlas, ClearInfo);

		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			const FArcTerrainDataProxyData::FSlot& Slot = Data.Slots[SlotIndex];
			if (!Slot.bResident || !Slot.WeightTile.IsValid() || Slot.WeightSourceSlice < 0)
			{
				continue;
			}

			FRDGTextureRef Source = RegisterTileSource(GraphBuilder, Slot.WeightTile,
				Slot.WeightSrcOrigin, Data.WeightSubRect, PF_G8,
				TEXT("ArcTerrainDI_SrcWeightTile"));
			if (Source == nullptr)
			{
				continue;
			}

			// A slice index past the source array degrades to a zero weight rather than an RHI copy from
			// a nonexistent slice.
			if (Slot.WeightSourceSlice >= static_cast<int32>(Source->Desc.ArraySize))
			{
				continue;
			}

			CopySubRect(GraphBuilder, Source, WeightAtlas, Slot.WeightSrcOrigin, Data.WeightSubRect,
				Slot.WeightSourceSlice, SlotIndex);
		}

		WeightAtlasSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(WeightAtlas));
	}

	// Fallbacks in the shape of SetCollisionHeightTextureParameters_Defaults
	// (PCGLandscapeDataInterface.cpp:889-899): a bound-but-dead resource, never a null binding.
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
}

void FArcTerrainDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	FRHISamplerState* BilinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	FRHISamplerState* PointSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Filterability is a per-adapter runtime result. On the point fallback the central difference
	// degrades to a texel-quantized difference — the normal stays plausible but stops being smooth.
	FRHISamplerState* HeightSampler = RHIPixelFormatHasCapabilities(EPixelFormat::PF_R32_FLOAT,
		EPixelFormatCapabilities::TextureFilterable) ? BilinearSampler : PointSampler;

	const FVector2f HeightSubRectInv(
		Data.HeightSubRect.X > 0 ? 1.0f / static_cast<float>(Data.HeightSubRect.X) : 0.0f,
		Data.HeightSubRect.Y > 0 ? 1.0f / static_cast<float>(Data.HeightSubRect.Y) : 0.0f);
	const FVector2f WeightSubRectInv(
		Data.WeightSubRect.X > 0 ? 1.0f / static_cast<float>(Data.WeightSubRect.X) : 0.0f,
		Data.WeightSubRect.Y > 0 ? 1.0f / static_cast<float>(Data.WeightSubRect.Y) : 0.0f);

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.HeightAtlas = HeightAtlasSRV;
		Parameters.HeightAtlasSampler = HeightSampler;
		Parameters.WeightAtlas = WeightAtlasSRV;
		Parameters.WeightAtlasSampler = BilinearSampler;
		Parameters.ResidencyMask = ResidencyMask;
		Parameters.WorldToTerrainLocal = Data.WorldToTerrainLocal;
		Parameters.TerrainLocalToWorld = Data.TerrainLocalToWorld;
		Parameters.TerrainInvNonUniformScale = Data.TerrainInvNonUniformScale;
		Parameters.LWCTile = Data.LWCTile;
		Parameters.AtlasOriginCell = Data.AtlasOriginCell;
		Parameters.AtlasCellCount = Data.AtlasCellCount;
		Parameters.HeightSubRectInv = HeightSubRectInv;
		Parameters.WeightSubRectInv = WeightSubRectInv;
		Parameters.CellSize = Data.CellSize;
		Parameters.HeightTileRes = Data.HeightTileRes;
		Parameters.WeightTileRes = Data.WeightTileRes;
		Parameters.TexelToLocal = Data.TexelToLocal;
		Parameters.ScatterBoundsMin = Data.ScatterBoundsMin;
		Parameters.ScatterBoundsExtent = Data.ScatterBoundsExtent;
		Parameters.ScatterPointCount = Data.ScatterPointCount;
		Parameters.ScatterSeed = Data.ScatterSeed;
		Parameters.ScatterSlopeRange = Data.ScatterSlopeRange;
		Parameters.ScatterLayerChannel = Data.ScatterLayerChannel;
		Parameters.ScatterMinLayerWeight = Data.ScatterMinLayerWeight;
		Parameters.ScatterAlignToNormal = Data.ScatterAlignToNormal;

		for (int32 SlotIndex = 0; SlotIndex < GArcTerrainDIMaxSlots; ++SlotIndex)
		{
			const bool bHasSlot = Data.Slots.IsValidIndex(SlotIndex);
			const FIntPoint HeightOrigin = bHasSlot ? Data.Slots[SlotIndex].HeightSrcOrigin : FIntPoint::ZeroValue;
			const FIntPoint WeightOrigin = bHasSlot ? Data.Slots[SlotIndex].WeightSrcOrigin : FIntPoint::ZeroValue;

			// .z carries the per-slot "does this cell carry the filter channel" flag: the weight atlas
			// holds ONE slice per slot, and a cell without the channel must sample 0, not slice garbage.
			const bool bHasWeight = bHasSlot && Data.Slots[SlotIndex].WeightTile.IsValid();

			Parameters.HeightSrcOriginPerSlot[SlotIndex] =
				FVector4f(static_cast<float>(HeightOrigin.X), static_cast<float>(HeightOrigin.Y), 0.0f, 0.0f);
			Parameters.WeightSrcOriginPerSlot[SlotIndex] =
				FVector4f(static_cast<float>(WeightOrigin.X), static_cast<float>(WeightOrigin.Y),
					bHasWeight ? 1.0f : 0.0f, 0.0f);
		}
	}
}

#undef LOCTEXT_NAMESPACE
