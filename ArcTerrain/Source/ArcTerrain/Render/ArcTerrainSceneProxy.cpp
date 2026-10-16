// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainSceneProxy.h"

#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainModule.h"
#include "Render/ArcTerrainCBTBuffers.h"
#include "Render/ArcTerrainCBTRenderer.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshBatch.h"
#include "PrimitiveViewRelevance.h"
#include "RHIResourceUtils.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "ShaderCore.h" // AllowDebugViewmodes

DECLARE_CYCLE_STAT(TEXT("RT: proxy gather"), STAT_ArcTerrainProxyGather, STATGROUP_ArcTerrain);

namespace ArcTerrainSceneProxyPrivate_7c04
{
	const FName GProxyResourceName(TEXT("ArcTerrain"));
	std::atomic<uint64> GNextProxySerial { 1 };

	uint64 AllocateProxySerial()
	{
		const uint64 ProxySerial = GNextProxySerial.fetch_add(1, std::memory_order_relaxed);
		check(ProxySerial != 0);
		return ProxySerial;
	}

	/** Face record layout must match the Buffer<uint4> the vertex factory indexes. */
	void PackFaceParams(const TArray<FArcTerrainRenderFace>& Faces, TArray<uint32>& OutPacked)
	{
		OutPacked.SetNumZeroed(FMath::Max(Faces.Num(), 1) * 4);
		for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); ++FaceIndex)
		{
			const FArcTerrainRenderFace& Face = Faces[FaceIndex];
			OutPacked[FaceIndex * 4 + 0] = static_cast<uint32>(Face.TexelOrigin.X);
			OutPacked[FaceIndex * 4 + 1] = static_cast<uint32>(Face.TexelOrigin.Y);
		}
	}

	/** A ring's window, or a single zero when it is empty — the buffer has to exist for the binding even
	 *  though a negative radius means nothing ever reads it. */
	void CreateRingSRV(FRHICommandListBase& RHICmdList, const TArray<uint32>& Slices, const TCHAR* Name,
		FBufferRHIRef& OutBuffer, FShaderResourceViewRHIRef& OutSRV)
	{
		OutSRV.SafeRelease();
		OutBuffer.SafeRelease();

		const uint32 Empty[1] = { 0u };
		OutBuffer = UE::RHIResourceUtils::CreateVertexBufferFromArray(
			RHICmdList, Name, EBufferUsageFlags::ShaderResource,
			Slices.IsEmpty() ? MakeConstArrayView(Empty, 1) : MakeConstArrayView(Slices));
		OutSRV = RHICmdList.CreateShaderResourceView(
			OutBuffer,
			FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Typed).SetFormat(PF_R32_UINT));
	}
}

FArcTerrainSceneProxy::FArcTerrainSceneProxy(UArcTerrainRenderComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, ArcTerrainSceneProxyPrivate_7c04::GProxyResourceName)
	, ProxySerial(ArcTerrainSceneProxyPrivate_7c04::AllocateProxySerial())
	, DrawSlot0(GetScene().GetFeatureLevel(), 0)
	, DrawSlot1(GetScene().GetFeatureLevel(), 1)
	, PendingBindings(InComponent->GetBindings())
{
	// Material usage gate (VHM proxy pattern): CheckMaterialUsage_Concurrent auto-sets
	// bUsedWithVirtualHeightfieldMesh in the editor — the permutation gate
	// FArcTerrainVertexFactory::ShouldCompilePermutation keys on it. An unusable material falls back to
	// the default material, which still renders correctly displaced terrain (displacement lives in the
	// vertex factory, not the material).
	UMaterialInterface* ComponentMaterial = InComponent->GetMaterial(0);
	const bool bValidMaterial = ComponentMaterial != nullptr
		&& ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_VirtualHeightfieldMesh);
	Material = bValidMaterial
		? ComponentMaterial->GetRenderProxy()
		: UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetShaderPlatform());

	const UMaterialInterface* const ProxyMaterialInterface = Material->GetMaterialInterface();
	UE_LOGF(LogArcTerrain, Warning,"[ProxyMaterial] component material[0]='%ls' ptr=0x%p usagevalid=%ls -> proxy draws with '%ls'%ls",
		ComponentMaterial != nullptr ? *ComponentMaterial->GetName() : TEXT("<null>"),
		static_cast<const void*>(ComponentMaterial), bValidMaterial ? TEXT("yes") : TEXT("NO"),
		ProxyMaterialInterface != nullptr ? *ProxyMaterialInterface->GetName() : TEXT("<null>"),
		bValidMaterial ? TEXT("")
			: TEXT("  <<<< the proxy fell back to the DEFAULT surface material: the terrain material and every ")
				TEXT("atlas parameter bound on it are not used by this draw at all"));

	// The bisector pool's per-frame update and its main-view capture hang off this registration. It
	// used to live on the per-cell proxy; without it here MainView.bValid never becomes true and the
	// tree silently freezes at its seed triangulation.
	GArcTerrainCBTRendererExtension.RegisterExtension();

	// The gather reads the pool's render-thread view state. Parallel GDME would run it on worker tasks
	// (SceneVisibility.cpp LaunchAsyncTask) while the pool's own delegate writes that state, and it costs
	// nothing to refuse: this proxy fills one FMeshBatch per view and publishes its million triangles
	// through indirect args, not through the collector.
	bSupportsParallelGDME = false;
}

SIZE_T FArcTerrainSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FArcTerrainSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
}

void FArcTerrainSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	DrawSlot0.VertexFactory.InitResource(RHICmdList);
	DrawSlot1.VertexFactory.InitResource(RHICmdList);
	bRenderResourcesCreated = true;
	ApplyBindings_RenderThread(RHICmdList, MoveTemp(PendingBindings));
}

void FArcTerrainSceneProxy::DestroyRenderThreadResources()
{
	InvalidateAllSlots_RenderThread();
	DrawSlot1.VertexFactory.ReleaseResource();
	DrawSlot0.VertexFactory.ReleaseResource();
	bRenderResourcesCreated = false;
}

void FArcTerrainSceneProxy::UpdateBindings_RenderThread(FRHICommandListBase& RHICmdList,
	FArcTerrainRenderBindings&& InBindings)
{
	if (!bRenderResourcesCreated)
	{
		PendingBindings = MoveTemp(InBindings);
		return;
	}
	ApplyBindings_RenderThread(RHICmdList, MoveTemp(InBindings));
}

FArcTerrainCBTBindingSourceToken FArcTerrainSceneProxy::GetSourceToken(const FDrawSlot& Slot) const
{
	FArcTerrainCBTBindingSourceToken SourceToken;
	SourceToken.ProxySerial = ProxySerial;
	SourceToken.SlotIndex = Slot.SlotIndex;
	return SourceToken;
}

FArcTerrainSceneProxy::FDrawSlot* FArcTerrainSceneProxy::FindSlot_RenderThread(
	const FArcTerrainRenderIdentity& Identity)
{
	if (DrawSlot0.bValid && DrawSlot0.Bindings.Identity == Identity)
	{
		return &DrawSlot0;
	}
	if (DrawSlot1.bValid && DrawSlot1.Bindings.Identity == Identity)
	{
		return &DrawSlot1;
	}
	return nullptr;
}

const FArcTerrainSceneProxy::FDrawSlot* FArcTerrainSceneProxy::FindSlot_RenderThread(
	const FArcTerrainRenderIdentity& Identity) const
{
	if (DrawSlot0.bValid && DrawSlot0.Bindings.Identity == Identity)
	{
		return &DrawSlot0;
	}
	if (DrawSlot1.bValid && DrawSlot1.Bindings.Identity == Identity)
	{
		return &DrawSlot1;
	}
	return nullptr;
}

void FArcTerrainSceneProxy::InvalidateSlot_RenderThread(FDrawSlot& Slot)
{
	if (Slot.bBindingSourceRegistered)
	{
		GArcTerrainCBTRendererExtension.UnregisterBindingSource_RenderThread(
			Slot.Bindings.Identity, GetSourceToken(Slot));
	}
	Slot.bBindingSourceRegistered = false;
	Slot.bValid = false;
	Slot.WeightRingSRV.SafeRelease();
	Slot.WeightRingBuffer.SafeRelease();
	Slot.HeightRingSRV.SafeRelease();
	Slot.HeightRingBuffer.SafeRelease();
	Slot.FaceParamsSRV.SafeRelease();
	Slot.FaceParamsBuffer.SafeRelease();
	Slot.Bindings = FArcTerrainRenderBindings();
}

void FArcTerrainSceneProxy::InvalidateAllSlots_RenderThread()
{
	InvalidateSlot_RenderThread(DrawSlot0);
	InvalidateSlot_RenderThread(DrawSlot1);
}

void FArcTerrainSceneProxy::ApplyBindings_RenderThread(FRHICommandListBase& RHICmdList,
	FArcTerrainRenderBindings&& InBindings)
{
	const uint64 CurrentRhiEpoch = GArcTerrainCBTRendererExtension.GetRhiEpoch();
	FDrawSlot* Slots[2] = { &DrawSlot0, &DrawSlot1 };
	for (FDrawSlot* Slot : Slots)
	{
		if ((Slot->bValid || Slot->bBindingSourceRegistered)
			&& Slot->Bindings.Identity.RhiEpoch != CurrentRhiEpoch)
		{
			InvalidateSlot_RenderThread(*Slot);
		}
	}

	if (!InBindings.bResourcesValid)
	{
		InvalidateAllSlots_RenderThread();
		return;
	}

	FDrawSlot* ExistingSlot = FindSlot_RenderThread(InBindings.Identity);
	if (ExistingSlot != nullptr)
	{
		if (!ExistingSlot->bBindingSourceRegistered)
		{
			ExistingSlot->bBindingSourceRegistered =
				GArcTerrainCBTRendererExtension.RegisterBindingSource_RenderThread(
					ExistingSlot->Bindings.Identity, GetSourceToken(*ExistingSlot));
		}
		return;
	}

	const FArcTerrainRenderIdentity& AcceptedIdentity =
		GArcTerrainCBTRendererExtension.GetAcceptedIdentity_RenderThread();
	const FDrawSlot* ProtectedSlot = FindSlot_RenderThread(AcceptedIdentity);
	FDrawSlot* TargetSlot = nullptr;
	if (!DrawSlot0.bValid && ProtectedSlot != &DrawSlot0)
	{
		TargetSlot = &DrawSlot0;
	}
	else if (!DrawSlot1.bValid && ProtectedSlot != &DrawSlot1)
	{
		TargetSlot = &DrawSlot1;
	}
	else
	{
		TargetSlot = ProtectedSlot == &DrawSlot0 ? &DrawSlot1 : &DrawSlot0;
	}

	InvalidateSlot_RenderThread(*TargetSlot);
	BuildSlot_RenderThread(RHICmdList, *TargetSlot, MoveTemp(InBindings));
}

bool FArcTerrainSceneProxy::BuildSlot_RenderThread(FRHICommandListBase& RHICmdList, FDrawSlot& Slot,
	FArcTerrainRenderBindings&& InBindings)
{
	using namespace ArcTerrainSceneProxyPrivate_7c04;

	Slot.Bindings = MoveTemp(InBindings);
	FArcTerrainRenderBindings& Bindings = Slot.Bindings;
	if (!Bindings.IsValid() || !Bindings.bResourcesValid
		|| Bindings.Identity.RhiEpoch != GArcTerrainCBTRendererExtension.GetRhiEpoch()
		|| !Slot.VertexFactory.IsInitialized() || !Bindings.AtlasResources->HeightAtlas.IsValid()
		|| Bindings.AtlasResources->HeightAtlas->GetRHI() == nullptr)
	{
		InvalidateSlot_RenderThread(Slot);
		return false;
	}

	TArray<uint32> PackedFaces;
	PackFaceParams(Bindings.Faces, PackedFaces);

	Slot.FaceParamsBuffer = UE::RHIResourceUtils::CreateVertexBufferFromArray(
		RHICmdList, TEXT("ArcTerrain.FaceParams"), EBufferUsageFlags::ShaderResource,
		MakeConstArrayView(PackedFaces));
	Slot.FaceParamsSRV = RHICmdList.CreateShaderResourceView(
		Slot.FaceParamsBuffer,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Typed).SetFormat(PF_R32G32B32A32_UINT));

	TArray<uint32> HeightRingSlices;
	ArcTerrainAtlas::FArcTerrainHeightRingScalars HeightRingScalars;
	ArcTerrainAtlas::PackHeightRings(Bindings.Rings.HeightTiers, HeightRingSlices, HeightRingScalars);

	CreateRingSRV(RHICmdList, HeightRingSlices, TEXT("ArcTerrain.HeightRing"),
		Slot.HeightRingBuffer, Slot.HeightRingSRV);

	TArray<uint32> WeightRingSlices;
	ArcTerrainAtlas::FArcTerrainWeightRingScalars WeightRingScalars;
	ArcTerrainAtlas::PackWeightRings(Bindings.Rings.Weight, WeightRingSlices, WeightRingScalars);

	CreateRingSRV(RHICmdList, WeightRingSlices, TEXT("ArcTerrain.WeightRing"),
		Slot.WeightRingBuffer, Slot.WeightRingSRV);

	FArcTerrainVFParameters Params;
	if (Bindings.AtlasResources.IsValid() && Bindings.AtlasResources->HeightAtlas.IsValid())
	{
		Params.HeightAtlas = Bindings.AtlasResources->HeightAtlas->GetRHI();
	}
	Params.SectorQuads = static_cast<uint32>(FMath::Max(Bindings.SectorQuads, 1));
	Params.FaceQuads = static_cast<uint32>(FMath::Max(Bindings.FaceQuads, 1));
	Params.SectorQuadsShift = FMath::Max(Bindings.SectorQuadsShift, 0);
	Params.WeightQuadsShift = FMath::Max(Bindings.WeightQuadsShift, 0);
	Params.TexelToLocal = Bindings.TexelToLocal;
	Params.InvTileResolution = 1.0f / static_cast<float>(FMath::Max(Bindings.SectorQuads, 1) + 3);
	Params.HeightRingOffset = HeightRingScalars.Offset;
	Params.HeightRingRadius = HeightRingScalars.Radius;
	Params.HeightRingCenterX = HeightRingScalars.CenterX;
	Params.HeightRingCenterY = HeightRingScalars.CenterY;
	Params.HeightCumulativeShift = HeightRingScalars.CumulativeShift;
	Params.NumHeightTiers = HeightRingScalars.NumTiers;
	Params.HeightFillSlice = HeightRingScalars.FillSlice;
	Params.WeightRingOffset = WeightRingScalars.Offset;
	Params.WeightRingRadius = WeightRingScalars.Radius;
	Params.WeightRingCenterX = WeightRingScalars.CenterX;
	Params.WeightRingCenterY = WeightRingScalars.CenterY;
	Params.WeightCumulativeShift = WeightRingScalars.CumulativeShift;
	Params.NumWeightTiers = WeightRingScalars.NumTiers;
	Params.WeightFillSlice = WeightRingScalars.FillSlice;
	Params.bResourcesValid = Bindings.bResourcesValid ? 1u : 0u;
	if (!Slot.VertexFactory.SetTerrainParams_RenderThread(RHICmdList, Params))
	{
		InvalidateSlot_RenderThread(Slot);
		return false;
	}

	if (!Slot.FaceParamsBuffer.IsValid() || !Slot.FaceParamsSRV.IsValid()
		|| !Slot.HeightRingBuffer.IsValid() || !Slot.HeightRingSRV.IsValid()
		|| !Slot.WeightRingBuffer.IsValid() || !Slot.WeightRingSRV.IsValid()
		|| Bindings.Identity.RhiEpoch != GArcTerrainCBTRendererExtension.GetRhiEpoch())
	{
		InvalidateSlot_RenderThread(Slot);
		return false;
	}

	Slot.bValid = true;
	Slot.bBindingSourceRegistered = GArcTerrainCBTRendererExtension.RegisterBindingSource_RenderThread(
		Bindings.Identity, GetSourceToken(Slot));

	const bool bFirstApply = !bLoggedWeightUniformsOnce;
	bLoggedWeightUniformsOnce = true;
	if (!Bindings.bLogWeightVerbose && !bFirstApply)
	{
		return true;
	}

	const FArcTerrainResidencyRing EmptyWeightRing;
	const FArcTerrainResidencyRing& WeightRing =
		Bindings.Rings.Weight.IsValidIndex(0) ? Bindings.Rings.Weight[0] : EmptyWeightRing;

	FLoggedWeightUniforms Current;
	Current.RingCenter = FIntPoint(WeightRingScalars.CenterX[0], WeightRingScalars.CenterY[0]);
	Current.RingRadius = WeightRingScalars.Radius[0];
	Current.RingSliceCount = WeightRing.Slices.Num();
	Current.NumTiers = WeightRingScalars.NumTiers;
	Current.FillSlice = WeightRingScalars.FillSlice;
	Current.LayoutFillSlice = Bindings.LayoutWeightFillSlice;
	Current.WeightQuadsShift = Params.WeightQuadsShift;
	Current.SectorQuads = Params.SectorQuads;
	Current.SectorQuadsShift = Params.SectorQuadsShift;
	Current.bResourcesValid = Params.bResourcesValid;
	if (!bFirstApply && Current == LastLoggedWeightUniforms)
	{
		return true;
	}
	LastLoggedWeightUniforms = Current;

	const int32 ExpectedSlices = Current.RingRadius >= 0
		? (2 * Current.RingRadius + 1) * (2 * Current.RingRadius + 1)
		: 0;
	UE_LOGF(LogArcTerrain, Warning,
		"[WeightUniforms] first=%ls numweighttiers=%u | tier0: WeightRingCenter=(%d,%d) "
		"WeightRingRadius=%d%ls ringslices=%d expectedslices=%d%ls | WeightFillSlice=%u "
		"layoutweightfillslice=%d%ls | WeightQuadsShift=%d "
		"SectorQuads=%u SectorQuadsShift=%d resourcesvalid=%u%ls%ls",
		bFirstApply ? TEXT("yes") : TEXT("no"), Current.NumTiers,
		Current.RingCenter.X, Current.RingCenter.Y, Current.RingRadius,
		WeightRing.Slices.IsEmpty() ? TEXT(" (FORCED -1: the weight ring has no slices)") : TEXT(""),
		Current.RingSliceCount, ExpectedSlices,
		ExpectedSlices == Current.RingSliceCount ? TEXT("") : TEXT(" RADIUS/SLICECOUNT MISMATCH"),
		Current.FillSlice, Bindings.LayoutWeightFillSlice,
		static_cast<int32>(Current.FillSlice) == Bindings.LayoutWeightFillSlice
			? TEXT("") : TEXT(" FILLSLICE MISMATCH"),
		Params.WeightQuadsShift, Params.SectorQuads, Params.SectorQuadsShift, Params.bResourcesValid,
		Params.bResourcesValid != 0u ? TEXT("")
			: TEXT("  <<<< the vertex factory is parked on invalid resources: it collapses every triangle"),
		Current.RingSliceCount > 0 ? TEXT("")
			: TEXT("  <<<< the weight ring is EMPTY: every leaf resolves the fill slice"));
	return true;
}

FPrimitiveViewRelevance FArcTerrainSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	// Dynamic-only: the batch is rebuilt every frame, so no mesh draw commands are ever cached.
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

void FArcTerrainSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_ArcTerrainProxyGather);

	const FArcTerrainRenderIdentity& AcceptedIdentity =
		GArcTerrainCBTRendererExtension.GetAcceptedIdentity_RenderThread();
	const TFunction<void(const TCHAR*)> RejectDraw = [this, &AcceptedIdentity](const TCHAR* Reason)
	{
		GArcTerrainCBTRendererExtension.RecordProxyDiagnostic_RenderThread(false);
		const FArcTerrainRenderIdentity& Slot0Identity = DrawSlot0.Bindings.Identity;
		const FArcTerrainRenderIdentity& Slot1Identity = DrawSlot1.Bindings.Identity;
		UE_LOGF(LogArcTerrain, Warning,
			"[CBTOwner] proxy draw rejected proxy=%llu reason=%ls acceptedvalid=%ls acceptedtoken=%llu acceptedterrain=%llu acceptedepoch=%llu acceptedrevision=%llu acceptedrhi=%llu acceptedworldtype=%d acceptedscene=%p "
			"slot0valid=%ls slot0registered=%ls slot0token=%llu slot0terrain=%llu slot0epoch=%llu slot0revision=%llu slot0rhi=%llu slot0worldtype=%d slot0scene=%p "
			"slot1valid=%ls slot1registered=%ls slot1token=%llu slot1terrain=%llu slot1epoch=%llu slot1revision=%llu slot1rhi=%llu slot1worldtype=%d slot1scene=%p",
			ProxySerial, Reason, AcceptedIdentity.IsValid() ? TEXT("yes") : TEXT("no"),
			AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
			AcceptedIdentity.Owner.OwnerEpoch, AcceptedIdentity.ResourceRevision, AcceptedIdentity.RhiEpoch,
			static_cast<int32>(AcceptedIdentity.WorldType), AcceptedIdentity.SceneKey,
			DrawSlot0.bValid ? TEXT("yes") : TEXT("no"),
			DrawSlot0.bBindingSourceRegistered ? TEXT("yes") : TEXT("no"),
			Slot0Identity.Owner.Key.WorldInstanceToken, Slot0Identity.Owner.Key.TerrainInstanceId,
			Slot0Identity.Owner.OwnerEpoch, Slot0Identity.ResourceRevision, Slot0Identity.RhiEpoch,
			static_cast<int32>(Slot0Identity.WorldType), Slot0Identity.SceneKey,
			DrawSlot1.bValid ? TEXT("yes") : TEXT("no"),
			DrawSlot1.bBindingSourceRegistered ? TEXT("yes") : TEXT("no"),
			Slot1Identity.Owner.Key.WorldInstanceToken, Slot1Identity.Owner.Key.TerrainInstanceId,
			Slot1Identity.Owner.OwnerEpoch, Slot1Identity.ResourceRevision, Slot1Identity.RhiEpoch,
			static_cast<int32>(Slot1Identity.WorldType), Slot1Identity.SceneKey);
	};
	if (!AcceptedIdentity.IsValid())
	{
		RejectDraw(TEXT("accepted-identity-invalid"));
		return;
	}
	const FDrawSlot* DrawSlot = FindSlot_RenderThread(AcceptedIdentity);
	if (DrawSlot == nullptr)
	{
		RejectDraw(TEXT("no-exact-fixed-slot"));
		return;
	}
	if (!DrawSlot->Bindings.bResourcesValid)
	{
		RejectDraw(TEXT("slot-resources-invalid"));
		return;
	}
	if (!DrawSlot->VertexFactory.IsInitialized())
	{
		RejectDraw(TEXT("slot-vertex-factory-uninitialized"));
		return;
	}
	if (DrawSlot->Bindings.Identity.SceneKey != ViewFamily.Scene)
	{
		RejectDraw(TEXT("view-family-scene-mismatch"));
		return;
	}
	if (!GArcTerrainCBTRendererExtension.IsDrawPacketAccepted_RenderThread(
		DrawSlot->Bindings.Identity, ViewFamily.Scene))
	{
		RejectDraw(TEXT("renderer-rejected-exact-slot"));
		return;
	}
	const FArcTerrainRenderBindings& Bindings = DrawSlot->Bindings;

	const FArcTerrainCBTBuffers& Buffers = GArcTerrainCBTRendererExtension.GetBuffers();
	if (!Buffers.IsInitialized())
	{
		RejectDraw(TEXT("cbt-buffers-uninitialized"));
		return;
	}
	if (Bindings.Faces.IsEmpty())
	{
		RejectDraw(TEXT("packet-faces-empty"));
		return;
	}

	FRHIShaderResourceView* const HeapSRV = Buffers.GetHeapSRV();
	FRHIShaderResourceView* const BisectorSRV = Buffers.GetBisectorSRV();
	FRHIBuffer* const DrawArgsBuffer = Buffers.GetDrawArgsBuffer();
	if (HeapSRV == nullptr || BisectorSRV == nullptr || DrawArgsBuffer == nullptr
		|| !DrawSlot->FaceParamsSRV.IsValid() || !DrawSlot->HeightRingSRV.IsValid()
		|| !DrawSlot->WeightRingSRV.IsValid())
	{
		RejectDraw(TEXT("draw-rhi-resource-missing"));
		return;
	}
	const FArcTerrainCBTBindingSourceToken SelectedSource = GetSourceToken(*DrawSlot);
	UE_LOGF(LogArcTerrain, Verbose,"[CBTOwner] proxy draw selected proxy=%llu slot=%u registered=%ls packet token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p; accepted token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p",
		SelectedSource.ProxySerial, static_cast<uint32>(SelectedSource.SlotIndex),
		DrawSlot->bBindingSourceRegistered ? TEXT("yes") : TEXT("no"),
		Bindings.Identity.Owner.Key.WorldInstanceToken, Bindings.Identity.Owner.Key.TerrainInstanceId,
		Bindings.Identity.Owner.OwnerEpoch, Bindings.Identity.ResourceRevision, Bindings.Identity.RhiEpoch,
		static_cast<int32>(Bindings.Identity.WorldType), Bindings.Identity.SceneKey,
		AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
		AcceptedIdentity.Owner.OwnerEpoch, AcceptedIdentity.ResourceRevision, AcceptedIdentity.RhiEpoch,
		static_cast<int32>(AcceptedIdentity.WorldType), AcceptedIdentity.SceneKey);
	GArcTerrainCBTRendererExtension.RecordProxyDiagnostic_RenderThread(true);

	FRHIShaderResourceView* const VisibleBisectorSRV = Buffers.GetVisibleBisectorSRV();

	int32 NumBatches = 0;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if ((VisibilityMap & (1 << ViewIndex)) == 0 || Views[ViewIndex] == nullptr)
		{
			continue;
		}

		const bool bCulledView = VisibleBisectorSRV != nullptr
			&& GArcTerrainCBTRendererExtension.IsCulledView_RenderThread(*Views[ViewIndex]);

		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		Mesh.bUseWireframeSelectionColoring = IsSelected();
		Mesh.VertexFactory = &DrawSlot->VertexFactory;
		Mesh.MaterialRenderProxy = Material;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = true;
		Mesh.bUseForMaterial = true;
		Mesh.CastShadow = true;
		Mesh.bUseForDepthPass = true;

		Mesh.Elements.SetNumZeroed(1);
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		// NumPrimitives = 0 is the RHI's "read the counts from the indirect args" contract.
		BatchElement.IndexBuffer = DrawSlot->VertexFactory.GetCBTRefIndexBuffer();
		BatchElement.IndirectArgsBuffer = DrawArgsBuffer;
		BatchElement.IndirectArgsOffset =
			bCulledView ? FArcTerrainCBTBuffers::VisibleDrawArgsOffsetBytes : 0u;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 0;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = 0;
		BatchElement.PrimitiveIdMode = PrimID_ForceZero;
		BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

		FArcTerrainVFUserData& UserData = Collector.AllocateOneFrameResource<FArcTerrainVFUserData>();
		UserData.CBTHeapSRV = HeapSRV;
		UserData.CBTBisectorSRV = BisectorSRV;
		UserData.VisibleBisectorSRV = VisibleBisectorSRV;
		UserData.FaceParamsSRV = DrawSlot->FaceParamsSRV.GetReference();
		UserData.HeightRingSRV = DrawSlot->HeightRingSRV.GetReference();
		UserData.WeightRingSRV = DrawSlot->WeightRingSRV.GetReference();
		UserData.PoolDepth = static_cast<uint32>(Buffers.GetPoolDepth());
		UserData.NumFaces = static_cast<uint32>(Bindings.Faces.Num());
		UserData.bUseVisibleList = bCulledView ? 1u : 0u;
		BatchElement.UserData = &UserData;

		Collector.AddMesh(ViewIndex, Mesh);
		++NumBatches;
	}

	const uint64 AcceptedOwnerEpoch = AcceptedIdentity.Owner.OwnerEpoch;
	uint64 LoggedOwnerEpoch = FirstDrawOwnerEpoch.load(std::memory_order_relaxed);
	if (NumBatches > 0 && AcceptedOwnerEpoch != 0 && LoggedOwnerEpoch != AcceptedOwnerEpoch
		&& FirstDrawOwnerEpoch.compare_exchange_strong(LoggedOwnerEpoch, AcceptedOwnerEpoch,
			std::memory_order_relaxed, std::memory_order_relaxed))
	{
		uint64 ProxyAccepted = 0;
		uint64 ProxyRejected = 0;
		GArcTerrainCBTRendererExtension.GetProxyDiagnosticCounts_RenderThread(ProxyAccepted, ProxyRejected);
		// Unstamped ground sits at BaseHeightCm (200 m down by default), so "I see nothing" is usually
		// "the camera is above it". Sane bounds plus batches > 0 means the geometry exists and the
		// question is where the camera is; arc.Terrain.CBT.Stats 1 then gives the live triangle count.
		UE_LOGF(LogArcTerrain, Log,
			"[Render] first proxy terrain draw: %d static faces, %d mesh batches, pool depth %d, proxy=%llu slot=%u, "
			"packet token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p; accepted token=%llu terrain=%llu epoch=%llu revision=%llu rhi=%llu worldtype=%d scenekey=%p; proxyaccepted=%llu proxyrejected=%llu. "
			"terrain-local bounds min=(%.0f, %.0f, %.0f) max=(%.0f, %.0f, %.0f). "
			"Live triangle count: arc.Terrain.CBT.Stats 1.",
			Bindings.Faces.Num(), NumBatches, Buffers.GetPoolDepth(),
			SelectedSource.ProxySerial, static_cast<uint32>(SelectedSource.SlotIndex),
			Bindings.Identity.Owner.Key.WorldInstanceToken, Bindings.Identity.Owner.Key.TerrainInstanceId,
			Bindings.Identity.Owner.OwnerEpoch, Bindings.Identity.ResourceRevision, Bindings.Identity.RhiEpoch,
			static_cast<int32>(Bindings.Identity.WorldType), Bindings.Identity.SceneKey,
			AcceptedIdentity.Owner.Key.WorldInstanceToken, AcceptedIdentity.Owner.Key.TerrainInstanceId,
			AcceptedIdentity.Owner.OwnerEpoch, AcceptedIdentity.ResourceRevision, AcceptedIdentity.RhiEpoch,
			static_cast<int32>(AcceptedIdentity.WorldType), AcceptedIdentity.SceneKey,
			ProxyAccepted, ProxyRejected,
			Bindings.LocalBounds.Min.X, Bindings.LocalBounds.Min.Y, Bindings.LocalBounds.Min.Z,
			Bindings.LocalBounds.Max.X, Bindings.LocalBounds.Max.Y, Bindings.LocalBounds.Max.Z);
	}
}

bool FArcTerrainSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}
