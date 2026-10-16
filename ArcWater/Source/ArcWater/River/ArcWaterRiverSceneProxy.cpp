// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverSceneProxy.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshBatch.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "PrimitiveViewRelevance.h"
#include "River/ArcWaterRiverComponent.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"

namespace ArcWaterRiverSceneProxyLocal_7f31
{
	const FName GProxyResourceName(TEXT("ArcWaterRiver"));
	const char* const GVertexFactoryDebugName = "FArcWaterRiverSceneProxy";
}

FArcWaterRiverSceneProxy::FArcWaterRiverSceneProxy(const UArcWaterRiverComponent* InComponent,
	TArray<FDynamicMeshVertex>&& InVertices, TArray<uint32>&& InIndices)
	: FPrimitiveSceneProxy(InComponent, ArcWaterRiverSceneProxyLocal_7f31::GProxyResourceName)
	, VertexFactory(GetScene().GetFeatureLevel(), ArcWaterRiverSceneProxyLocal_7f31::GVertexFactoryDebugName)
	, Vertices(MoveTemp(InVertices))
{
	IndexBuffer.Indices = MoveTemp(InIndices);

	UMaterialInterface* const ComponentMaterial = InComponent->GetMaterial(0);
	const bool bValidMaterial = ComponentMaterial != nullptr
		&& ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_Water);

	UMaterialInterface* const EffectiveMaterial = bValidMaterial
		? ComponentMaterial
		: UMaterial::GetDefaultMaterial(MD_Surface);
	Material = EffectiveMaterial->GetRenderProxy();
	MaterialRelevance = EffectiveMaterial->GetRelevance_Concurrent(GetScene().GetShaderPlatform());
}

SIZE_T FArcWaterRiverSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FArcWaterRiverSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize()
		+ Vertices.GetAllocatedSize() + IndexBuffer.Indices.GetAllocatedSize();
}

void FArcWaterRiverSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	if (Vertices.IsEmpty() || IndexBuffer.Indices.IsEmpty())
	{
		return;
	}

	VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
	VertexBuffers.InitFromDynamicVertex(RHICmdList, &VertexFactory, Vertices, 2);

	IndexBuffer.InitResource(RHICmdList);

	Vertices.Empty();
}

void FArcWaterRiverSceneProxy::DestroyRenderThreadResources()
{
	IndexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
}

FPrimitiveViewRelevance FArcWaterRiverSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

void FArcWaterRiverSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const int32 NumPrimitives = IndexBuffer.Indices.Num() / 3;
	const uint32 NumVertices = VertexBuffers.PositionVertexBuffer.GetNumVertices();
	if (Material == nullptr || NumPrimitives == 0 || NumVertices == 0)
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if ((VisibilityMap & (1 << ViewIndex)) == 0)
		{
			continue;
		}

		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		Mesh.bUseWireframeSelectionColoring = IsSelected();
		Mesh.VertexFactory = &VertexFactory;
		Mesh.MaterialRenderProxy = Material;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = true;
		Mesh.bUseForMaterial = true;

		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer =
			Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
		FPrimitiveUniformShaderParametersBuilder Builder;
		BuildUniformShaderParameters(Builder);
		DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), Builder);

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = static_cast<uint32>(NumPrimitives);
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = NumVertices - 1;
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

		Collector.AddMesh(ViewIndex, Mesh);
	}
}
