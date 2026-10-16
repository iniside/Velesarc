// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainVertexFactory.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalRenderResources.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "RHIResourceUtils.h"
#include "RHIStaticStates.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FArcTerrainVFParameters, "ArcTerrainVF");

namespace ArcTerrainVertexFactoryLocal_4f21
{
	/**
	 * The factory owns its sampler and substitutes GBlackArrayTexture for a null atlas so the uniform
	 * buffer is always valid to create and draw with.
	 */
	void ApplyBindingDefaults(FArcTerrainVFParameters& Params)
	{
		if (Params.HeightAtlas == nullptr)
		{
			Params.HeightAtlas = GBlackArrayTexture->TextureRHI;
		}
		Params.HeightSamplerBilinear = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
}

void FArcTerrainCBTRefIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	const uint16 Indices[3] = { 0, 1, 2 };
	IndexBufferRHI = UE::RHIResourceUtils::CreateIndexBufferFromArray(
		RHICmdList, TEXT("FArcTerrainCBTRefIndexBuffer"), EBufferUsageFlags::Static, MakeConstArrayView(Indices, 3));
}

/**
 * Shader parameter bindings: the terrain-wide uniform buffer plus the per-batch loose parameters
 * sourced from FMeshBatchElement::UserData (FArcTerrainVFUserData).
 */
class FArcTerrainVFShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FArcTerrainVFShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		CBTHeapParameter.Bind(ParameterMap, TEXT("CBTHeap"));
		CBTBisectorsParameter.Bind(ParameterMap, TEXT("CBTBisectors"));
		VisibleBisectorsParameter.Bind(ParameterMap, TEXT("ArcTerrainVisibleBisectors"));
		FaceParamsParameter.Bind(ParameterMap, TEXT("ArcTerrainFaceParams"));
		HeightRingParameter.Bind(ParameterMap, TEXT("ArcTerrainHeightRing"));
		WeightRingParameter.Bind(ParameterMap, TEXT("ArcTerrainWeightRing"));
		PoolDepthParameter.Bind(ParameterMap, TEXT("CBTPoolDepth"));
		NumFacesParameter.Bind(ParameterMap, TEXT("ArcTerrainNumFaces"));
		UseVisibleListParameter.Bind(ParameterMap, TEXT("ArcTerrainUseVisibleList"));
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
		const FArcTerrainVertexFactory* VertexFactory = static_cast<const FArcTerrainVertexFactory*>(InVertexFactory);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FArcTerrainVFParameters>(), VertexFactory->UniformBuffer);

		const FArcTerrainVFUserData* UserData = static_cast<const FArcTerrainVFUserData*>(BatchElement.UserData);

		FRHIShaderResourceView* const HeapSRV =
			(UserData != nullptr && UserData->CBTHeapSRV != nullptr) ? UserData->CBTHeapSRV : VertexFactory->GetRawFallbackSRV();
		FRHIShaderResourceView* const BisectorSRV =
			(UserData != nullptr && UserData->CBTBisectorSRV != nullptr) ? UserData->CBTBisectorSRV : VertexFactory->GetRawFallbackSRV();
		// The list is only READ when the batch says so, but the binding must always resolve — an unbound
		// SRV is a driver-level invalid draw whatever the branch does.
		const bool bUseVisibleList = UserData != nullptr && UserData->bUseVisibleList != 0u
			&& UserData->VisibleBisectorSRV != nullptr;
		FRHIShaderResourceView* const VisibleBisectorSRV =
			bUseVisibleList ? UserData->VisibleBisectorSRV : VertexFactory->GetRawFallbackSRV();
		FRHIShaderResourceView* const FaceParamsSRV =
			(UserData != nullptr && UserData->FaceParamsSRV != nullptr) ? UserData->FaceParamsSRV : VertexFactory->GetFaceParamsFallbackSRV();
		FRHIShaderResourceView* const HeightRingSRV =
			(UserData != nullptr && UserData->HeightRingSRV != nullptr) ? UserData->HeightRingSRV : VertexFactory->GetRingFallbackSRV();
		FRHIShaderResourceView* const WeightRingSRV =
			(UserData != nullptr && UserData->WeightRingSRV != nullptr) ? UserData->WeightRingSRV : VertexFactory->GetRingFallbackSRV();

		ShaderBindings.Add(CBTHeapParameter, HeapSRV);
		ShaderBindings.Add(CBTBisectorsParameter, BisectorSRV);
		ShaderBindings.Add(VisibleBisectorsParameter, VisibleBisectorSRV);
		ShaderBindings.Add(FaceParamsParameter, FaceParamsSRV);
		ShaderBindings.Add(HeightRingParameter, HeightRingSRV);
		ShaderBindings.Add(WeightRingParameter, WeightRingSRV);
		ShaderBindings.Add(PoolDepthParameter, UserData != nullptr ? UserData->PoolDepth : 0u);
		ShaderBindings.Add(NumFacesParameter, UserData != nullptr ? UserData->NumFaces : 0u);
		ShaderBindings.Add(UseVisibleListParameter, bUseVisibleList ? 1u : 0u);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, CBTHeapParameter);
	LAYOUT_FIELD(FShaderResourceParameter, CBTBisectorsParameter);
	LAYOUT_FIELD(FShaderResourceParameter, VisibleBisectorsParameter);
	LAYOUT_FIELD(FShaderResourceParameter, FaceParamsParameter);
	LAYOUT_FIELD(FShaderResourceParameter, HeightRingParameter);
	LAYOUT_FIELD(FShaderResourceParameter, WeightRingParameter);
	LAYOUT_FIELD(FShaderParameter, PoolDepthParameter);
	LAYOUT_FIELD(FShaderParameter, NumFacesParameter);
	LAYOUT_FIELD(FShaderParameter, UseVisibleListParameter);
};

IMPLEMENT_TYPE_LAYOUT(FArcTerrainVFShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FArcTerrainVertexFactory, SF_Vertex, FArcTerrainVFShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FArcTerrainVertexFactory, SF_Pixel, FArcTerrainVFShaderParameters);

FArcTerrainVertexFactory::FArcTerrainVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	: FVertexFactory(InFeatureLevel)
{
	// Neutral params so InitRHI can create a valid uniform buffer before the first
	// SetTerrainParams_RenderThread.
	Params.SectorQuads = 1;
	Params.FaceQuads = 1;
	Params.SectorQuadsShift = 0;
	Params.WeightQuadsShift = 0;
	Params.TexelToLocal = 1.0f;
	Params.InvTileResolution = 0.25f; // 1 / (1 + 3)
	// EVERY component, not just .x: a default-constructed vector is zeros and zero is a legal radius, so a
	// key of (0,0) would hit a 1x1 window and read the fallback SRV.
	Params.HeightRingRadius = FIntVector4(-1, -1, -1, -1);
	Params.NumHeightTiers = 0u;
	Params.WeightRingRadius = FIntVector4(-1, -1, -1, -1);
	Params.NumWeightTiers = 0u;
	Params.bResourcesValid = 0u;
}

void FArcTerrainVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	ArcTerrainVertexFactoryLocal_4f21::ApplyBindingDefaults(Params);
	UniformBuffer = FArcTerrainVFBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

	// Vertexless: one null ManualFetch stream and an empty declaration; the vertex is synthesized
	// from SV_InstanceID / SV_VertexID in the shader (VirtualHeightfieldMesh pattern).
	FVertexStream NullVertexStream;
	NullVertexStream.VertexBuffer = nullptr;
	NullVertexStream.Stride = 0;
	NullVertexStream.Offset = 0;
	NullVertexStream.VertexStreamUsage = EVertexStreamUsage::ManualFetch;

	check(Streams.Num() == 0);
	Streams.Add(NullVertexStream);

	FVertexDeclarationElementList Elements;
	InitDeclaration(Elements);

	CBTRefIndexBuffer.InitResource(RHICmdList);

	const FRHIBufferCreateDesc RawDesc =
		FRHIBufferCreateDesc::CreateByteAddress(TEXT("ArcTerrainVF.RawFallback"), 16u, 4u)
		.AddUsage(EBufferUsageFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask);
	RawFallbackBuffer = RHICmdList.CreateBuffer(RawDesc);
	RawFallbackSRV = RHICmdList.CreateShaderResourceView(
		RawFallbackBuffer,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));

	const uint32 FallbackFace[4] = { 0u, 0u, 0u, 0u };
	FaceParamsFallbackBuffer = UE::RHIResourceUtils::CreateVertexBufferFromArray(
		RHICmdList, TEXT("ArcTerrainVF.FaceParamsFallback"), EBufferUsageFlags::ShaderResource,
		MakeConstArrayView(FallbackFace, 4));
	FaceParamsFallbackSRV = RHICmdList.CreateShaderResourceView(
		FaceParamsFallbackBuffer,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Typed).SetFormat(PF_R32G32B32A32_UINT));

	const uint32 FallbackRing[1] = { 0u };
	RingFallbackBuffer = UE::RHIResourceUtils::CreateVertexBufferFromArray(
		RHICmdList, TEXT("ArcTerrainVF.RingFallback"), EBufferUsageFlags::ShaderResource,
		MakeConstArrayView(FallbackRing, 1));
	RingFallbackSRV = RHICmdList.CreateShaderResourceView(
		RingFallbackBuffer,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Typed).SetFormat(PF_R32_UINT));
}

void FArcTerrainVertexFactory::ReleaseRHI()
{
	RingFallbackSRV.SafeRelease();
	RingFallbackBuffer.SafeRelease();
	FaceParamsFallbackSRV.SafeRelease();
	FaceParamsFallbackBuffer.SafeRelease();
	RawFallbackSRV.SafeRelease();
	RawFallbackBuffer.SafeRelease();
	CBTRefIndexBuffer.ReleaseResource();
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

bool FArcTerrainVertexFactory::SetTerrainParams_RenderThread(
	FRHICommandListBase& RHICmdList, const FArcTerrainVFParameters& InParams)
{
	Params = InParams;
	ArcTerrainVertexFactoryLocal_4f21::ApplyBindingDefaults(Params);

	if (!UniformBuffer.IsValid())
	{
		return false;
	}

	UniformBuffer.UpdateUniformBufferImmediate(RHICmdList, Params);
	return UniformBuffer.IsValid();
}

bool FArcTerrainVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
	{
		return false;
	}

	// Reuse the VirtualHeightfieldMesh material usage gate ("heightfield displaced by VF" — same
	// semantics); the special engine material covers the DefaultMaterial fallback.
	return Parameters.MaterialParameters.MaterialDomain == MD_Surface
		&& (Parameters.MaterialParameters.bIsUsedWithVirtualHeightfieldMesh || Parameters.MaterialParameters.bIsSpecialEngineMaterial);
}

void FArcTerrainVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("VF_ARCTERRAIN"), 1);
}

#if WITH_EDITOR

void FArcTerrainVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
	// Permissive: GPUScene is off for this factory, so binding the Primitive uniform buffer is
	// the intended fallback path — nothing to reject here.
}

#endif // WITH_EDITOR

void FArcTerrainVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	// Vertexless factory: no vertex fetch elements — matches the empty runtime declaration.
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FArcTerrainVertexFactory, "/Plugin/ArcTerrain/ArcTerrainVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPSOPrecaching
);
