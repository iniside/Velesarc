// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliagePrimitive.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GPUSceneWriter.h"
#include "InstancedStaticMesh/ISMInstanceDataSceneProxy.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "NaniteSceneProxy.h"
#include "PrimitiveComponentHelper.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "UObject/ObjectKey.h"

namespace ArcEcotopeFoliagePrimitivePrivate
{
#if WITH_EDITOR
	TSet<FObjectKey> KickedMaterials;

	void KickIncompleteMaterialCompile(UMaterialInterface* Material)
	{
		if (!IsInGameThread() || Material == nullptr || Material->IsComplete() || Material->IsCompiling())
		{
			return;
		}

		// One kick per material: a material that never reaches IsComplete() would otherwise re-enter
		// UpdateStaticPermutation() on every rebuild, and each compile re-arms that rebuild.
		bool bAlreadyKicked = false;
		KickedMaterials.Add(FObjectKey(Material), &bAlreadyKicked);
		if (bAlreadyKicked)
		{
			return;
		}

		if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material))
		{
			MaterialInstance->CacheShaders(EMaterialShaderPrecompileMode::Background);

			// For an instance, FMaterial::CacheShaders keeps bShaderMapValid at "has a shader map at all"
			// and so skips BeginCompileShaderMap on an incomplete-but-present map
			// (MaterialShared.cpp:3304-3325); forcing a static-permutation update queues one regardless.
			if (!MaterialInstance->IsComplete() && !MaterialInstance->IsCompiling())
			{
				MaterialInstance->UpdateStaticPermutation();
			}
		}
		else if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
		{
			BaseMaterial->ForceRecompileForRendering();
		}
	}
#endif // WITH_EDITOR
}

FArcEcotopeFoliagePrimitive::~FArcEcotopeFoliagePrimitive()
{
	check(PrimitiveSceneData.SceneProxy == nullptr);
}

bool FArcEcotopeFoliagePrimitive::Initialize(
	UWorld* InWorld,
	UObject* InPrimitiveUObject,
	UStaticMesh* InMesh,
	TConstArrayView<TObjectPtr<UMaterialInterface>> InMaterialOverrides,
	const FArcEcotopeFoliageRenderSettings& InRenderSettings)
{
	check(IsInGameThread());
	check(PrimitiveSceneData.SceneProxy == nullptr);

	if (InWorld == nullptr || InWorld->Scene == nullptr || InMesh == nullptr || InPrimitiveUObject == nullptr)
	{
		return false;
	}

	World = InWorld;
	Scene = InWorld->Scene;
	PrimitiveUObject = InPrimitiveUObject;
	MeshRef.Reset(InMesh);
	RenderSettings = InRenderSettings;
	MaterialRefs.Reset();
	MaterialRefs.Append(InMaterialOverrides);

	MaterialGCRoots.Reset();
	MaterialGCRoots.Reserve(InMaterialOverrides.Num());
	for (const TObjectPtr<UMaterialInterface>& Material : InMaterialOverrides)
	{
		MaterialGCRoots.Emplace(Material.Get());
	}

	return true;
}

void FArcEcotopeFoliagePrimitive::SetTransformAndBounds(const FTransform& InTransform, const FBoxSphereBounds& InLocalBounds)
{
	check(IsInGameThread());

	Transform = InTransform;
	LocalBounds = InLocalBounds;
	WorldBounds = InLocalBounds.TransformBy(InTransform);

	if (PrimitiveSceneData.SceneProxy != nullptr && Scene != nullptr)
	{
		SceneDesc.RenderMatrix = Transform.ToMatrixWithScale();
		SceneDesc.AttachmentRootPosition = Transform.GetTranslation();
		SceneDesc.Bounds = WorldBounds;
		SceneDesc.LocalBounds = LocalBounds;
		Scene->UpdatePrimitiveTransform(&SceneDesc);
	}
}

int32 FArcEcotopeFoliagePrimitive::EnsureInstanceAllocation(int32 NumRequiredInstances)
{
	check(IsInGameThread());

	if (NumRequiredInstances <= 0)
	{
		// Returning zero stops the caller enqueuing, so nothing would hide the previous dispatch's
		// transforms; the primitive leaves the scene instead.
		NumInstances = 0;
		RemoveFromScene();
		return 0;
	}

	if (Scene == nullptr)
	{
		return NumAllocatedInstances;
	}

	// GPUScene freezes the instance span at proxy creation, so growth is a recreate, not a resize.
	if (PrimitiveSceneData.SceneProxy == nullptr || NumAllocatedInstances < NumRequiredInstances)
	{
		NumInstances = NumRequiredInstances;
		RecreateProxy();
	}
	NumInstances = NumRequiredInstances;

	return NumAllocatedInstances;
}

void FArcEcotopeFoliagePrimitive::UpdateInstancesGPU(int32 NumRequiredInstances, FGpuInstanceUpdateFunction UpdateFunction)
{
	check(IsInGameThread());

	if (EnsureInstanceAllocation(NumRequiredInstances) <= 0 || PrimitiveSceneData.SceneProxy == nullptr)
	{
		return;
	}

	PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
	Scene->UpdatePrimitiveInstancesFromCompute(
		&SceneDesc,
		FGPUSceneWriteDelegate::CreateLambda(
			[UpdateFunction_RT = MoveTemp(UpdateFunction)](FRDGBuilder& GraphBuilder, const FGPUSceneWriteDelegateParams& Params)
			{
				UpdateFunction_RT(GraphBuilder, Params);
			}));
	PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
}

void FArcEcotopeFoliagePrimitive::RemoveFromScene()
{
	check(IsInGameThread());

	if (PrimitiveSceneData.SceneProxy != nullptr && Scene != nullptr)
	{
		Scene->RemovePrimitive(&SceneDesc);
		PrimitiveSceneData.SceneProxy = nullptr;
	}

	NumAllocatedInstances = 0;
}

void FArcEcotopeFoliagePrimitive::Release(TUniquePtr<FArcEcotopeFoliagePrimitive>&& InPrimitive)
{
	check(IsInGameThread());

	if (!InPrimitive)
	{
		return;
	}

	InPrimitive->RemoveFromScene();

	FArcEcotopeFoliagePrimitive* RawPrimitive = InPrimitive.Release();
	ENQUEUE_RENDER_COMMAND(DeleteArcEcotopeFoliagePrimitive)(
		[RawPrimitive](FRHICommandListImmediate&)
		{
			delete RawPrimitive;
		});
}

void FArcEcotopeFoliagePrimitive::RecreateProxy()
{
	RemoveFromScene();

	if (FPrimitiveSceneProxy* SceneProxy = CreateSceneProxy())
	{
		NumAllocatedInstances = NumInstances;
		AddToScene(SceneProxy);
	}
}

FPrimitiveSceneProxy* FArcEcotopeFoliagePrimitive::CreateSceneProxy()
{
	LLM_SCOPE(ELLMTag::InstancedMesh);

	UStaticMesh* Mesh = MeshRef.Get();
	if (NumInstances <= 0 || Mesh == nullptr || Mesh->IsCompiling() || Scene == nullptr || !PrimitiveUObject.IsValid())
	{
		return nullptr;
	}

	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (RenderData == nullptr || !RenderData->IsInitialized())
	{
		return nullptr;
	}

	FillProxyDesc();
	ProxyDesc.InstanceDataSceneProxy = BuildInstanceData();

	Nanite::FMaterialAudit NaniteMaterials{};
	if (ProxyDesc.ShouldCreateNaniteProxy(&NaniteMaterials))
	{
		return ::new Nanite::FSceneProxy(NaniteMaterials, ProxyDesc);
	}

	const FStaticMeshLODResourcesArray& LODResources = RenderData->LODResources;
	const int32 MinLODIdx = Mesh->GetMinLODIdx();
	if (LODResources.Num() == 0
		|| LODResources[FMath::Clamp<int32>(MinLODIdx, 0, LODResources.Num() - 1)].VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		return nullptr;
	}

	return ::new FInstancedStaticMeshSceneProxy(ProxyDesc, Scene->GetFeatureLevel());
}

void FArcEcotopeFoliagePrimitive::BuildRenderSafeMaterials()
{
	UStaticMesh* Mesh = MeshRef.Get();
	const int32 NumSlots = Mesh != nullptr ? Mesh->GetStaticMaterials().Num() : 0;
	const int32 Count = FMath::Max(NumSlots, MaterialRefs.Num());

	RenderSafeMaterials.Reset();
	RenderSafeMaterials.SetNumZeroed(Count);

	// Resolve through the desc so the completeness check sees what the Nanite audit will bind: the audit
	// asks GetNaniteAuditMaterial (StaticMeshSceneProxyDesc.cpp:214-217), which substitutes the material's
	// Nanite override (StaticMeshComponentHelper.h:166-170).
	ProxyDesc.StaticMesh = Mesh;
	ProxyDesc.OverrideMaterials = MakeArrayView(MaterialRefs);

	bool bAnySubstituted = false;
	for (int32 SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
	{
		UMaterialInterface* Override = MaterialRefs.IsValidIndex(SlotIndex) ? MaterialRefs[SlotIndex].Get() : nullptr;
		UMaterialInterface* Effective = ProxyDesc.GetNaniteAuditMaterial(SlotIndex);

		if (Effective != nullptr && !Effective->IsComplete())
		{
#if WITH_EDITOR
			ArcEcotopeFoliagePrimitivePrivate::KickIncompleteMaterialCompile(Effective);
#endif
			RenderSafeMaterials[SlotIndex] = UMaterial::GetDefaultMaterial(MD_Surface);
			bAnySubstituted = true;
		}
		else
		{
			// A null override keeps the mesh's own slot material.
			RenderSafeMaterials[SlotIndex] = Override;
		}
	}

	if (!bAnySubstituted && MaterialRefs.IsEmpty())
	{
		RenderSafeMaterials.Reset();
	}

	ProxyDesc.OverrideMaterials = MakeArrayView(RenderSafeMaterials);
}

void FArcEcotopeFoliagePrimitive::FillProxyDesc()
{
	UStaticMesh* Mesh = MeshRef.Get();

	BuildRenderSafeMaterials();

	ProxyDesc.StaticMesh = Mesh;
	ProxyDesc.NaniteResources = Mesh->GetRenderData()->GetNaniteResources();
	ProxyDesc.OverrideMaterials = MakeArrayView(RenderSafeMaterials);
	ProxyDesc.Scene = Scene;
	ProxyDesc.World = World;
	ProxyDesc.FeatureLevel = Scene->GetFeatureLevel();
	ProxyDesc.Owner = PrimitiveUObject.Get();
	ProxyDesc.Component = PrimitiveUObject.Get();
	ProxyDesc.ComponentId = PrimitiveSceneData.PrimitiveSceneId;
	ProxyDesc.CustomPrimitiveData = &CustomPrimitiveData;
	ProxyDesc.StatId = Mesh->GetStatID(true);
	ProxyDesc.Mobility = EComponentMobility::Movable;
	ProxyDesc.bIsVisible = true;
	ProxyDesc.bIsInstancedStaticMesh = true;
	ProxyDesc.bCollisionEnabled = false;
	ProxyDesc.CastShadow = RenderSettings.bCastShadow;
	ProxyDesc.bCastDynamicShadow = RenderSettings.bCastDynamicShadow;
	ProxyDesc.bCastContactShadow = RenderSettings.bCastContactShadow;
	ProxyDesc.bCastShadowAsTwoSided = RenderSettings.bCastShadowAsTwoSided;
	ProxyDesc.bCastFarShadow = RenderSettings.bCastFarShadow;
	ProxyDesc.InstanceMinDrawDistance = RenderSettings.InstanceMinDrawDistance;
	ProxyDesc.InstanceStartCullDistance = RenderSettings.InstanceStartCullDistance;
	ProxyDesc.InstanceEndCullDistance = RenderSettings.InstanceEndCullDistance;
	ProxyDesc.MinDrawDistance = RenderSettings.MinDrawDistance;
	ProxyDesc.CachedMaxDrawDistance = RenderSettings.MaxDrawDistance;
	ProxyDesc.bEvaluateWorldPositionOffset = RenderSettings.bEvaluateWorldPositionOffset;
	ProxyDesc.WorldPositionOffsetDisableDistance = RenderSettings.WorldPositionOffsetDisableDistance;
	ProxyDesc.bAffectDynamicIndirectLighting = RenderSettings.bAffectDynamicIndirectLighting;
	ProxyDesc.bAffectDistanceFieldLighting = RenderSettings.bAffectDistanceFieldLighting;
	ProxyDesc.bReceivesDecals = RenderSettings.bReceivesDecals;
	ProxyDesc.bVisibleInRayTracing = RenderSettings.bVisibleInRayTracing;
	ProxyDesc.bUseGpuLodSelection = RenderSettings.bUseGpuLodSelection;
	ProxyDesc.InstanceLODDistanceScale = RenderSettings.InstanceLODDistanceScale;
	ProxyDesc.SetMaterialRelevance(ProxyDesc.GetMaterialRelevance(Scene->GetShaderPlatform()));
	ProxyDesc.SetCollisionResponseToChannels(FCollisionResponseContainer::GetDefaultResponseContainer());
}

TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> FArcEcotopeFoliagePrimitive::BuildInstanceData()
{
	FInstanceSceneDataBuffers InstanceSceneDataBuffers(/*InbInstanceDataIsGPUOnly=*/true);
	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView View = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	InstanceSceneDataBuffers.SetPrimitiveLocalToWorld(Transform.ToMatrixWithScale(), AccessTag);

	const FPrimitiveMaterialPropertyDescriptor MaterialDesc =
		FPrimitiveComponentHelper::GetUsedMaterialPropertyDesc(ProxyDesc, Scene->GetShaderPlatform());
	const float LocalAbsMaxDisplacement =
		FMath::Max(-MaterialDesc.MinMaxMaterialDisplacement.X, MaterialDesc.MinMaxMaterialDisplacement.Y)
		+ MaterialDesc.MaxWorldPositionOffsetDisplacement;
	const FVector3f PadExtent =
		FISMCInstanceDataSceneProxy::GetLocalBoundsPadExtent(View.PrimitiveToRelativeWorld, LocalAbsMaxDisplacement);

	FRenderBounds InstanceLocalBounds = MeshRef->GetBounds();
	InstanceLocalBounds.Min -= PadExtent;
	InstanceLocalBounds.Max += PadExtent;
	check(!View.Flags.bHasPerInstanceLocalBounds);
	View.InstanceLocalBounds.Add(InstanceLocalBounds);

	View.NumInstancesGPUOnly = NumInstances;
	View.NumCustomDataFloats = 0;
	View.Flags.bHasPerInstanceCustomData = false;
	View.Flags.bHasPerInstanceRandom = MaterialDesc.bAnyMaterialHasPerInstanceRandom;

	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);
	InstanceSceneDataBuffers.ValidateData();

	return MakeShared<FInstanceDataSceneProxy, ESPMode::ThreadSafe>(MoveTemp(InstanceSceneDataBuffers));
}

void FArcEcotopeFoliagePrimitive::AddToScene(FPrimitiveSceneProxy* SceneProxy)
{
	PrimitiveSceneData.SceneProxy = SceneProxy;

	SceneDesc.ProxyDesc = nullptr;
	SceneDesc.SceneProxy = SceneProxy;
	SceneDesc.PrimitiveSceneData = &PrimitiveSceneData;
	SceneDesc.PrimitiveComponentInterface = nullptr;
	SceneDesc.PrimitiveUObject = PrimitiveUObject.Get();
	SceneDesc.bRecreateProxyOnUpdateTransform = false;
	SceneDesc.Mobility = EComponentMobility::Movable;
	SceneDesc.RenderMatrix = Transform.ToMatrixWithScale();
	SceneDesc.AttachmentRootPosition = Transform.GetTranslation();
	SceneDesc.Bounds = WorldBounds;
	SceneDesc.LocalBounds = LocalBounds;

	Scene->AddPrimitive(&SceneDesc);
}
