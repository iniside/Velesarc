// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNiagaraRenderStateHelper.h"

#include "ArcNiagaraVisFragments.h"
#include "MassEntityFragments.h"
#include "MassEntityManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSceneProxy.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemRenderData.h"
#include "PrimitiveSceneDesc.h"
#include "PrimitiveSceneInfoData.h"
#include "PrimitiveSceneProxyDesc.h"
#include "SceneInterface.h"
#include "Engine/World.h"
#include "RenderingThread.h"

// ---------------------------------------------------------------------------
// FArcNiagaraVisFragment — destructor & move (must be in a .cpp that sees the full type)
// ---------------------------------------------------------------------------

FArcNiagaraVisFragment::~FArcNiagaraVisFragment()
{
	if (RenderStateHelper)
	{
		if (RenderStateHelper->HasSceneProxy())
		{
			RenderStateHelper->DestroyNiagaraRenderState();
		}
		delete RenderStateHelper;
		RenderStateHelper = nullptr;
	}
}

FArcNiagaraVisFragment::FArcNiagaraVisFragment(FArcNiagaraVisFragment&& Other)
	: RenderStateHelper(Other.RenderStateHelper)
{
	Other.RenderStateHelper = nullptr;
}

// ---------------------------------------------------------------------------
// FArcNiagaraRenderStateHelper
// ---------------------------------------------------------------------------

FArcNiagaraRenderStateHelper::FArcNiagaraRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager)
	: EntityHandle(InEntityHandle)
	, EntityManager(StaticCastSharedRef<FMassEntityManager>(InEntityManager->AsShared()))
	, CachedBounds(FBoxSphereBounds(FVector::ZeroVector, FVector(100.0f), 100.0f))
{
}

FArcNiagaraRenderStateHelper::~FArcNiagaraRenderStateHelper()
{
	if (NiagaraProxy)
	{
		DestroyNiagaraRenderState();
	}

	if (Controller.IsValid())
	{
		Controller->Deactivate(true);
		Controller->Release();
		Controller.Reset();
	}
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::Initialize(UWorld& World, UNiagaraSystem& System, bool bCastShadow)
{
	WeakWorld = &World;
	SystemAsset = &System;
	bCastShadowCached = bCastShadow;

	Controller = MakeShared<FNiagaraSystemInstanceController>();
	Controller->Initialize(
		World,
		System,
		/*OverrideParameters=*/ nullptr,
		/*AttachComponent=*/ nullptr,
		ENiagaraTickBehavior::UsePrereqs,
		/*bPooled=*/ false,
		/*RandomSeedOffset=*/ 0,
		/*bForceSolo=*/ false,
		/*WarmupTickCount=*/ 0,
		/*WarmupTickDelta=*/ 0.f
	);

	Controller->Activate(FNiagaraSystemInstance::EResetMode::ResetAll);
}

// ---------------------------------------------------------------------------
// CreateNiagaraRenderState / CreateRenderState
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::CreateNiagaraRenderState()
{
	CreateRenderState(nullptr);
}

void FArcNiagaraRenderStateHelper::CreateRenderState(FRegisterComponentContext* /*Context*/)
{
	check(Controller.IsValid());
	check(!NiagaraProxy);

	bRenderStateDirty = false;

	FSceneInterface* Scene = GetScene();
	check(Scene);

	// Fill the Niagara-specific proxy descriptor
	ProxyDesc.SystemAsset = SystemAsset.Get();
	ProxyDesc.SystemInstanceController = Controller;
	ProxyDesc.OcclusionQueryMode = ENiagaraOcclusionQueryMode::Default;

	// Base FPrimitiveSceneProxyDesc fields
	ProxyDesc.Mobility = EComponentMobility::Movable;
	ProxyDesc.bCastDynamicShadow = bCastShadowCached;
	ProxyDesc.bIsVisible = true;
	ProxyDesc.Component = nullptr;
	ProxyDesc.PrimitiveComponentInterface = this;
	ProxyDesc.Owner = EntityManager->GetOwner();
#if !WITH_STATE_STREAM
	ProxyDesc.World = WeakWorld.Get();
#endif
	ProxyDesc.Scene = Scene;
	ProxyDesc.FeatureLevel = WeakWorld->GetFeatureLevel();
	ProxyDesc.CustomPrimitiveData = &CustomPrimitiveData;

	// Create the scene proxy
	NiagaraProxy = new FNiagaraSceneProxy(ProxyDesc);
	PrimitiveSceneData.SceneProxy = NiagaraProxy;

	// Build scene desc and register with the scene
	FPrimitiveSceneDesc Desc;
	Desc.SceneProxy = NiagaraProxy;
	Desc.ProxyDesc = &ProxyDesc;
	Desc.PrimitiveSceneData = &PrimitiveSceneData;
#if WITH_EDITOR
	Desc.PrimitiveComponentInterface = this;
#endif
	Desc.RenderMatrix = GetTransform().ToMatrixWithScale();
	Desc.AttachmentRootPosition = GetTransform().GetTranslation();
	Desc.LocalBounds = CachedBounds;
	Desc.Bounds = CachedBounds;
	Desc.Mobility = EComponentMobility::Movable;

	Scene->AddPrimitive(&Desc);
}

// ---------------------------------------------------------------------------
// DestroyNiagaraRenderState / DestroyRenderState
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::DestroyNiagaraRenderState()
{
	DestroyRenderState();
}

void FArcNiagaraRenderStateHelper::DestroyRenderState()
{
	if (NiagaraProxy)
	{
		NiagaraProxy->DestroyRenderState_Concurrent();

		FSceneInterface* Scene = GetScene();
		if (Scene)
		{
			FPrimitiveSceneDesc Desc;
			Desc.SceneProxy = NiagaraProxy;
			Desc.ProxyDesc = &ProxyDesc;
			Desc.PrimitiveSceneData = &PrimitiveSceneData;
			Scene->RemovePrimitive(&Desc);
		}

		PrimitiveSceneData.SceneProxy = nullptr;
		NiagaraProxy = nullptr;
	}

	bRenderStateDirty = false;
}

// ---------------------------------------------------------------------------
// SendDynamicRenderData
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::SendDynamicRenderData()
{
	if (!NiagaraProxy || !Controller.IsValid())
	{
		return;
	}

	// Use the proxy's own RenderData — the proxy constructor creates it via
	// SystemInstanceController->CreateSystemRenderData(). We must use the same
	// RenderData the proxy uses for GetDynamicMeshElements, otherwise dynamic
	// data goes to the wrong renderer objects.
	FNiagaraSystemRenderData* ProxyRenderData = NiagaraProxy->GetSystemRenderData();
	if (!ProxyRenderData)
	{
		return;
	}

	FNiagaraSceneProxy::FDynamicData NewProxyDynamicData;
	if (FNiagaraSystemInstance* SystemInstance = Controller->GetSystemInstance_Unsafe())
	{
		NewProxyDynamicData.LWCRenderTile = SystemInstance->GetLWCTile();
	}
	NewProxyDynamicData.LODDistanceOverride = -1.0f;

	FNiagaraSystemRenderData::FSetDynamicDataCommandList SetDataCommands;
	Controller->GenerateSetDynamicDataCommands(SetDataCommands, *ProxyRenderData, *NiagaraProxy);

	FNiagaraSceneProxy* ProxyCapture = NiagaraProxy;
	ENQUEUE_RENDER_COMMAND(ArcNiagaraSetDynamicData)(
		[ProxyCapture, CommandsRT = MoveTemp(SetDataCommands), NewProxyDynamicData](FRHICommandListImmediate& /*RHICmdList*/)
		{
			FNiagaraSystemRenderData::ExecuteDynamicDataCommands_RenderThread(CommandsRT);
			ProxyCapture->SetProxyDynamicData(NewProxyDynamicData);
		}
	);

	Controller->PostTickRenderers(*ProxyRenderData);
}

// ---------------------------------------------------------------------------
// UpdateTransform
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::UpdateTransform()
{
	FSceneInterface* Scene = GetScene();
	if (!Scene || !NiagaraProxy || !Controller.IsValid())
	{
		return;
	}

	const FTransform& CurrentTransform = EntityManager->GetFragmentDataChecked<FTransformFragment>(EntityHandle).GetTransform();

	// Update the Niagara system instance's world transform so particles spawn at the
	// correct location. Without an AttachComponent the instance won't auto-update this.
	if (FNiagaraSystemInstance* SystemInstance = Controller->GetSystemInstance_Unsafe())
	{
		SystemInstance->SetWorldTransform(CurrentTransform);
	}

	// Update cached bounds
	CachedBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(100.0f), 100.0f).TransformBy(CurrentTransform);

	FPrimitiveSceneDesc Desc;
	Desc.SceneProxy = NiagaraProxy;
	Desc.ProxyDesc = &ProxyDesc;
	Desc.PrimitiveSceneData = &PrimitiveSceneData;
	Desc.RenderMatrix = CurrentTransform.ToMatrixWithScale();
	Desc.AttachmentRootPosition = CurrentTransform.GetTranslation();
	Desc.LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(100.0f), 100.0f);
	Desc.Bounds = CachedBounds;
	Desc.Mobility = EComponentMobility::Movable;

	Scene->UpdatePrimitiveTransform(&Desc);
}

// ---------------------------------------------------------------------------
// IPrimitiveComponent interface
// ---------------------------------------------------------------------------

bool FArcNiagaraRenderStateHelper::IsRenderStateCreated() const
{
	return NiagaraProxy != nullptr;
}

bool FArcNiagaraRenderStateHelper::IsRenderStateDirty() const
{
	return bRenderStateDirty;
}

bool FArcNiagaraRenderStateHelper::ShouldCreateRenderState() const
{
	return FApp::CanEverRender() && Controller.IsValid() && SystemAsset.IsValid();
}

bool FArcNiagaraRenderStateHelper::IsRegistered() const
{
	return NiagaraProxy != nullptr;
}

bool FArcNiagaraRenderStateHelper::IsUnreachable() const
{
	const UObject* OwnerPtr = EntityManager->GetOwner();
	return !OwnerPtr || OwnerPtr->IsUnreachable();
}

bool FArcNiagaraRenderStateHelper::IsStaticMobility() const
{
	return false;
}

bool FArcNiagaraRenderStateHelper::IsMipStreamingForced() const
{
	return false;
}

UWorld* FArcNiagaraRenderStateHelper::GetWorld() const
{
	return WeakWorld.Get();
}

FSceneInterface* FArcNiagaraRenderStateHelper::GetScene() const
{
	return WeakWorld.IsValid() ? WeakWorld->Scene : nullptr;
}

FPrimitiveSceneProxy* FArcNiagaraRenderStateHelper::GetSceneProxy() const
{
	return NiagaraProxy;
}

void FArcNiagaraRenderStateHelper::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool /*bGetDebugMaterials*/) const
{
	if (Controller.IsValid())
	{
		Controller->GetUsedMaterials(OutMaterials);
	}
}

void FArcNiagaraRenderStateHelper::MarkRenderStateDirty()
{
	bRenderStateDirty = true;
}

FString FArcNiagaraRenderStateHelper::GetName() const
{
	return GetNameSafe(EntityManager->GetOwner());
}

FString FArcNiagaraRenderStateHelper::GetFullName() const
{
	return GetFullNameSafe(EntityManager->GetOwner());
}

FTransform FArcNiagaraRenderStateHelper::GetTransform() const
{
	return EntityManager->GetFragmentDataChecked<FTransformFragment>(EntityHandle).GetTransform();
}

const FBoxSphereBounds& FArcNiagaraRenderStateHelper::GetBounds() const
{
	return CachedBounds;
}

float FArcNiagaraRenderStateHelper::GetLastRenderTimeOnScreen() const
{
	return PrimitiveSceneData.GetLastRenderTimeOnScreen();
}

void FArcNiagaraRenderStateHelper::GetPrimitiveStats(FPrimitiveStats& /*PrimitiveStats*/) const
{
	// No meaningful stats for componentless Niagara primitive
}

UObject* FArcNiagaraRenderStateHelper::GetUObject()
{
	return EntityManager->GetOwner();
}

const UObject* FArcNiagaraRenderStateHelper::GetUObject() const
{
	return EntityManager->GetOwner();
}

void FArcNiagaraRenderStateHelper::PrecachePSOs()
{
	// Not implemented — PSO precaching is not required for componentless Niagara render state
}

UObject* FArcNiagaraRenderStateHelper::GetOwner() const
{
	return EntityManager->GetOwner();
}

FString FArcNiagaraRenderStateHelper::GetOwnerName() const
{
	return GetNameSafe(EntityManager->GetOwner());
}

FPrimitiveSceneProxy* FArcNiagaraRenderStateHelper::CreateSceneProxy()
{
	// Scene proxy creation is managed by CreateRenderState / CreateNiagaraRenderState.
	// This is called from the IPrimitiveComponent interface; return the existing proxy.
	return NiagaraProxy;
}

FRenderAssetOwnerStreamingState& FArcNiagaraRenderStateHelper::GetStreamingState() const
{
	return StreamingState;
}

ULevel* FArcNiagaraRenderStateHelper::GetComponentLevel() const
{
	return nullptr;
}

IPrimitiveComponent* FArcNiagaraRenderStateHelper::GetLODParentPrimitive() const
{
	return nullptr;
}

float FArcNiagaraRenderStateHelper::GetMinDrawDistance() const
{
	return 0.0f;
}

float FArcNiagaraRenderStateHelper::GetStreamingScale() const
{
	return 1.0f;
}

void FArcNiagaraRenderStateHelper::OnRenderAssetFirstLodChange(const UStreamableRenderAsset* /*RenderAsset*/, int32 /*FirstLodIndex*/)
{
}

UStreamableRenderAsset* FArcNiagaraRenderStateHelper::GetStreamableNaniteAsset() const
{
	return nullptr;
}

void FArcNiagaraRenderStateHelper::GetStreamableRenderAssetInfo(TArray<FStreamingRenderAssetPrimitiveInfo>& /*StreamableRenderAssets*/) const
{
}

void FArcNiagaraRenderStateHelper::GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& /*LevelContext*/, TArray<FStreamingRenderAssetPrimitiveInfo>& /*OutStreamingRenderAssets*/) const
{
}

#if WITH_EDITOR
HHitProxy* FArcNiagaraRenderStateHelper::CreateMeshHitProxy(int32 /*SectionIndex*/, int32 /*MaterialIndex*/)
{
	return nullptr;
}
#endif

HHitProxy* FArcNiagaraRenderStateHelper::CreatePrimitiveHitProxies(TArray<TRefCountPtr<HHitProxy>>& /*OutHitProxies*/)
{
	return nullptr;
}
