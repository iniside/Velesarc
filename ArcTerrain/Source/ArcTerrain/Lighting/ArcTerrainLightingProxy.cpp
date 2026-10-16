// Copyright Lukasz Baran. All Rights Reserved.

#include "Lighting/ArcTerrainLightingProxy.h"

#include "Lighting/ArcTerrainLightingComponent.h"

#include "MaterialDomain.h"
#include "Math/ScaleMatrix.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshBatch.h"
#include "PrimitiveViewRelevance.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"

#if RHI_RAYTRACING
#include "RayTracingGeometryManagerInterface.h"
#endif

namespace ArcTerrainLightingProxyPrivate_3b91
{
	const FName GProxyResourceName(TEXT("ArcTerrainLighting"));
}

FArcTerrainLightingProxy::FArcTerrainLightingProxy(UArcTerrainLightingComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, ArcTerrainLightingProxyPrivate_3b91::GProxyResourceName)
	, VertexFactory(GetScene().GetFeatureLevel(), "FArcTerrainLightingProxy")
	, Mesh(InComponent->GetMesh())
	, NumVertices(static_cast<uint32>(InComponent->GetMesh().Positions.Num()))
	, NumPrimitives(static_cast<uint32>(InComponent->GetMesh().Indices.Num() / 3))
	, PackedHeightTexture(InComponent->GetPackedHeightTexture())
{
	UMaterialInterface* ComponentMaterial = InComponent->GetMaterial(0);
	Material = ComponentMaterial != nullptr
		? ComponentMaterial->GetRenderProxy()
		: UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetShaderPlatform());

	bSupportsHeightfieldRepresentation = PackedHeightTexture != nullptr;
	bAffectDistanceFieldLighting = true;
	bAffectDynamicIndirectLighting = true;

	QuadToLocal = ArcTerrainLightingMesh::HeightfieldQuadToLocal(InComponent->GetCellSize());
}

SIZE_T FArcTerrainLightingProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FArcTerrainLightingProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
}

void FArcTerrainLightingProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	check(Mesh.Normals.Num() == Mesh.Positions.Num());
	check(Mesh.Tangents.Num() == Mesh.Positions.Num());
	check(Mesh.Bitangents.Num() == Mesh.Positions.Num());
	check(Mesh.UV0.Num() == Mesh.Positions.Num());
	check(Mesh.UV1.Num() == Mesh.Positions.Num());
	check(Mesh.UV2.Num() == Mesh.Positions.Num());

	VertexBuffers.PositionVertexBuffer.Init(NumVertices);
	VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
	VertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, 3);

	for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const int32 Index = static_cast<int32>(VertexIndex);
		VertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex) = Mesh.Positions[Index];
		VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
			VertexIndex, Mesh.Tangents[Index], Mesh.Bitangents[Index], Mesh.Normals[Index]);
		VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexIndex, 0, Mesh.UV0[Index]);
		VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexIndex, 1, Mesh.UV1[Index]);
		VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexIndex, 2, Mesh.UV2[Index]);
	}

	IndexBuffer.Indices = Mesh.Indices;

	VertexBuffers.PositionVertexBuffer.InitResource(RHICmdList);
	VertexBuffers.StaticMeshVertexBuffer.InitResource(RHICmdList);
	IndexBuffer.InitResource(RHICmdList);

	FLocalVertexFactory::FDataType Data;
	VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&VertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&VertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&VertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(&VertexFactory, Data, 0);
	FColorVertexBuffer::BindDefaultColorVertexBuffer(
		&VertexFactory, Data, FColorVertexBuffer::NullBindStride::ZeroForDefaultBufferBind);
	VertexFactory.SetData(RHICmdList, Data);
	VertexFactory.InitResource(RHICmdList);

#if RHI_RAYTRACING
	if (IsRayTracingEnabled() && IndexBuffer.IndexBufferRHI.IsValid() && NumPrimitives > 0)
	{
		RayTracingGeometryGroupHandle = GRayTracingGeometryManager->RegisterRayTracingGeometryGroup(1, 0);

		FRayTracingGeometryInitializer Initializer;
		Initializer.DebugName = ArcTerrainLightingProxyPrivate_3b91::GProxyResourceName;
		Initializer.OwnerName = ArcTerrainLightingProxyPrivate_3b91::GProxyResourceName;
		Initializer.IndexBuffer = IndexBuffer.IndexBufferRHI;
		Initializer.IndexBufferStride = IndexBuffer.IndexBufferRHI->GetStride();
		Initializer.GeometryType = RTGT_Triangles;
		Initializer.TotalPrimitiveCount = NumPrimitives;
		Initializer.bAllowUpdate = false;
		Initializer.Type = ERayTracingGeometryInitializerType::Rendering;

		FRayTracingGeometrySegment Segment;
		Segment.VertexBuffer = VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
		Segment.VertexBufferElementType = VET_Float3;
		Segment.VertexBufferStride = VertexBuffers.PositionVertexBuffer.GetStride();
		Segment.MaxVertices = NumVertices;
		Segment.NumPrimitives = NumPrimitives;
		Initializer.Segments.Add(Segment);

		RayTracingGeometry.GroupHandle = RayTracingGeometryGroupHandle;
		RayTracingGeometry.LODIndex = 0;
		RayTracingGeometry.SetInitializer(MoveTemp(Initializer));
		RayTracingGeometry.InitResource(RHICmdList);
	}
#endif

	Mesh = ArcTerrainLightingMesh::FMesh();

	UpdateVisibleInLumenScene();
}

void FArcTerrainLightingProxy::DestroyRenderThreadResources()
{
#if RHI_RAYTRACING
	RayTracingGeometry.ReleaseResource();
	if (!RayTracingGeometryGroupHandle.IsNull() && !bReleasedRayTracingGeometryGroup)
	{
		bReleasedRayTracingGeometryGroup = true;
		GRayTracingGeometryManager->ReleaseRayTracingGeometryGroup(RayTracingGeometryGroupHandle);
	}
#endif

	VertexFactory.ReleaseResource();
	IndexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
}

void FArcTerrainLightingProxy::GetHeightfieldRepresentation(UTexture2D*& OutHeightmapTexture,
	UTexture2D*& OutVisibilityTexture, FHeightfieldComponentDescription& OutDescription) const
{
	OutHeightmapTexture = PackedHeightTexture;
	OutVisibilityTexture = nullptr;

	constexpr int32 Resolution = ArcTerrainLightingMesh::GHeightfieldResolution;
	constexpr int32 Quads = Resolution - 1;
	const float InvResolution = 1.0f / static_cast<float>(Resolution);

	OutDescription.LocalToWorld =
		FScaleMatrix(FVector(QuadToLocal, QuadToLocal, ArcTerrainLightingMesh::GHeightfieldScaleZ))
		* GetLocalToWorld();
	OutDescription.HeightfieldScaleBias = FVector4f(InvResolution, InvResolution, 0.0f, 0.0f);
	// Asymmetric by contract: the min corner takes no inset, the max corner one texel.
	OutDescription.MinMaxUV = FVector4f(
		0.0f, 0.0f, static_cast<float>(Quads) * InvResolution, static_cast<float>(Quads) * InvResolution);
	OutDescription.HeightfieldRect = FIntRect(0, 0, Quads, Quads);
	OutDescription.NumSubsections = 1;
	OutDescription.SubsectionScaleAndBias = FVector4(Quads, Quads, 0.0, 0.0);
	OutDescription.VisibilityChannel = -1;
}

FPrimitiveViewRelevance FArcTerrainLightingProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bDynamicRelevance = false;
	Result.bStaticRelevance = true;
	// Drops the base pass only: the component's bRenderInMainPass stays true, because RT hit shaders and
	// Lumen card capture both gate on Proxy->ShouldRenderInMainPass().
	Result.bRenderInMainPass = false;
	Result.bShadowRelevance = false;
	Result.bRenderCustomDepth = false;
	Result.bVelocityRelevance = false;
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

void FArcTerrainLightingProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	if (Material == nullptr || NumPrimitives == 0)
	{
		return;
	}

	FMeshBatch MeshBatch;
	MeshBatch.VertexFactory = &VertexFactory;
	MeshBatch.MaterialRenderProxy = Material;
	MeshBatch.SegmentIndex = 0;
	MeshBatch.LODIndex = 0;
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = SDPG_World;
	MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	MeshBatch.bCanApplyViewModeOverrides = false;
	MeshBatch.CastShadow = false;
	MeshBatch.bUseForDepthPass = false;
	MeshBatch.bUseAsOccluder = false;
	MeshBatch.bUseForMaterial = true;
	MeshBatch.bDitheredLODTransition = false;
	MeshBatch.bRenderToVirtualTexture = false;
	MeshBatch.bUseForLumenSurfaceCacheCapture = true;

	FMeshBatchElement& Element = MeshBatch.Elements[0];
	Element.IndexBuffer = &IndexBuffer;
	Element.FirstIndex = 0;
	Element.NumPrimitives = NumPrimitives;
	Element.MinVertexIndex = 0;
	Element.MaxVertexIndex = NumVertices > 0 ? NumVertices - 1 : 0;
	Element.PrimitiveUniformBuffer = GetUniformBuffer();

	PDI->DrawMesh(MeshBatch, FLT_MAX);
}

#if RHI_RAYTRACING
ERayTracingPrimitiveFlags FArcTerrainLightingProxy::GetRayTracingPrimitiveFlags()
{
	return ERayTracingPrimitiveFlags::CacheInstances;
}

void FArcTerrainLightingProxy::GetStaticRayTracingInstances(FStaticRayTracingInstances& OutRayTracingInstances)
{
	if (Material == nullptr || NumPrimitives == 0 || !RayTracingGeometry.IsValid() || RayTracingGeometry.IsEvicted())
	{
		return;
	}

	FStaticRayTracingInstancesLOD& LOD = OutRayTracingInstances.LODs.AddDefaulted_GetRef();
	FRayTracingInstance& Instance = LOD.Instances.AddDefaulted_GetRef();
	Instance.Geometry = &RayTracingGeometry;
	// Defaults to 0, which builds the BLAS and leaves the TLAS empty.
	Instance.NumTransforms = 1;

	FMeshBatch& Batch = Instance.Materials.AddDefaulted_GetRef();
	Batch.VertexFactory = &VertexFactory;
	Batch.MaterialRenderProxy = Material;
	Batch.SegmentIndex = 0;
	Batch.LODIndex = 0;
	Batch.Type = PT_TriangleList;
	Batch.DepthPriorityGroup = SDPG_World;
	Batch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Batch.bCanApplyViewModeOverrides = false;
	Batch.bUseForMaterial = true;
	Batch.CastShadow = true;
	Batch.CastRayTracedShadow = true;

	FMeshBatchElement& Element = Batch.Elements[0];
	Element.IndexBuffer = &IndexBuffer;
	Element.FirstIndex = 0;
	Element.NumPrimitives = NumPrimitives;
	Element.MinVertexIndex = 0;
	Element.MaxVertexIndex = NumVertices > 0 ? NumVertices - 1 : 0;
	Element.PrimitiveUniformBuffer = GetUniformBuffer();
}
#endif
