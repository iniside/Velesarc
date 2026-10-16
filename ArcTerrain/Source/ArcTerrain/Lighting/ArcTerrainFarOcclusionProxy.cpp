// Copyright Lukasz Baran. All Rights Reserved.

#include "Lighting/ArcTerrainFarOcclusionProxy.h"

#include "Lighting/ArcTerrainFarOcclusionComponent.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
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

namespace ArcTerrainFarOcclusionProxyPrivate_8c04
{
	const FName GProxyResourceName(TEXT("ArcTerrainFarOcclusion"));
}

FArcTerrainFarOcclusionProxy::FArcTerrainFarOcclusionProxy(UArcTerrainFarOcclusionComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, ArcTerrainFarOcclusionProxyPrivate_8c04::GProxyResourceName)
	, VertexFactory(GetScene().GetFeatureLevel(), "FArcTerrainFarOcclusionProxy")
	, Mesh(InComponent->GetMesh())
	, NumVertices(static_cast<uint32>(InComponent->GetMesh().Positions.Num()))
	, NumPrimitives(static_cast<uint32>(InComponent->GetMesh().Indices.Num() / 3))
{
	Material = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

	bAffectDynamicIndirectLighting = false;
}

SIZE_T FArcTerrainFarOcclusionProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FArcTerrainFarOcclusionProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
}

void FArcTerrainFarOcclusionProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
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
		Initializer.DebugName = ArcTerrainFarOcclusionProxyPrivate_8c04::GProxyResourceName;
		Initializer.OwnerName = ArcTerrainFarOcclusionProxyPrivate_8c04::GProxyResourceName;
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

void FArcTerrainFarOcclusionProxy::DestroyRenderThreadResources()
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

FPrimitiveViewRelevance FArcTerrainFarOcclusionProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = false;
	Result.bRenderInMainPass = false;
	Result.bStaticRelevance = false;
	Result.bDynamicRelevance = false;
	Result.bShadowRelevance = false;
	Result.bRenderCustomDepth = false;
	Result.bVelocityRelevance = false;
	return Result;
}

#if RHI_RAYTRACING
ERayTracingPrimitiveFlags FArcTerrainFarOcclusionProxy::GetRayTracingPrimitiveFlags()
{
	return ERayTracingPrimitiveFlags::CacheInstances | ERayTracingPrimitiveFlags::FarField;
}

void FArcTerrainFarOcclusionProxy::GetStaticRayTracingInstances(FStaticRayTracingInstances& OutRayTracingInstances)
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
	Batch.CastShadow = false;
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
