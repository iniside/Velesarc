// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPrimitiveRenderStateHelper.h"

#include "ArcDestroyRenderStateContext.h"
#include "ArcMassRenderLogs.h"
#include "Async/TaskGraphInterfaces.h"
#include "Mesh/ArcRenderStateHelperRegistry.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "PrimitiveComponentHelper.h"
#include "PrimitiveSceneDesc.h"
#include "PrimitiveSceneProxyDesc.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "UnrealEngine.h"
#include "Elements/Framework/TypedElementHandle.h"


#if WITH_EDITOR
#include "ObjectCacheEventSink.h"
#endif //WITH_EDITOR

//----------------------------------------------------------------------//
// FArcPrimitiveRenderStateHelper
//----------------------------------------------------------------------//
FArcPrimitiveRenderStateHelper::FArcPrimitiveRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment)
	: Super(InEntityHandle, InEntityManager)
{
	// Copying data from fragment as scene proxy decs take a view on this array
	CustomPrimitiveData = RenderPrimitiveFragment.CustomPrimitiveData;
}

FArcPrimitiveRenderStateHelper::~FArcPrimitiveRenderStateHelper()
{
	// Dangling-pointer guard: if any teardown path skips DestroyRenderState,
	// this ensures the raw pointer is removed from the registry before the object is freed.
	// Unregister is idempotent — safe to call even if DestroyRenderState already ran.
	ArcRenderStateHelperRegistry::Unregister(this);
}

const FArcRenderPrimitiveFragment& FArcPrimitiveRenderStateHelper::GetRenderPrimitiveFragment() const
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderPrimitiveFragment>(EntityHandle);
}

FArcRenderPrimitiveFragment& FArcPrimitiveRenderStateHelper::GetMutableRenderPrimitiveFragment()
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderPrimitiveFragment>(EntityHandle);
}

void FArcPrimitiveRenderStateHelper::ResetSceneProxyDescUnsupportedProperties()
{
	// Unsupported properties
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	//SceneProxyDesc.bLevelInstanceEditingState = false; //@Todo FastGeo will need to turn this off outside of this method
	//SceneProxyDesc.bSelectable = false; //@Todo FastGeo will need to turn this off outside of this method
	SceneProxyDesc.bUseEditorCompositing = false;
	SceneProxyDesc.bIsBeingMovedByEditor = false;
	//SceneProxyDesc.bSelected = false; //@Todo FastGeo will need to turn this off outside of this method
	//SceneProxyDesc.bIndividuallySelected = false; //@Todo FastGeo will need to turn this off outside of this method
	//SceneProxyDesc.bShouldRenderSelected = false; //@Todo FastGeo will need to turn this off outside of this method
	SceneProxyDesc.bWantsEditorEffects = false;
	SceneProxyDesc.bIsHiddenEd = false;
	SceneProxyDesc.bIsOwnerEditorOnly = false;
#if WITH_EDITOR
	SceneProxyDesc.bIsOwnedByFoliage = false;
#endif // WITH_EDITOR
	SceneProxyDesc.HiddenEditorViews = 0;
#if WITH_EDITOR
	SceneProxyDesc.OverlayColor = FColor(EForceInit::ForceInitToZero);
#endif // WITH_EDITOR
	SceneProxyDesc.Component = nullptr;

	// Properties that will be initialized by InitializeSceneProxyDescDynamicProperties
	SceneProxyDesc.ComponentId = FPrimitiveComponentId();
	SceneProxyDesc.StatId = TStatId();
	SceneProxyDesc.Owner = nullptr;
	SceneProxyDesc.World = nullptr;
	SceneProxyDesc.CustomPrimitiveData = nullptr;
	SceneProxyDesc.Scene = nullptr;
	SceneProxyDesc.PrimitiveComponentInterface = nullptr;
	SceneProxyDesc.FeatureLevel = ERHIFeatureLevel::Num;
	SceneProxyDesc.RuntimeVirtualTextures = TArrayView<URuntimeVirtualTexture*>();
	SceneProxyDesc.bIsVisible = false;
	SceneProxyDesc.bUsePSOPrecacheFallbackMaterial = false;
#if MESH_DRAW_COMMAND_STATS
	SceneProxyDesc.MeshDrawCommandStatsCategory = NAME_None;
#endif // MESH_DRAW_COMMAND_STATS
}

bool FArcPrimitiveRenderStateHelper::IsDrawnInGame() const
{
	// Drawn in game must consider both the component bIsVisible flag AND the bIsHidden flag (which actually originates from the actor bHiddenInGame property)
	// This logic mimics what is done to initialize FPrimitiveSceneProxy::DrawInGame
	return GetSceneProxyDesc().bIsVisible && !GetSceneProxyDesc().bIsHidden;
}

EComponentMobility::Type FArcPrimitiveRenderStateHelper::GetMobility() const
{
	return GetSceneProxyDesc().Mobility;
}

void FArcPrimitiveRenderStateHelper::UpdateVisibility()
{
	const FArcRenderPrimitiveFragment& RenderPrimitiveFragment = GetRenderPrimitiveFragment();

	// Update SceneProxyDesc.bIsVisible as it's dependant on component and component cluster visibility
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.bIsVisible = RenderPrimitiveFragment.bIsVisible;

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] UpdateVisibility: entity=[%d:%d] visible=%d hidden=%d",
		EntityHandle.Index, EntityHandle.SerialNumber, SceneProxyDesc.bIsVisible, SceneProxyDesc.bIsHidden);
}

void FArcPrimitiveRenderStateHelper::UpdateTransform()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisPrimitiveUpdateTransform);

	FArcRenderPrimitiveFragment& RenderPrimitiveFragment = GetMutableRenderPrimitiveFragment();

	FSceneInterface* Scene = GetScene();
	checkf(Scene, TEXT("Expecting a scene if this method is called"));

	SceneDesc.RenderMatrix = GetRenderMatrix();
	SceneDesc.AttachmentRootPosition = GetTransform().GetTranslation();
	SceneDesc.LocalBounds = RenderPrimitiveFragment.LocalBounds;
	SceneDesc.Bounds = GetBounds();

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] UpdateTransform: entity=[%d:%d] proxy=%p pushing bounds origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f)",
		EntityHandle.Index, EntityHandle.SerialNumber, GetSceneProxy(),
		SceneDesc.Bounds.Origin.X, SceneDesc.Bounds.Origin.Y, SceneDesc.Bounds.Origin.Z,
		SceneDesc.Bounds.BoxExtent.X, SceneDesc.Bounds.BoxExtent.Y, SceneDesc.Bounds.BoxExtent.Z);

	Scene->UpdatePrimitiveTransform(&SceneDesc);
}

#if WITH_EDITOR
void FArcPrimitiveRenderStateHelper::UpdateSelection()
{
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	if (PrimitiveSceneData.SceneProxy)
	{
		PrimitiveSceneData.SceneProxy->SetSelection_GameThread(SceneProxyDesc.ShouldRenderSelected());
	}
}

void FArcPrimitiveRenderStateHelper::UpdateLevelInstanceEditingState()
{
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	if (PrimitiveSceneData.SceneProxy)
	{
		PrimitiveSceneData.SceneProxy->SetLevelInstanceEditingState_GameThread(SceneProxyDesc.GetLevelInstanceEditingState());
	}
}
#endif// WITH_EDITOR

void FArcPrimitiveRenderStateHelper::InitializeSceneProxyDescDynamicProperties()
{
	check(GetWorld());
	check(GetScene());

	ResetSceneProxyDescUnsupportedProperties();

	// Initialize non-serialized properties
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.ComponentId = GetPrimitiveSceneId();
	UObject const* AdditionalStatObjectPtr = AdditionalStatObject();
	SceneProxyDesc.StatId = AdditionalStatObjectPtr ? AdditionalStatObjectPtr->GetStatID(true) : TStatId();
	SceneProxyDesc.Owner = GetOwner();
	SceneProxyDesc.World = GetWorld();
	SceneProxyDesc.CustomPrimitiveData = &CustomPrimitiveData;
	SceneProxyDesc.Scene = GetScene();
	SceneProxyDesc.PrimitiveComponentInterface = this;
	SceneProxyDesc.FeatureLevel = SceneProxyDesc.Scene->GetFeatureLevel();
	TConstArrayView<URuntimeVirtualTexture*> RuntimeVirtualTextures = GetRuntimeVirtualTextures();
	SceneProxyDesc.RuntimeVirtualTextures = (TArrayView<URuntimeVirtualTexture*>&)RuntimeVirtualTextures; // ugly cast, but just making it work for now. It would mean to change the SceneProxyDesc to fix this.
	SceneProxyDesc.bUsePSOPrecacheFallbackMaterial = UsePSOPrecacheFallbackMaterial();
#if MESH_DRAW_COMMAND_STATS
	static FName NAME_FastGeoPrimitiveComponent(TEXT("FastGeoPrimitiveComponent"));
	SceneProxyDesc.MeshDrawCommandStatsCategory = NAME_FastGeoPrimitiveComponent;
#endif // MESH_DRAW_COMMAND_STATS
	//check(SceneProxyDesc.bIsInstancedStaticMesh == (this->IsA<FFastGeoInstancedStaticMeshComponent>() || this->IsA<FFastGeoProceduralISMComponent>()));
	UpdateVisibility();
}

FPrimitiveSceneDesc FArcPrimitiveRenderStateHelper::BuildSceneDesc()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisBuildSceneDesc);

	FArcRenderPrimitiveFragment& RenderPrimitiveFragment = GetMutableRenderPrimitiveFragment();

	check(GetSceneProxy());

	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneDesc.SceneProxy = GetSceneProxy();
	SceneDesc.ProxyDesc = &SceneProxyDesc;
	SceneDesc.PrimitiveSceneData = &PrimitiveSceneData;
#if WITH_EDITOR
	SceneDesc.PrimitiveComponentInterface = this;
#endif
	SceneDesc.RenderMatrix = GetRenderMatrix();
	SceneDesc.AttachmentRootPosition = GetTransform().GetTranslation();
	SceneDesc.LocalBounds = RenderPrimitiveFragment.LocalBounds;
	SceneDesc.Bounds = GetBounds();
	SceneDesc.Mobility = SceneProxyDesc.Mobility;

	return SceneDesc;
}

void FArcPrimitiveRenderStateHelper::CreateRenderState(FRegisterComponentContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisCreateRenderState);

	if (IsRenderStateDelayed())
	{
		if (IsPSOPrecaching())
		{
			return;
		}
		// A force-completion path can advance the job set without resetting state, and this function requires None on entry.
		DestroyRenderState(nullptr);
	}

	MarkPrecachePSOsRequired();
	PrecachePSOs();

	//FWriteScopeLock WriteLock(*Lock.Get());
	check(ProxyState == EProxyCreationState::None);
	ProxyState = EProxyCreationState::Creating;
	bRenderStateDirty = false;

#if WITH_EDITOR
	NotifyRenderStateChanged();
#endif

	FSceneInterface* Scene = GetScene();
	check(Scene);

	UE::ArcMassRender::ESceneProxyCreationError Error = UE::ArcMassRender::ESceneProxyCreationError::None;
	if (FPrimitiveSceneProxy* SceneProxy = CreateSceneProxy(&Error))
	{
		//SceneProxy->SetPrimitiveColor(GetDebugColor());
		check(GetSceneProxy());
		FPrimitiveSceneDesc Desc = BuildSceneDesc();
		Scene->AddPrimitive(&Desc);

		ProxyState = EProxyCreationState::Created;
		ArcRenderStateHelperRegistry::Register(this, GetWorld());
#if WITH_EDITOR
		ArcRenderStateHelperRegistry::TrackProxyMaterials(this);
#endif
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] CreateRenderState: helper=%p entity=[%d:%d] proxy=%p created",
			this, EntityHandle.Index, EntityHandle.SerialNumber, SceneProxy);
	}
	else if (Error == UE::ArcMassRender::ESceneProxyCreationError::WaitingPSOs)
	{
		ProxyState = EProxyCreationState::Delayed;
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] CreateRenderState: helper=%p entity=[%d:%d] delayed (waiting PSOs)",
			this, EntityHandle.Index, EntityHandle.SerialNumber);
	}
	else
	{
		ProxyState = EProxyCreationState::None;
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] CreateRenderState: helper=%p entity=[%d:%d] FAILED (error=%d)",
			this, EntityHandle.Index, EntityHandle.SerialNumber, static_cast<int32>(Error));
	}
}

FString FArcPrimitiveRenderStateHelper::GetName() const
{
	return GetNameSafe(GetEntityManager().GetOwner());
}

FString FArcPrimitiveRenderStateHelper::GetFullName() const
{
	return GetFullNameSafe(GetEntityManager().GetOwner());
}

void FArcPrimitiveRenderStateHelper::DestroyRenderState(FArcDestroyRenderStateContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisDestroyRenderState);

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	// Invalidating the job set cancels an in-flight precache completion that would otherwise re-arm the rebuild on a torn-down helper.
	LatestPSOPrecacheJobSet++;
	LatestPSOPrecacheJobSetCompleted = LatestPSOPrecacheJobSet;
	bPendingPSOPrecacheRebuild = false;
#endif

	//FWriteScopeLock WriteLock(*Lock.Get());
	if (GetSceneProxy())
	{
		FSceneInterface* Scene = GetScene();
		check(Scene);

		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] DestroyRenderState: helper=%p entity=[%d:%d] proxy=%p batched=%d",
			this, EntityHandle.Index, EntityHandle.SerialNumber, GetSceneProxy(), Context != nullptr);

		check(ProxyState == EProxyCreationState::Created);

#if WITH_EDITOR
		// Before DestroyProxy so the untrack render command is enqueued ahead of the proxy's
		// scene removal (the unbatched fallback enqueues removal inside DestroyProxy).
		ArcRenderStateHelperRegistry::UntrackProxyMaterials(GetSceneProxy());
#endif

		FArcDestroyRenderStateContext::DestroyProxy(Context, GetSceneProxy(), Scene);

		PrimitiveSceneData.SceneProxy = nullptr;

		ProxyState = EProxyCreationState::None;

		ArcRenderStateHelperRegistry::Unregister(this);

		bRenderStateDirty = false;

#if WITH_EDITOR
		NotifyRenderStateChanged();
#endif
	}
	else
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] DestroyRenderState: helper=%p entity=[%d:%d] no proxy (state=%d)",
			this, EntityHandle.Index, EntityHandle.SerialNumber, static_cast<int32>(ProxyState));
		check(ProxyState == EProxyCreationState::None || ProxyState == EProxyCreationState::Delayed);
		ProxyState = EProxyCreationState::None;
	}
}

#if WITH_EDITOR
void FArcPrimitiveRenderStateHelper::NotifyRenderStateChanged()
{
	FObjectCacheEventSink::NotifyRenderStateChanged_Concurrent(this);
}
#endif

bool FArcPrimitiveRenderStateHelper::IsRenderStateDirty() const
{
	return Super::IsRenderStateDirty();
}

bool FArcPrimitiveRenderStateHelper::IsRenderStateCreated() const
{
	return Super::IsRenderStateCreated();
}

bool FArcPrimitiveRenderStateHelper::ShouldCreateRenderState() const
{
	const FArcRenderPrimitiveFragment& RenderPrimitiveFragment = GetRenderPrimitiveFragment();

	if (!FApp::CanEverRender())
	{
		return false;
	}

	// If the detail mode setting allows it, add it to the scene.
	const bool bDetailModeAllowsRendering = RenderPrimitiveFragment.DetailMode <= GetCachedScalabilityCVars().DetailMode;
	if (!bDetailModeAllowsRendering)
	{
		return false;
	}

	const FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	const bool bShouldCreateRenderState = (RenderPrimitiveFragment.bIsVisible && !SceneProxyDesc.bIsHidden) || SceneProxyDesc.bCastHiddenShadow || SceneProxyDesc.bAffectIndirectLightingWhileHidden || SceneProxyDesc.bRayTracingFarField;
	return bShouldCreateRenderState;
}

bool FArcPrimitiveRenderStateHelper::IsRegistered() const
{
	// If the Mass entity is created it is because the element is registered in the world/scene
	return true;
}

bool FArcPrimitiveRenderStateHelper::IsUnreachable() const
{
	const UObject* OwnerPtr = GetEntityManager().GetOwner();
	return !OwnerPtr || OwnerPtr->IsUnreachable();
}

bool FArcPrimitiveRenderStateHelper::IsStaticMobility() const
{
	return true;
}

bool FArcPrimitiveRenderStateHelper::IsMipStreamingForced() const
{
	return false;
}

UWorld* FArcPrimitiveRenderStateHelper::GetWorld() const
{
	return Super::GetWorld();
}

FSceneInterface* FArcPrimitiveRenderStateHelper::GetScene() const
{
	return Super::GetScene();
}

TConstArrayView<URuntimeVirtualTexture*> FArcPrimitiveRenderStateHelper::GetRuntimeVirtualTextures() const
{
	const FArcRenderPrimitiveFragment& RenderPrimitiveFragment = GetRenderPrimitiveFragment();
	return RenderPrimitiveFragment.RuntimeVirtualTextures;
}

bool FArcPrimitiveRenderStateHelper::IsFirstPersonRelevant() const
{
	return GetSceneProxyDesc().IsFirstPersonRelevant();
}

FPrimitiveComponentId FArcPrimitiveRenderStateHelper::GetPrimitiveSceneId() const
{
	return PrimitiveSceneData.PrimitiveSceneId;
}

FPrimitiveSceneProxy* FArcPrimitiveRenderStateHelper::GetSceneProxy() const
{
	return PrimitiveSceneData.SceneProxy;
}

void FArcPrimitiveRenderStateHelper::MarkRenderStateDirty()
{
	Super::MarkRenderStateDirty();

	// Engine systems treat IPrimitiveComponent::MarkRenderStateDirty as the universal "cached draw
	// data is stale — refresh it" hook (e.g. FShaderCompilingManager::PropagateMaterialChangesToPrimitives
	// after async shader compiles recreate material uniform buffers). The RenderStateDirty Mass signal
	// has no live consumer (UArcRenderStateDirtyUpdateProcessor is disabled), so without this the
	// Nanite shading bins keep a dangling MaterialUB pointer. Request an in-place static-mesh update:
	// mesh draw commands and Nanite material bins are re-cached next frame without recreating the proxy.
	// Deliberately outside the Super guard — bRenderStateDirty may already be set from an earlier call.
	// FIFO render-command ordering keeps the proxy alive: any later DestroyRenderState enqueues its
	// scene removal after this command.
	if (FPrimitiveSceneProxy* SceneProxy = GetSceneProxy())
	{
		if (FSceneInterface* Scene = GetScene())
		{
			ENQUEUE_RENDER_COMMAND(ArcRenderStateStaticMeshUpdate)(
				[Scene, SceneProxy](FRHICommandListImmediate&)
				{
					Scene->UpdateCachedRenderStates(SceneProxy);
				});
		}
	}
}

void FArcPrimitiveRenderStateHelper::DestroyRenderState()
{
	DestroyRenderState(nullptr);
}

FTransform FArcPrimitiveRenderStateHelper::GetTransform() const
{
	const FTransformFragment& TransformFragment = GetEntityManager().GetFragmentDataChecked<FTransformFragment>(EntityHandle);
	return TransformFragment.GetTransform();
}

const FBoxSphereBounds& FArcPrimitiveRenderStateHelper::GetBounds() const
{
	const FArcRenderPrimitiveFragment& RenderPrimitiveFragment = GetRenderPrimitiveFragment();
	return RenderPrimitiveFragment.WorldBounds;
}

void FArcPrimitiveRenderStateHelper::GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const
{
}

UObject* FArcPrimitiveRenderStateHelper::GetUObject()
{
	return GetEntityManager().GetOwner();
}

const UObject* FArcPrimitiveRenderStateHelper::GetUObject() const
{
	return GetEntityManager().GetOwner();
}

FMatrix FArcPrimitiveRenderStateHelper::GetRenderMatrix() const
{
	return GetTransform().ToMatrixWithScale();
}

float FArcPrimitiveRenderStateHelper::GetLastRenderTimeOnScreen() const
{
	return PrimitiveSceneData.GetLastRenderTimeOnScreen();
}

void FArcPrimitiveRenderStateHelper::SetCollisionEnabled(bool bInCollisionEnabled)
{
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.bCollisionEnabled = bInCollisionEnabled;
}

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
void FArcPrimitiveRenderStateHelper::OnPrecacheFinished(int32 JobSetThatJustCompleted)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PSOPrecacheFinishedTask);
	int32 CurrJobSetCompleted = LatestPSOPrecacheJobSetCompleted.load();
	while (CurrJobSetCompleted < JobSetThatJustCompleted && !LatestPSOPrecacheJobSetCompleted.compare_exchange_weak(CurrJobSetCompleted, JobSetThatJustCompleted)) {}

	if (JobSetThatJustCompleted < LatestPSOPrecacheJobSet)
	{
		return;
	}

	if (ProxyState == EProxyCreationState::Delayed)
	{
		bPendingPSOPrecacheRebuild = true;
		ProxyState = EProxyCreationState::None;
		return;
	}

	bPendingPSOPrecacheRebuild = true;
	MarkRenderStateDirty();
}

bool FArcPrimitiveRenderStateHelper::ConsumePendingPSOPrecacheRebuild()
{
	const bool bPending = bPendingPSOPrecacheRebuild;
	bPendingPSOPrecacheRebuild = false;
	return bPending;
}

void FArcPrimitiveRenderStateHelper::RequestRecreateRenderStateWhenPSOPrecacheFinished(const FGraphEventArray& PSOPrecacheCompileEvents)
{
	// If the proxy creation strategy relies on knowing when the precached PSO has been compiled,
	// schedule a task to mark the render state dirty when all PSOs are compiled so the proxy gets recreated.
	if (GetPSOPrecacheProxyCreationStrategy() != EPSOPrecacheProxyCreationStrategy::AlwaysCreate)
	{
		LatestPSOPrecacheJobSet++;
		if (PSOPrecacheCompileEvents.IsEmpty())
		{
			// Nothing to compile, so no completion task will ever fire: close the job set here, otherwise
			// IsPSOPrecaching stays true forever and a Delayed proxy is never created.
			LatestPSOPrecacheJobSetCompleted = LatestPSOPrecacheJobSet;
		}
		else
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady(
				[WeakThis = AsWeak(), JobSetThatCompletes = LatestPSOPrecacheJobSet]()
				{
					if (const TSharedPtr<FArcRenderStateHelper> Helper = WeakThis.Pin())
					{
						Helper->OnPrecacheFinished(JobSetThatCompletes);
					}
				}
				, TStatId()
				, &PSOPrecacheCompileEvents
				, ENamedThreads::GameThread);
		}
		MarkRenderStateDirty();
	}
	bPSOPrecacheCalled = true;
}

void FArcPrimitiveRenderStateHelper::SetupPrecachePSOParams(FPSOPrecacheParams& Params)
{
	const FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	//auto IsPrecomputedLightingValid = []() { return false; };
	Params.bRenderInMainPass = SceneProxyDesc.bRenderInMainPass;
	Params.bRenderInDepthPass = SceneProxyDesc.bRenderInDepthPass;
	Params.bStaticLighting = SceneProxyDesc.bHasStaticLighting;
	Params.bUsesIndirectLightingCache = Params.bStaticLighting && SceneProxyDesc.IndirectLightingCacheQuality != ILCQ_Off;// && (!IsPrecomputedLightingValid() || SceneProxyDesc.LightmapType == ELightmapType::ForceVolumetric);
	Params.bAffectDynamicIndirectLighting = SceneProxyDesc.bAffectDynamicIndirectLighting;
	Params.bCastShadow = SceneProxyDesc.CastShadow;
	// Custom depth can be toggled at runtime with PSO precache call so assume it might be needed when depth pass is needed
	// Ideally precache those with lower priority and don't wait on these (UE-174426)
	Params.bRenderCustomDepth = SceneProxyDesc.bRenderInDepthPass;
	Params.bCastShadowAsTwoSided = SceneProxyDesc.bCastShadowAsTwoSided;
	Params.SetMobility(SceneProxyDesc.Mobility);
	Params.SetStencilWriteMask(FRendererStencilMaskEvaluation::ToStencilMask(SceneProxyDesc.CustomDepthStencilWriteMask));

	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials(UsedMaterials);
	for (const UMaterialInterface* MaterialInterface : UsedMaterials)
	{
		if (MaterialInterface && MaterialInterface->IsUsingWorldPositionOffset_Concurrent(GMaxRHIShaderPlatform))
		{
			Params.bAnyMaterialHasWorldPositionOffset = true;
			break;
		}
	}
}

#endif

bool FArcPrimitiveRenderStateHelper::IsPSOPrecaching() const
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	// Consider as precaching when marked as required to do PSOs precaching (even if task has not been launched yet)
	return bPSOPrecacheRequired || (LatestPSOPrecacheJobSetCompleted != LatestPSOPrecacheJobSet);
#else
	return false;
#endif
}

bool FArcPrimitiveRenderStateHelper::UsePSOPrecacheFallbackMaterial() const
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	return IsPSOPrecaching() && GetPSOPrecacheProxyCreationStrategy() == EPSOPrecacheProxyCreationStrategy::UseFallbackMaterialUntilPSOPrecached;
#else
	return false;
#endif
}

bool FArcPrimitiveRenderStateHelper::CheckPSOPrecachingAndBoostPriority(EPSOPrecachePriority NewPSOPrecachePriority)
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	bool bPrecacheStillRunning = IsPSOPrecaching();

	ensure(!IsComponentPSOPrecachingEnabled() || bPSOPrecacheCalled || bPSOPrecacheRequired);
	check(NewPSOPrecachePriority == EPSOPrecachePriority::High || NewPSOPrecachePriority == EPSOPrecachePriority::Highest);

	if (bPrecacheStillRunning && PSOPrecacheRequestPriority < NewPSOPrecachePriority)
	{
		// Only boost PSO priority if PSO task was started
		if (LatestPSOPrecacheJobSetCompleted != LatestPSOPrecacheJobSet)
		{
			BoostPSOPriority(NewPSOPrecachePriority, MaterialPSOPrecacheRequestIDs);
		}
		PSOPrecacheRequestPriority = NewPSOPrecachePriority;
	}
	return bPrecacheStillRunning;
#else
	return false;
#endif
}


void FArcPrimitiveRenderStateHelper::MarkPrecachePSOsRequired()
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	if (bPSOPrecacheCalled || !FApp::CanEverRender() || !IsComponentPSOPrecachingEnabled())
	{
		return;
	}
	bPSOPrecacheRequired = true;
	PSOPrecacheRequestPriority = EPSOPrecachePriority::Medium;
#endif
}

void FArcPrimitiveRenderStateHelper::PrecachePSOs()
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	if (!bPSOPrecacheRequired)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FArcPrimitiveRenderStateHelper::PrecachePSOs);

	// Clear the current request data
	MaterialPSOPrecacheRequestIDs.Empty();
	PSOPrecacheRequestPriority = EPSOPrecachePriority::Medium;

	// Collect the data from the derived classes
	FPSOPrecacheParams PSOPrecacheParams;
	SetupPrecachePSOParams(PSOPrecacheParams);
	FMaterialInterfacePSOPrecacheParamsList PSOPrecacheDataArray;
	CollectPSOPrecacheData(PSOPrecacheParams, PSOPrecacheDataArray);
	// Set priority
	for (FMaterialInterfacePSOPrecacheParams& Params : PSOPrecacheDataArray)
	{
		Params.Priority = PSOPrecacheRequestPriority;
	}

	FGraphEventArray GraphEvents;
	PrecacheMaterialPSOs(PSOPrecacheDataArray, MaterialPSOPrecacheRequestIDs, GraphEvents);

	RequestRecreateRenderStateWhenPSOPrecacheFinished(GraphEvents);
	bPSOPrecacheRequired = false;
#endif
}

UObject* FArcPrimitiveRenderStateHelper::GetOwner() const
{
	return GetEntityManager().GetOwner();
}

FString FArcPrimitiveRenderStateHelper::GetOwnerName() const
{
	return GetNameSafe(GetEntityManager().GetOwner());
}

FPrimitiveSceneProxy* FArcPrimitiveRenderStateHelper::CreateSceneProxy()
{
	return CreateSceneProxy(/*OutError*/nullptr);
}

//
// Functions below here added for use in streamer, which is not used by FastGeo. These functions may not get used.
static FRenderAssetOwnerStreamingState GFastGeoStreamingStateStub;
FRenderAssetOwnerStreamingState& FArcPrimitiveRenderStateHelper::GetStreamingState() const
{
	return GFastGeoStreamingStateStub;
}

ULevel* FArcPrimitiveRenderStateHelper::GetComponentLevel() const
{
	return nullptr;
}

IPrimitiveComponent* FArcPrimitiveRenderStateHelper::GetLODParentPrimitive() const
{
	return nullptr;
}

float FArcPrimitiveRenderStateHelper::GetMinDrawDistance() const
{
	return 0.0f;
}

float FArcPrimitiveRenderStateHelper::GetStreamingScale() const
{
	return 1.0f;
}

void FArcPrimitiveRenderStateHelper::OnRenderAssetFirstLodChange(const UStreamableRenderAsset* RenderAsset, int32 FirstLodIndex)
{
}

UStreamableRenderAsset* FArcPrimitiveRenderStateHelper::GetStreamableNaniteAsset() const
{
	return nullptr;
}

void FArcPrimitiveRenderStateHelper::GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const
{
}

#if WITH_EDITOR
HHitProxy* FArcPrimitiveRenderStateHelper::CreateMeshHitProxy(int32 SectionIndex, int32 MaterialIndex)
{
	if (FArcTypeElementFragment* TypeElementFragment = GetEntityManager().GetFragmentDataPtr<FArcTypeElementFragment>(EntityHandle))
	{
		return new HArcTypeElementHandleHitProxy(TypeElementFragment->InstanceId, MaterialIndex, SectionIndex);
	}
	return nullptr;
}
#endif // WITH_EDITOR

HHitProxy* FArcPrimitiveRenderStateHelper::CreatePrimitiveHitProxies(TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
#if WITH_EDITOR
	if (FArcTypeElementFragment* TypeElementFragment = GetEntityManager().GetFragmentDataPtr<FArcTypeElementFragment>(EntityHandle))
	{
		HArcTypeElementHandleHitProxy* HitProxy = new HArcTypeElementHandleHitProxy(TypeElementFragment->InstanceId);
		OutHitProxies.Add(HitProxy);
		return HitProxy;
	}
#endif // WITH_EDITOR
	return nullptr;
}

FPrimitiveMaterialPropertyDescriptor FArcPrimitiveRenderStateHelper::GetUsedMaterialPropertyDesc(EShaderPlatform InShaderPlatform) const
{
	return FPrimitiveComponentHelper::GetUsedMaterialPropertyDesc(*this, InShaderPlatform);
}
