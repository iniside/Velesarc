// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"

#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "MassEntityManager.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "Animation/TransformProviderData.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SceneInterface.h"
#include "SkeletalRenderPublic.h"
#include "Components/InstancedSkinnedMeshComponent.h"

TArray<TObjectPtr<UMaterialInterface>> FArcMassInstancedSkinnedMeshRenderStateHelper::EmptyMaterialSlotOverlays;

FArcMassInstancedSkinnedMeshRenderStateHelper::FArcMassInstancedSkinnedMeshRenderStateHelper(
	FMassEntityHandle InEntityHandle,
	TNotNull<FMassEntityManager*> InEntityManager,
	const FMassRenderPrimitiveFragment& RenderPrimitiveFragment,
	const FMassOverrideMaterialsFragment* OverrideMaterialsFragment)
	: Super(InEntityHandle, InEntityManager, RenderPrimitiveFragment, OverrideMaterialsFragment)
{
}

FArcMassInstancedSkinnedMeshRenderStateHelper::~FArcMassInstancedSkinnedMeshRenderStateHelper()
{
	// MeshObject is intentionally NOT deleted here — it's preserved across proxy recreations.
	// Call DestroyMeshObject() explicitly when the holder entity is destroyed.
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::DestroyMeshObject()
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (ISMFrag.MeshObject)
	{
		ISMFrag.MeshObject->ReleaseResources();
		BeginCleanup(ISMFrag.MeshObject);
		ISMFrag.MeshObject = nullptr;
	}
}

bool FArcMassInstancedSkinnedMeshRenderStateHelper::ShouldCreateRenderState() const
{
	const FArcMassRenderISMSkinnedFragment& ISMFrag = GetRenderISMSkinnedFragment();
	return ISMFrag.PerInstanceData.Num() > 0 && ISMFrag.SceneProxyDesc.IsValid();
}

TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe>& FArcMassInstancedSkinnedMeshRenderStateHelper::BuildInstanceData()
{
	FInstanceSceneDataBuffers InstanceSceneDataBuffers{};
	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView View = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	// Primitive transform
	InstanceSceneDataBuffers.SetPrimitiveLocalToWorld(GetRenderMatrix(), AccessTag);

	// Instance local bounds from skinned asset
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	FRenderBounds InstanceLocalBounds = Asset ? Asset->GetBounds() : FRenderBounds();
	View.InstanceLocalBounds.Add(InstanceLocalBounds);

	const FArcMassRenderISMSkinnedFragment& ISMFrag = GetRenderISMSkinnedFragment();

	// Per-instance transforms
	View.InstanceToPrimitiveRelative.Reserve(ISMFrag.PerInstanceData.Num());
	for (const FArcSkinnedMeshInstanceData& InstanceData : ISMFrag.PerInstanceData)
	{
		FRenderTransform InstanceToPrimitive = FRenderTransform(InstanceData.Transform.ToMatrixWithScale());
		FRenderTransform LocalToPrimitiveRelativeWorld = InstanceToPrimitive * View.PrimitiveToRelativeWorld;
		LocalToPrimitiveRelativeWorld.Orthogonalize();
		View.InstanceToPrimitiveRelative.Add(LocalToPrimitiveRelativeWorld);
	}

	// Skinning data
	UTransformProviderData* TransformProvider = SkinnedMeshFrag.TransformProvider;
	View.Flags.bHasPerInstanceSkinningData = true;
	View.InstanceSkinningData.Reserve(ISMFrag.PerInstanceData.Num());
	int32 InstanceIdx = 0;
	for (const FArcSkinnedMeshInstanceData& InstanceData : ISMFrag.PerInstanceData)
	{
		uint32 SkinningOffset = 0;
		if (TransformProvider)
		{
			FSkinnedMeshInstanceData SkinData;
			SkinData.Transform = InstanceData.Transform;
			SkinData.AnimationIndex = InstanceData.AnimationIndex;
			SkinningOffset = TransformProvider->GetSkinningDataOffset(
				InstanceIdx, GetComponentTransform(), SkinData);
		}
		View.InstanceSkinningData.Add(SkinningOffset);
		++InstanceIdx;
	}

	// Custom data
	View.InstanceCustomData = ISMFrag.PerInstanceCustomData;
	View.NumCustomDataFloats = ISMFrag.PerInstanceData.Num() > 0 && ISMFrag.PerInstanceCustomData.Num() > 0
		? ISMFrag.PerInstanceCustomData.Num() / ISMFrag.PerInstanceData.Num() : 0;
	View.Flags.bHasPerInstanceCustomData = View.NumCustomDataFloats > 0;
	if (!View.Flags.bHasPerInstanceCustomData)
	{
		View.NumCustomDataFloats = 0;
		View.InstanceCustomData.Reset();
	}

	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);
	InstanceSceneDataBuffers.ValidateData();

	DataProxy = MakeShared<FInstanceDataSceneProxy, ESPMode::ThreadSafe>(MoveTemp(InstanceSceneDataBuffers));
	return DataProxy;
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::CreateRenderState(FRegisterComponentContext* Context)
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();

	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	if (!Asset)
	{
		return;
	}

	FSkeletalMeshRenderData* RenderData = Asset->GetResourceForRendering();
	if (!RenderData)
	{
		return;
	}

	// Create MeshObject if not already created (first time or after holder destruction)
	if (!ISMFrag.MeshObject && ISMFrag.SceneProxyDesc.IsValid())
	{
		ISMFrag.SceneProxyDesc->Scene = GetScene();
		ISMFrag.MeshObject = FInstancedSkinnedMeshSceneProxyDesc::CreateMeshObject(
			*ISMFrag.SceneProxyDesc, RenderData, GetScene()->GetFeatureLevel());
	}

	if (!ISMFrag.MeshObject)
	{
		return;
	}

	// Submit animation changes before building instance data
	UTransformProviderData* TransformProvider = SkinnedMeshFrag.TransformProvider;
	if (TransformProvider && TransformProvider->IsEnabled())
	{
		TransformProvider->SubmitChanges();
	}

	// Set mesh object on descriptor for proxy creation
	ISMFrag.SceneProxyDesc->MeshObject = ISMFrag.MeshObject;
	ISMFrag.SceneProxyDesc->InstanceDataSceneProxy = BuildInstanceData();

	// Delegate to base class which calls CreateSceneProxy() then registers with scene
	Super::CreateRenderState(Context);
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::DestroyRenderState(FMassDestroyRenderStateContext* Context)
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	ISMFrag.InstanceDataSceneProxy.Reset();
	// MeshObject is NOT deleted here — preserved across proxy recreations

	Super::DestroyRenderState(Context);
}

FPrimitiveSceneProxy* FArcMassInstancedSkinnedMeshRenderStateHelper::CreateSceneProxy(ESceneProxyCreationError* OutError)
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (!ISMFrag.SceneProxyDesc.IsValid() || !ISMFrag.MeshObject || ISMFrag.PerInstanceData.Num() == 0)
	{
		return nullptr;
	}

	InitializeSceneProxyDescDynamicProperties();

	const bool bHideSkin = false;
	const bool bNanite = ISMFrag.SceneProxyDesc->ShouldNaniteSkin();
	const bool bEnabled = true;

	PrimitiveSceneData.SceneProxy = FInstancedSkinnedMeshSceneProxyDesc::CreateSceneProxy(
		*ISMFrag.SceneProxyDesc, bHideSkin, bNanite, bEnabled);
	return PrimitiveSceneData.SceneProxy;
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::InitializeSceneProxyDescDynamicProperties()
{
	Super::InitializeSceneProxyDescDynamicProperties();

	// Restore InstanceDataSceneProxy after ResetSceneProxyDescUnsupportedProperties nulled it
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (ISMFrag.SceneProxyDesc.IsValid())
	{
		ISMFrag.SceneProxyDesc->InstanceDataSceneProxy = DataProxy;
	}
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::ResetSceneProxyDescUnsupportedProperties()
{
	Super::ResetSceneProxyDescUnsupportedProperties();

	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (ISMFrag.SceneProxyDesc.IsValid())
	{
		ISMFrag.SceneProxyDesc->InstanceDataSceneProxy = nullptr;
	}
}

FPrimitiveSceneProxyDesc& FArcMassInstancedSkinnedMeshRenderStateHelper::GetSceneProxyDesc()
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	checkf(ISMFrag.SceneProxyDesc.IsValid(), TEXT("Expecting a valid scene proxy desc"));
	return *ISMFrag.SceneProxyDesc;
}

const FPrimitiveSceneProxyDesc& FArcMassInstancedSkinnedMeshRenderStateHelper::GetSceneProxyDesc() const
{
	const FArcMassRenderISMSkinnedFragment& ISMFrag = GetRenderISMSkinnedFragment();
	checkf(ISMFrag.SceneProxyDesc.IsValid(), TEXT("Expecting a valid scene proxy desc"));
	return *ISMFrag.SceneProxyDesc;
}

UMaterialInterface* FArcMassInstancedSkinnedMeshRenderStateHelper::GetMaterial(int32 MaterialIndex) const
{
	TConstArrayView<TObjectPtr<UMaterialInterface>> Overrides = GetOverrideMaterials();
	if (Overrides.IsValidIndex(MaterialIndex) && Overrides[MaterialIndex])
	{
		return Overrides[MaterialIndex];
	}
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	if (Asset && Asset->GetMaterials().IsValidIndex(MaterialIndex))
	{
		return Asset->GetMaterials()[MaterialIndex].MaterialInterface;
	}
	return nullptr;
}

int32 FArcMassInstancedSkinnedMeshRenderStateHelper::GetNumMaterials() const
{
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	return Asset ? Asset->GetMaterials().Num() : 0;
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool /*bGetDebugMaterials*/) const
{
	const int32 NumMaterials = GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		UMaterialInterface* Material = GetMaterial(MaterialIndex);
		if (Material)
		{
			OutMaterials.Add(Material);
		}
	}
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& /*AssetMaterialSlotOverlayMaterials*/) const
{
}

const TArray<TObjectPtr<UMaterialInterface>>& FArcMassInstancedSkinnedMeshRenderStateHelper::GetComponentMaterialSlotsOverlayMaterial() const
{
	return EmptyMaterialSlotOverlays;
}

UMaterialInterface* FArcMassInstancedSkinnedMeshRenderStateHelper::GetOverlayMaterial() const
{
	return nullptr;
}

// ISkinnedMeshComponent
USkinnedAsset* FArcMassInstancedSkinnedMeshRenderStateHelper::GetSkinnedAsset() const
{
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	return SkinnedMeshFrag.SkinnedAsset.Get();
}

IPrimitiveComponent* FArcMassInstancedSkinnedMeshRenderStateHelper::GetPrimitiveComponentInterface()
{
	return this;
}

// Fragment accessors
const FArcMassRenderISMSkinnedFragment& FArcMassInstancedSkinnedMeshRenderStateHelper::GetRenderISMSkinnedFragment() const
{
	return GetEntityManager().GetFragmentDataChecked<FArcMassRenderISMSkinnedFragment>(EntityHandle);
}

FArcMassRenderISMSkinnedFragment& FArcMassInstancedSkinnedMeshRenderStateHelper::GetMutableRenderISMSkinnedFragment()
{
	return GetEntityManager().GetFragmentDataChecked<FArcMassRenderISMSkinnedFragment>(EntityHandle);
}

const FArcMassSkinnedMeshFragment& FArcMassInstancedSkinnedMeshRenderStateHelper::GetSkinnedMeshFragment() const
{
	return GetEntityManager().GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(EntityHandle);
}
