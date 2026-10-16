// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOceanSceneProxy.h"

#include "Ocean/ArcOceanFoamAccumulator.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshBatch.h"
#include "PrimitiveViewRelevance.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"

namespace ArcOceanSceneProxyLocal_5d21
{
	const FName GProxyResourceName(TEXT("ArcOcean"));

	constexpr float MinBaseCellSize = 10.0f;

	double SnapAxis(double WorldCoord, double SnapStep)
	{
		return FMath::FloorToDouble(WorldCoord / SnapStep) * SnapStep;
	}
}

FArcOceanSceneProxy::FArcOceanSceneProxy(const UArcOceanComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, ArcOceanSceneProxyLocal_5d21::GProxyResourceName)
	, RingGeometry(InComponent->GetVertsPerSide())
	, FullGridVertexFactory(GetScene().GetFeatureLevel(), &RingGeometry.GetFullGridMesh().VertexBuffer)
	, DonutVertexFactory(GetScene().GetFeatureLevel(), &RingGeometry.GetDonutMesh().VertexBuffer)
	, TrimVertexFactory(GetScene().GetFeatureLevel(), &RingGeometry.GetTrimMesh().VertexBuffer)
	, BaseCellSize(FMath::Max(InComponent->GetBaseCellSize(), ArcOceanSceneProxyLocal_5d21::MinBaseCellSize))
	, RingCount(FMath::Clamp(InComponent->GetRingCount(), 1, ArcOcean::MaxRings))
{
	bAlwaysHasVelocity = true;

	UMaterialInterface* ComponentMaterial = InComponent->GetMaterial(0);
	const bool bValidMaterial = ComponentMaterial != nullptr
		&& ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_Water);
	UMaterialInterface* const EffectiveMaterial = bValidMaterial
		? ComponentMaterial
		: UMaterial::GetDefaultMaterial(MD_Surface);
	Material = EffectiveMaterial->GetRenderProxy();
	MaterialRelevance = EffectiveMaterial->GetRelevance_Concurrent(GetScene().GetShaderPlatform());

	ArcOcean::FArcOceanWaveFields WaveFields;
	ArcOcean::PackWaveParams(InComponent->GetWaveSettings(), WaveFields);
	for (int32 SlotIndex = 0; SlotIndex < ArcOcean::MaxWaves * 2; ++SlotIndex)
	{
		WaveParams.Waves[SlotIndex] = WaveFields.Waves[SlotIndex];
	}
	WaveParams.WaveCount = WaveFields.WaveCount;
	WaveParams.DeepDepthCm = InComponent->GetDeepDepthCm();
}

SIZE_T FArcOceanSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FArcOceanSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
}

void FArcOceanSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	RingGeometry.InitResource(RHICmdList);

	FullGridVertexFactory.InitResource(RHICmdList);
	DonutVertexFactory.InitResource(RHICmdList);
	TrimVertexFactory.InitResource(RHICmdList);

	ArcOcean::SeedSeabedParams(WaveParams);
	ArcOcean::SeedFoamParams(WaveParams);
	PushParamsToFactories(RHICmdList);

	FArcOceanFoamAccumulator::Get().RegisterProxy(this);
}

void FArcOceanSceneProxy::DestroyRenderThreadResources()
{
	FArcOceanFoamAccumulator::Get().UnregisterProxy(this);

	TrimVertexFactory.ReleaseResource();
	DonutVertexFactory.ReleaseResource();
	FullGridVertexFactory.ReleaseResource();
	RingGeometry.ReleaseResource();
}

void FArcOceanSceneProxy::SetWaveFields_RenderThread(FRHICommandListBase& RHICmdList, const ArcOcean::FArcOceanWaveFields& InFields)
{
	for (int32 SlotIndex = 0; SlotIndex < ArcOcean::MaxWaves * 2; ++SlotIndex)
	{
		WaveParams.Waves[SlotIndex] = InFields.Waves[SlotIndex];
	}
	WaveParams.WaveCount = InFields.WaveCount;

	PushParamsToFactories(RHICmdList);
}

void FArcOceanSceneProxy::SetSeabedFields_RenderThread(FRHICommandListBase& RHICmdList, const FArcOceanSeabedFields& InFields)
{
	ArcOcean::ApplySeabedFields(WaveParams, InFields);

	PushParamsToFactories(RHICmdList);
}

void FArcOceanSceneProxy::SetFoamFields_RenderThread(FRHICommandListBase& RHICmdList, const FArcOceanFoamFields& InFields)
{
	ArcOcean::ApplyFoamFields(WaveParams, InFields);

	PushParamsToFactories(RHICmdList);
}

void FArcOceanSceneProxy::PushParamsToFactories(FRHICommandListBase& RHICmdList)
{
	FullGridVertexFactory.UpdateParams_RenderThread(RHICmdList, WaveParams);
	DonutVertexFactory.UpdateParams_RenderThread(RHICmdList, WaveParams);
	TrimVertexFactory.UpdateParams_RenderThread(RHICmdList, WaveParams);
}

FPrimitiveViewRelevance FArcOceanSceneProxy::GetViewRelevance(const FSceneView* View) const
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

void FArcOceanSceneProxy::AddRingBatch(FMeshElementCollector& Collector, int32 ViewIndex, const FSceneViewFamily& ViewFamily,
	const FArcOceanRingMesh& RingMesh, const FArcOceanVertexFactory& RingVertexFactory,
	float CellSize, const FVector2f& OriginTranslatedWorld, uint32 TrimRotationSteps) const
{
	if (RingMesh.NumPrimitives == 0)
	{
		return;
	}

	FMeshBatch& Mesh = Collector.AllocateMesh();
	Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	Mesh.bUseWireframeSelectionColoring = IsSelected();
	Mesh.VertexFactory = &RingVertexFactory;
	Mesh.MaterialRenderProxy = Material;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
	Mesh.bCanApplyViewModeOverrides = true;
	Mesh.bUseForMaterial = true;
	Mesh.CastShadow = false;
	Mesh.bUseForDepthPass = true;

	Mesh.Elements.SetNumZeroed(1);
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &RingMesh.IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = RingMesh.NumPrimitives;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = RingMesh.NumVertices - 1;
	BatchElement.PrimitiveIdMode = PrimID_ForceZero;
	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

	FArcOceanVFUserData& UserData = Collector.AllocateOneFrameResource<FArcOceanVFUserData>();
	UserData.SnappedOriginTranslatedWorld = OriginTranslatedWorld;
	UserData.CellSize = CellSize;
	UserData.TrimRotationSteps = TrimRotationSteps;
	BatchElement.UserData = &UserData;

	Collector.AddMesh(ViewIndex, Mesh);
}

void FArcOceanSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	using namespace ArcOceanSceneProxyLocal_5d21;

	if (Material == nullptr || ViewFamily.Views.IsEmpty() || ViewFamily.Views[0] == nullptr)
	{
		return;
	}

	// Shadow-depth and capture views carry their own ViewOrigin, so a per-view snap would tessellate the depth pass differently from the base pass.
	const FSceneView& PrimaryView = *ViewFamily.Views[0];
	const FVector ViewOriginWorld = PrimaryView.ViewMatrices.GetViewOrigin();
	const FVector PreViewTranslation = PrimaryView.ViewMatrices.GetPreViewTranslation();

	FVector2D RingOriginWorld[ArcOcean::MaxRings];
	FVector2f RingOriginTranslatedWorld[ArcOcean::MaxRings];
	float RingCellSize[ArcOcean::MaxRings];

	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		const double CellSize = static_cast<double>(BaseCellSize) * static_cast<double>(1 << RingIndex);
		const double SnapStep = 2.0 * CellSize;

		RingCellSize[RingIndex] = static_cast<float>(CellSize);
		RingOriginWorld[RingIndex] = FVector2D(
			SnapAxis(ViewOriginWorld.X, SnapStep),
			SnapAxis(ViewOriginWorld.Y, SnapStep));
		RingOriginTranslatedWorld[RingIndex] = FVector2f(
			static_cast<float>(RingOriginWorld[RingIndex].X + PreViewTranslation.X),
			static_cast<float>(RingOriginWorld[RingIndex].Y + PreViewTranslation.Y));
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if ((VisibilityMap & (1 << ViewIndex)) == 0 || Views[ViewIndex] == nullptr)
		{
			continue;
		}

		AddRingBatch(Collector, ViewIndex, ViewFamily, RingGeometry.GetFullGridMesh(), FullGridVertexFactory,
			RingCellSize[0], RingOriginTranslatedWorld[0], 0u);

		for (int32 RingIndex = 1; RingIndex < RingCount; ++RingIndex)
		{
			AddRingBatch(Collector, ViewIndex, ViewFamily, RingGeometry.GetDonutMesh(), DonutVertexFactory,
				RingCellSize[RingIndex], RingOriginTranslatedWorld[RingIndex], 0u);

			const bool bOffsetX = RingOriginWorld[RingIndex - 1].X != RingOriginWorld[RingIndex].X;
			const bool bOffsetY = RingOriginWorld[RingIndex - 1].Y != RingOriginWorld[RingIndex].Y;
			const uint32 TrimRotationSteps =
				static_cast<uint32>(FArcOceanRingGeometry::GetTrimRotationSteps(bOffsetX, bOffsetY));

			AddRingBatch(Collector, ViewIndex, ViewFamily, RingGeometry.GetTrimMesh(), TrimVertexFactory,
				RingCellSize[RingIndex], RingOriginTranslatedWorld[RingIndex], TrimRotationSteps);
		}
	}
}

bool FArcOceanSceneProxy::CanBeOccluded() const
{
	return false;
}
