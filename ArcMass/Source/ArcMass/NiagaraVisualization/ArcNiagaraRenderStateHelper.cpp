// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNiagaraRenderStateHelper.h"

#include "ArcNiagaraVisFragments.h"
#include "MassEntityFragments.h"
#include "MassEntityManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSceneProxy.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemRenderData.h"
#include "NiagaraTypes.h"
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

/** Orphaned helpers — systems whose entities have been destroyed but whose
 *  particles are still fading out. Ticked each frame by TickOrphanedSystems(). */
static TArray<FArcNiagaraRenderStateHelper*> OrphanedHelpers;

/** ALL living helpers (active + orphaned). Used by OnWorldCleanup to strip
 *  scene proxies from active helpers before FScene::~FScene asserts. */
static TSet<FArcNiagaraRenderStateHelper*> AllHelpers;

// ---------------------------------------------------------------------------
// World cleanup — removes all orphaned helpers (and their scene proxies)
// before the FScene destructor asserts that no primitives remain.
// ---------------------------------------------------------------------------

static bool bWorldCleanupRegistered = false;

static void OnWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	// Phase 1: Delete orphaned helpers for this world (nobody else will tick/clean them).
	for (int32 i = OrphanedHelpers.Num() - 1; i >= 0; --i)
	{
		FArcNiagaraRenderStateHelper* Helper = OrphanedHelpers[i];
		if (Helper->GetWorld() == World || Helper->GetWorld() == nullptr)
		{
			delete Helper; // Destructor calls DestroyNiagaraRenderState → Scene->RemovePrimitive
			OrphanedHelpers.RemoveAtSwap(i);
		}
	}

	// Phase 2: Destroy render state of all ACTIVE helpers for this world.
	// Entities still own these helpers — we just remove the scene proxy
	// before FScene::~FScene() asserts that all primitives are gone.
	// The helpers survive; when the entity is eventually destroyed, the fragment
	// destructor calls Orphan() which detects the dead world and deletes the helper.
	for (FArcNiagaraRenderStateHelper* Helper : AllHelpers)
	{
		if (Helper->GetWorld() == World && Helper->HasSceneProxy())
		{
			Helper->DestroyNiagaraRenderState();
		}
	}
}

static void EnsureWorldCleanupRegistered()
{
	if (!bWorldCleanupRegistered)
	{
		FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
		bWorldCleanupRegistered = true;
	}
}

// ---------------------------------------------------------------------------
// FArcNiagaraVisFragment — destructor & move (must be in a .cpp that sees the full type)
// ---------------------------------------------------------------------------

FArcNiagaraVisFragment::~FArcNiagaraVisFragment()
{
	if (RenderStateHelper)
	{
		// Don't destroy immediately — orphan the helper so existing particles
		// can finish their lifetime. TickOrphanedSystems() manages cleanup.
		RenderStateHelper->Orphan();
		RenderStateHelper = nullptr; // Fragment no longer owns it
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
	AllHelpers.Add(this);
}

FArcNiagaraRenderStateHelper::~FArcNiagaraRenderStateHelper()
{
	AllHelpers.Remove(this);

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
	EnsureWorldCleanupRegistered();

	WeakWorld = &World;
	SystemAsset = &System;
	bCastShadowCached = bCastShadow;
	CachedTransform = EntityManager->GetFragmentDataChecked<FTransformFragment>(EntityHandle).GetTransform();

#if WITH_EDITOR
	// In editor, Niagara systems may not be compiled until opened in the Niagara editor.
	// Force a synchronous compile so the system is ready to spawn particles.
	System.WaitForCompilationComplete(/*bIncludingGPUShaders=*/ true, /*bShowProgress=*/ false);
#endif

	if (!System.IsReadyToRun())
	{
		UE_LOG(LogMass, Warning, TEXT("ArcNiagara: System %s is not ready to run"), *GetNameSafe(&System));
		return;
	}

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

void FArcNiagaraRenderStateHelper::SendDynamicRenderData(const FTransform& Transform)
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
		GetSharedAnchorComponent()->SetWorldTransform(Transform);
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

void FArcNiagaraRenderStateHelper::UpdateTransform(const FTransform& Transform)
{
	CachedTransform = Transform;

	FSceneInterface* Scene = GetScene();
	if (!Scene || !NiagaraProxy || !Controller.IsValid())
	{
		return;
	}

	// Update the Niagara system instance's world transform so particles spawn at the
	// correct location. The shared anchor component transform is synced in
	// SendDynamicRenderData() right before ManualTick.
	if (FNiagaraSystemInstance* SystemInstance = Controller->GetSystemInstance_Unsafe())
	{
		SystemInstance->SetWorldTransform(Transform);
	}

	// Update cached bounds
	CachedBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(100.0f), 100.0f).TransformBy(Transform);

	FPrimitiveSceneDesc Desc;
	Desc.SceneProxy = NiagaraProxy;
	Desc.ProxyDesc = &ProxyDesc;
	Desc.PrimitiveSceneData = &PrimitiveSceneData;
	Desc.RenderMatrix = Transform.ToMatrixWithScale();
	Desc.AttachmentRootPosition = Transform.GetTranslation();
	Desc.LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(100.0f), 100.0f);
	Desc.Bounds = CachedBounds;
	Desc.Mobility = EComponentMobility::Movable;

	Scene->UpdatePrimitiveTransform(&Desc);
}

// ---------------------------------------------------------------------------
// Orphaned system lifecycle
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::Orphan()
{
	if (bOrphaned)
	{
		return;
	}
	bOrphaned = true;

	// If world is tearing down (PIE exit, level transition), skip graceful fadeout.
	// Destroy immediately so the scene proxy is removed before FScene::~FScene asserts.
	// The caller (fragment destructor) sets its pointer to nullptr right after this returns.
	if (!WeakWorld.IsValid() || WeakWorld->bIsTearingDown)
	{
		delete this; // Destructor handles proxy + controller cleanup
		return;
	}

	// If the system was never fully initialized, nothing to fade out
	if (!Controller.IsValid())
	{
		OrphanedHelpers.Add(this);
		return;
	}

	// Stop spawning new particles, let existing ones finish their lifetime.
	// Deactivate(false) sets the system to Inactive state: scripts still run,
	// particles still simulate and render, but no new particles are spawned.
	Controller->Deactivate(false);

	OrphanedHelpers.Add(this);
}

void FArcNiagaraRenderStateHelper::TickOrphanedSystems(UWorld& World)
{
	for (int32 i = OrphanedHelpers.Num() - 1; i >= 0; --i)
	{
		FArcNiagaraRenderStateHelper* Helper = OrphanedHelpers[i];

		FNiagaraSystemInstance* SystemInstance = Helper->Controller.IsValid()
			? Helper->Controller->GetSystemInstance_Unsafe()
			: nullptr;

		// Clean up if: no valid system, all particles dead, or world is tearing down
		if (!SystemInstance || SystemInstance->IsComplete() || !Helper->WeakWorld.IsValid())
		{
			delete Helper; // Destructor handles proxy + controller cleanup
			OrphanedHelpers.RemoveAtSwap(i);
			continue;
		}

		// Tick and render using the last known transform
		Helper->SendDynamicRenderData(Helper->CachedTransform);
	}
}

int32 FArcNiagaraRenderStateHelper::GetOrphanedCount()
{
	return OrphanedHelpers.Num();
}

// ---------------------------------------------------------------------------
// Parameter overrides
// ---------------------------------------------------------------------------

void FArcNiagaraRenderStateHelper::ApplyParameterOverrides(const FArcNiagaraVisParamsFragment& Params)
{
	if (!Controller.IsValid())
	{
		return;
	}

	// --- Scalar types ---

	for (const auto& [Name, Value] : Params.FloatParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(), Name);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value), Var, /*bAdd=*/ true);
	}

	for (const auto& [Name, Value] : Params.IntParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetIntDef(), Name);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value), Var, /*bAdd=*/ true);
	}

	// Niagara bools are stored as FNiagaraBool (int32: True = INDEX_NONE, False = 0)
	for (const auto& [Name, Value] : Params.BoolParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetBoolDef(), Name);
		const FNiagaraBool NiagaraValue(Value);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&NiagaraValue), Var, /*bAdd=*/ true);
	}

	// --- Vector types (double → single precision conversion) ---

	for (const auto& [Name, Value] : Params.Vector2DParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec2Def(), Name);
		const FVector2f Value2f(Value);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value2f), Var, /*bAdd=*/ true);
	}

	for (const auto& [Name, Value] : Params.VectorParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec3Def(), Name);
		const FVector3f Value3f(Value);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value3f), Var, /*bAdd=*/ true);
	}

	for (const auto& [Name, Value] : Params.Vector4Params)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec4Def(), Name);
		const FVector4f Value4f(Value);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value4f), Var, /*bAdd=*/ true);
	}

	// --- Color & rotation ---

	// FLinearColor layout (4 floats) matches Niagara's color type directly
	for (const auto& [Name, Value] : Params.ColorParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetColorDef(), Name);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value), Var, /*bAdd=*/ true);
	}

	for (const auto& [Name, Value] : Params.QuatParams)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetQuatDef(), Name);
		const FQuat4f Value4f(Value);
		OverrideParameters.SetParameterData(reinterpret_cast<const uint8*>(&Value4f), Var, /*bAdd=*/ true);
	}
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
	if (bOrphaned)
	{
		return CachedTransform;
	}
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
