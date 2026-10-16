// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOceanVertexFactory.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "PixelFormat.h"
#include "RHICommandList.h"
#include "RenderResource.h"
#include "TextureResource.h"

#include "Render/ArcTerrainAtlasTexture.h"

#include "Ocean/ArcOceanRingGeometry.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FArcOceanVFParameters, "ArcOceanVF");

static_assert(ArcTerrainPublishedMaxSlots <= 32,
	"ArcTerrainAtlasSample.ush selects slots out of a uint32 mask with 1u << SlotIndex.");

namespace ArcOceanVertexFactoryLocal_1c74
{
	class FDefaultSeabedTexture : public FTexture
	{
	public:
		virtual void InitRHI(FRHICommandListBase& RHICmdList) override
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2DArray(TEXT("ArcOcean.DefaultSeabed"))
				.SetExtent(1, 1)
				.SetArraySize(1)
				.SetFormat(PF_R32_FLOAT)
				.SetFlags(ETextureCreateFlags::ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);

			TextureRHI = RHICmdList.CreateTexture(Desc);
			SamplerStateRHI = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}

		virtual uint32 GetSizeX() const override { return 1; }
		virtual uint32 GetSizeY() const override { return 1; }
	};

	class FDefaultFoamTexture : public FTexture
	{
	public:
		virtual void InitRHI(FRHICommandListBase& RHICmdList) override
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("ArcOcean.DefaultFoam"))
				.SetExtent(1, 1)
				.SetFormat(PF_R16F)
				.SetFlags(ETextureCreateFlags::ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);

			TextureRHI = RHICmdList.CreateTexture(Desc);
			SamplerStateRHI = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}

		virtual uint32 GetSizeX() const override { return 1; }
		virtual uint32 GetSizeY() const override { return 1; }
	};

	FRHITexture* ResolveAtlasTexture(FArcTerrainAtlasTextureResource* AtlasResource)
	{
		if (AtlasResource == nullptr)
		{
			return nullptr;
		}

		FRHITexture* const Texture = AtlasResource->TextureRHI.GetReference();
		if (Texture == nullptr)
		{
			return nullptr;
		}

		const FRHITextureDesc& Desc = Texture->GetDesc();
		if (Desc.Dimension != ETextureDimension::Texture2DArray || Desc.Format != PF_R32_FLOAT)
		{
			return nullptr;
		}

		return Texture;
	}

	void WriteSnapshot(FArcOceanVFParameters& OutParams, const FArcTerrainAtlasSnapshot& InSnapshot)
	{
		OutParams.SeabedWorldToLocal = InSnapshot.WorldToLocal;
		OutParams.SeabedLocalToWorld = InSnapshot.LocalToWorld;
		OutParams.SeabedLWCTile = InSnapshot.LWCTile;
		OutParams.SeabedOriginSector = InSnapshot.OriginSector;
		OutParams.SeabedSectorSide = InSnapshot.SectorSide;
		OutParams.SeabedSectorShift = InSnapshot.SectorShift;
		OutParams.SeabedCellSizeCm = InSnapshot.CellSizeCm;
		OutParams.SeabedHeightQuads = static_cast<float>(InSnapshot.HeightQuads);
		OutParams.SeabedHeightSliceSide = static_cast<float>(InSnapshot.HeightSliceSide);
		OutParams.SeabedValidSlotMask = InSnapshot.ValidSlotMask;

		for (int32 SlotIndex = 0; SlotIndex < ArcTerrainPublishedMaxSlots; ++SlotIndex)
		{
			OutParams.SeabedSliceAndOriginPerSlot[SlotIndex] = InSnapshot.SliceAndOriginPerSlot[SlotIndex];
		}
	}
}

static TGlobalResource<ArcOceanVertexFactoryLocal_1c74::FDefaultSeabedTexture> GArcOceanDefaultSeabedTexture;
static TGlobalResource<ArcOceanVertexFactoryLocal_1c74::FDefaultFoamTexture> GArcOceanDefaultFoamTexture;

void ArcOcean::SeedSeabedParams(FArcOceanVFParameters& OutParams)
{
	const FArcTerrainAtlasSnapshot EmptySnapshot;
	ArcOceanVertexFactoryLocal_1c74::WriteSnapshot(OutParams, EmptySnapshot);

	OutParams.SeabedHeightAtlas = GArcOceanDefaultSeabedTexture.TextureRHI;
	OutParams.SeabedHeightAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
}

void ArcOcean::ApplySeabedFields(FArcOceanVFParameters& OutParams, const FArcOceanSeabedFields& InFields)
{
	FRHITexture* const AtlasTexture = ArcOceanVertexFactoryLocal_1c74::ResolveAtlasTexture(InFields.AtlasResource);

	if (AtlasTexture != nullptr)
	{
		ArcOceanVertexFactoryLocal_1c74::WriteSnapshot(OutParams, InFields.Snapshot);
		OutParams.SeabedHeightAtlas = AtlasTexture;
		OutParams.SeabedHeightAtlasSampler = InFields.bAtlasFilterable
			? TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI()
			: TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
	else
	{
		ArcOcean::SeedSeabedParams(OutParams);
	}

	OutParams.DeepDepthCm = InFields.DeepDepthCm;
}

void ArcOcean::SeedFoamParams(FArcOceanVFParameters& OutParams)
{
	OutParams.FoamWindowOriginHigh = FVector3f::ZeroVector;
	OutParams.FoamWindowOriginLow = FVector3f::ZeroVector;
	OutParams.FoamWindowSizeCm = 0.0f;
	OutParams.FoamAccumulator = GArcOceanDefaultFoamTexture.TextureRHI;
	OutParams.FoamAccumulatorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
}

void ArcOcean::ApplyFoamFields(FArcOceanVFParameters& OutParams, const FArcOceanFoamFields& InFields)
{
	if (InFields.Texture == nullptr || InFields.WindowSizeCm <= 0.0f)
	{
		ArcOcean::SeedFoamParams(OutParams);
		return;
	}

	OutParams.FoamWindowOriginHigh = InFields.WindowOriginHigh;
	OutParams.FoamWindowOriginLow = InFields.WindowOriginLow;
	OutParams.FoamWindowSizeCm = InFields.WindowSizeCm;
	OutParams.FoamAccumulator = InFields.Texture;
	OutParams.FoamAccumulatorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
}

class FArcOceanVFShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FArcOceanVFShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		CellSizeParameter.Bind(ParameterMap, TEXT("ArcOceanCellSize"));
		SnappedOriginParameter.Bind(ParameterMap, TEXT("ArcOceanSnappedOriginTranslatedWorld"));
		TrimRotationStepsParameter.Bind(ParameterMap, TEXT("ArcOceanTrimRotationSteps"));
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const class FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const class FVertexFactory* InVertexFactory,
		const struct FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		const FArcOceanVertexFactory* VertexFactory = static_cast<const FArcOceanVertexFactory*>(InVertexFactory);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FArcOceanVFParameters>(), VertexFactory->UniformBuffer);

		const FArcOceanVFUserData* UserData = static_cast<const FArcOceanVFUserData*>(BatchElement.UserData);

		ShaderBindings.Add(CellSizeParameter, UserData != nullptr ? UserData->CellSize : 0.0f);
		ShaderBindings.Add(SnappedOriginParameter, UserData != nullptr ? UserData->SnappedOriginTranslatedWorld : FVector2f::ZeroVector);
		ShaderBindings.Add(TrimRotationStepsParameter, UserData != nullptr ? UserData->TrimRotationSteps : 0u);
	}

protected:
	LAYOUT_FIELD(FShaderParameter, CellSizeParameter);
	LAYOUT_FIELD(FShaderParameter, SnappedOriginParameter);
	LAYOUT_FIELD(FShaderParameter, TrimRotationStepsParameter);
};

IMPLEMENT_TYPE_LAYOUT(FArcOceanVFShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FArcOceanVertexFactory, SF_Vertex, FArcOceanVFShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FArcOceanVertexFactory, SF_Pixel, FArcOceanVFShaderParameters);

FArcOceanVertexFactory::FArcOceanVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FVertexBuffer* InGridVertexBuffer)
	: FVertexFactory(InFeatureLevel)
	, GridVertexBuffer(InGridVertexBuffer)
{
	check(GridVertexBuffer != nullptr);

	for (int32 SlotIndex = 0; SlotIndex < ArcOcean::MaxWaves * 2; ++SlotIndex)
	{
		Params.Waves[SlotIndex] = FVector4f::Zero();
	}
	Params.WaveCount = 0;
	Params.DeepDepthCm = 0.0f;
}

void FArcOceanVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	ArcOcean::SeedSeabedParams(Params);
	ArcOcean::SeedFoamParams(Params);

	UniformBuffer = FArcOceanVFBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

	check(Streams.Num() == 0);

	FVertexStream GridStream;
	GridStream.VertexBuffer = GridVertexBuffer;
	GridStream.Stride = FArcOceanRingGeometry::GetVertexStride();
	GridStream.Offset = 0;
	GridStream.VertexStreamUsage = EVertexStreamUsage::Default;

	const uint8 StreamIndex = static_cast<uint8>(Streams.Add(GridStream));
	const uint16 Stride = static_cast<uint16>(GridStream.Stride);

	FVertexDeclarationElementList Elements;
	Elements.Add(FVertexElement(StreamIndex, STRUCT_OFFSET(FArcOceanGridVertex, GridX), VET_Short2, 0, Stride, false));
	Elements.Add(FVertexElement(StreamIndex, STRUCT_OFFSET(FArcOceanGridVertex, MorphWeight), VET_Float1, 1, Stride, false));

	InitDeclaration(Elements);
}

void FArcOceanVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

void FArcOceanVertexFactory::UpdateParams_RenderThread(FRHICommandListBase& RHICmdList, const FArcOceanVFParameters& InParams)
{
	Params = InParams;

	if (UniformBuffer.IsValid())
	{
		UniformBuffer.UpdateUniformBufferImmediate(RHICmdList, Params);
	}
}

bool FArcOceanVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
	{
		return false;
	}

	return (Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.bIsUsedWithWater)
		|| Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FArcOceanVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("VF_ARCOCEAN"), 1);
	OutEnvironment.SetDefine(TEXT("ARC_OCEAN_MAX_WAVES"), ArcOcean::MaxWaves);
	OutEnvironment.SetDefine(TEXT("ARC_OCEAN_TRIM_ROTATION_COORD_SUM"), FArcOceanRingGeometry::TrimRotationCoordSum);
	OutEnvironment.SetDefine(TEXT("ARC_TERRAIN_ATLAS_MAX_SLOTS"), ArcTerrainPublishedMaxSlots);
}

#if WITH_EDITOR

void FArcOceanVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
}

#endif // WITH_EDITOR

void FArcOceanVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	const uint16 Stride = static_cast<uint16>(FArcOceanRingGeometry::GetVertexStride());
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FArcOceanGridVertex, GridX), VET_Short2, 0, Stride, false));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FArcOceanGridVertex, MorphWeight), VET_Float1, 1, Stride, false));
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FArcOceanVertexFactory, "/Plugin/ArcWater/ArcOceanVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPSOPrecaching
);
