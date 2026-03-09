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
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"

// ---------------------------------------------------------------------------
// Shared anchor component — a single unregistered USceneComponent shared by
// all FArcNiagaraRenderStateHelper instances. Prevents the null-AttachComponent
// check in FNiagaraSystemInstance::Tick_GameThread from calling Complete().
// Safe to share because ManualTick is synchronous (game thread) — each entity
// sets the transform on this component right before its own tick.
// ---------------------------------------------------------------------------

static USceneComponent* GetSharedAnchorComponent()
{
	static TStrongObjectPtr<USceneComponent> SharedAnchor;
	if (!SharedAnchor.IsValid())
	{
		SharedAnchor = TStrongObjectPtr<USceneComponent>(NewObject<USceneComponent>(GetTransientPackage()));
	}
	return SharedAnchor.Get();
}

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
		/*OverrideParameters=*/ &OverrideParameters,
		/*AttachComponent=*/ GetSharedAnchorComponent(),
		ENiagaraTickBehavior::UsePrereqs,
		/*bPooled=*/ false,
		/*RandomSeedOffset=*/ 0,
		/*bForceSolo=*/ true,  // Must be solo: Tick_GameThread calls Complete() when AttachComponent is null
		/*WarmupTickCount=*/ 0,
		/*WarmupTickDelta=*/ 0.f
	);

	Controller->Activate(FNiagaraSystemInstance::EResetMode::ResetAll);

	// Sync render time — without this, EngineTimeSinceRendered is huge and
	// systems with scalability/LOD settings may throttle or cull themselves.
	Controller->SetLastRenderTime(World.GetTimeSeconds());
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
	CachedScene = Scene;

	// Set the system instance's world transform before creating the proxy
	// so that renderers created during proxy construction have the correct position.
	if (FNiagaraSystemInstance* SystemInstance = Controller->GetSystemInstance_Unsafe())
	{
		SystemInstance->SetWorldTransform(GetTransform());
	}

	// Fill the Niagara-specific proxy descriptor
	ProxyDesc.SystemAsset = SystemAsset.Get();
	ProxyDesc.SystemInstanceController = Controller;
	ProxyDesc.OcclusionQueryMode = ENiagaraOcclusionQueryMode::Default;

	// Base FPrimitiveSceneProxyDesc fields
	ProxyDesc.Mobility = EComponentMobility::Movable;
	ProxyDesc.bCastDynamicShadow = bCastShadowCached;
	ProxyDesc.bIsVisible = true;
	ProxyDesc.bRenderInMainPass = true;
	ProxyDesc.bRenderInDepthPass = true;
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

		// Use CachedScene for reliable cleanup — WeakWorld may be invalid during teardown
		FSceneInterface* Scene = CachedScene ? CachedScene : GetScene();
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
		CachedScene = nullptr;
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
		UE_LOG(LogMass, Warning, TEXT("ArcNiagara: Proxy has no RenderData!"));
		return;
	}

	FNiagaraSystemInstance* SystemInstance = Controller->GetSystemInstance_Unsafe();

	// Sync render time so the system knows it's being rendered
	if (SystemInstance && WeakWorld.IsValid())
	{
		Controller->SetLastRenderTime(WeakWorld->GetTimeSeconds());
	}

	// Solo systems are not ticked by the world manager — we must tick them manually.
	// Set the shared anchor's transform to THIS entity's transform right before ticking.
	// TickInstanceParameters_GameThread reads AttachComponent->GetComponentTransform().
	// Safe because ManualTick is synchronous on the game thread.
	if (SystemInstance && SystemInstance->IsSolo() && WeakWorld.IsValid())
	{
		const FTransform& CurrentTransform = EntityManager->GetFragmentDataChecked<FTransformFragment>(EntityHandle).GetTransform();
		GetSharedAnchorComponent()->SetWorldTransform(CurrentTransform);
		Controller->ManualTick(WeakWorld->GetDeltaSeconds(), nullptr);
	}

	// Diagnostic: log system state once per second
	static double LastLogTime = 0.0;
	if (WeakWorld.IsValid() && WeakWorld->GetTimeSeconds() - LastLogTime > 1.0)
	{
		LastLogTime = WeakWorld->GetTimeSeconds();
		const int32 NumRenderers = ProxyRenderData->GetNumRenderers();
		UE_LOG(LogMass, Log, TEXT("ArcNiagara: System=%s Renderers=%d RenderingEnabled=%d IsSolo=%d"),
			*GetNameSafe(SystemAsset.Get()),
			NumRenderers,
			ProxyRenderData->IsRenderingEnabled_GT() ? 1 : 0,
			SystemInstance ? (SystemInstance->IsSolo() ? 1 : 0) : -1);
		if (SystemInstance)
		{
			UE_LOG(LogMass, Log, TEXT("ArcNiagara: ExecState=%d TickCount=%d NumEmitters=%d Transform=%s"),
				static_cast<int32>(SystemInstance->GetActualExecutionState()),
				SystemInstance->GetTickCount(),
				SystemInstance->GetEmitters().Num(),
				*SystemInstance->GetWorldTransform().GetTranslation().ToString());
			// Log per-emitter particle counts
			for (int32 i = 0; i < SystemInstance->GetEmitters().Num(); ++i)
			{
				const FNiagaraEmitterInstance& EmitterInst = SystemInstance->GetEmitters()[i].Get();
				bool bIsComplete = EmitterInst.IsComplete() ? 1 : 0;
				int32 nump = EmitterInst.GetNumParticles();
				bool bIsInittialized = EmitterInst.GetParticleData().IsInitialized() ? 1 : 0;
				
				UE_LOG(LogMass, Log, TEXT("ArcNiagara:   Emitter[%d] Particles=%d IsComplete=%d IsDisabled=%d DataInit=%d"),
					i,
					EmitterInst.GetNumParticles(),
					EmitterInst.IsComplete() ? 1 : 0,
					EmitterInst.IsDisabled() ? 1 : 0,
					EmitterInst.GetParticleData().IsInitialized() ? 1 : 0);
			}
		}
	}

	// PostTickRenderers MUST be called BEFORE GenerateSetDynamicDataCommands.
	// This matches the NiagaraComponent flow: PostSystemTick_GameThread runs first
	// (which calls PostTickRenderers), then SendRenderDynamicData_Concurrent runs later
	// (which calls GenerateSetDynamicDataCommands). Some renderers prepare sorted
	// indices and other data in PostTick that GenerateDynamicData relies on.
	Controller->PostTickRenderers(*ProxyRenderData);

	FNiagaraSceneProxy::FDynamicData NewProxyDynamicData;
	if (SystemInstance)
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
	// correct location. The shared anchor component transform is synced in
	// SendDynamicRenderData() right before ManualTick.
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
