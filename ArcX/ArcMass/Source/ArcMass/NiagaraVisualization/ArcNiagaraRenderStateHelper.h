// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ComponentInterfaces.h"
#include "MassEntityHandle.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSceneProxy.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "PrimitiveSceneDesc.h"
#include "PrimitiveSceneInfoData.h"
#include "UObject/StrongObjectPtr.h"

struct FMassEntityManager;
class UNiagaraSystem;
class USceneComponent;
struct FTransformFragment;

/**
 * Niagara proxy desc that doesn't require a UPrimitiveComponent for GetUsedMaterials.
 * Delegates to IPrimitiveComponent interface instead.
 */
struct FArcNiagaraSceneProxyDesc : public FNiagaraSceneProxyDesc
{
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override
	{
		if (PrimitiveComponentInterface)
		{
			PrimitiveComponentInterface->GetUsedMaterials(OutMaterials, bGetDebugMaterials);
		}
	}
};

/**
 * Component-less Niagara render state for Mass entities.
 * Implements IPrimitiveComponent directly to register a FNiagaraSceneProxy
 * with the scene without requiring any UActorComponent.
 *
 * Pattern mirrors FMassPrimitiveRenderStateHelper (MassEngine, private)
 * but is self-contained — no inheritance from engine private headers.
 */
struct ARCMASS_API FArcNiagaraRenderStateHelper : public IPrimitiveComponent
{
public:
	FArcNiagaraRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager);
	virtual ~FArcNiagaraRenderStateHelper();

	/** Initialize the Niagara system instance. Must be called before CreateRenderState. */
	void Initialize(UWorld& World, UNiagaraSystem& System, bool bCastShadow);

	/** Create scene proxy and register with the renderer. */
	void CreateNiagaraRenderState();

	/** Remove from scene and destroy proxy. */
	void DestroyNiagaraRenderState();

	/** Push dynamic render data to the scene proxy (call per frame). */
	void SendDynamicRenderData();

	/** Update scene transform from entity's FTransformFragment. */
	void UpdateTransform();

	bool IsInitialized() const { return Controller.IsValid(); }
	bool HasSceneProxy() const { return NiagaraProxy != nullptr; }

	FNiagaraSystemInstanceControllerPtr GetController() const { return Controller; }

	//~ Begin IPrimitiveComponent interface
	virtual bool IsRenderStateCreated() const override;
	virtual bool IsRenderStateDirty() const override;
	virtual bool ShouldCreateRenderState() const override;
	virtual bool IsRegistered() const override;
	virtual bool IsUnreachable() const override;
	virtual bool IsStaticMobility() const override;
	virtual bool IsMipStreamingForced() const override;
	virtual UWorld* GetWorld() const override;
	virtual FSceneInterface* GetScene() const override;
	virtual FPrimitiveSceneProxy* GetSceneProxy() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void MarkRenderStateDirty() override;
	virtual void DestroyRenderState() override;
	virtual void CreateRenderState(FRegisterComponentContext* Context) override;
	virtual FString GetName() const override;
	virtual FString GetFullName() const override;
	virtual FTransform GetTransform() const override;
	virtual const FBoxSphereBounds& GetBounds() const override;
	virtual float GetLastRenderTimeOnScreen() const override;
	virtual void GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const override;
	virtual UObject* GetUObject() override;
	virtual const UObject* GetUObject() const override;
	virtual void PrecachePSOs() override;
	virtual UObject* GetOwner() const override;
	virtual FString GetOwnerName() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FRenderAssetOwnerStreamingState& GetStreamingState() const override;
	virtual ULevel* GetComponentLevel() const override;
	virtual IPrimitiveComponent* GetLODParentPrimitive() const override;
	virtual float GetMinDrawDistance() const override;
	virtual float GetStreamingScale() const override;
	virtual void OnRenderAssetFirstLodChange(const UStreamableRenderAsset* RenderAsset, int32 FirstLodIndex) override;
	virtual UStreamableRenderAsset* GetStreamableNaniteAsset() const override;
	virtual void GetStreamableRenderAssetInfo(TArray<FStreamingRenderAssetPrimitiveInfo>& StreamableRenderAssets) const override;
	virtual void GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const override;
#if WITH_EDITOR
	virtual HHitProxy* CreateMeshHitProxy(int32 SectionIndex, int32 MaterialIndex) override;
#endif
	virtual HHitProxy* CreatePrimitiveHitProxies(TArray<TRefCountPtr<HHitProxy>>& OutHitProxies) override;
	//~ End IPrimitiveComponent interface

private:
	FMassEntityHandle EntityHandle;
	TSharedRef<FMassEntityManager> EntityManager;

	// Niagara system ownership
	FNiagaraSystemInstanceControllerPtr Controller;
	FNiagaraUserRedirectionParameterStore OverrideParameters;
	TStrongObjectPtr<USceneComponent> AnchorComponent; // Lightweight transform anchor — prevents Tick_GameThread from calling Complete()
	FNiagaraSceneProxy* NiagaraProxy = nullptr;
	FArcNiagaraSceneProxyDesc ProxyDesc;

	// Scene integration
	FPrimitiveSceneInfoData PrimitiveSceneData{};
	FCustomPrimitiveData CustomPrimitiveData;
	FBoxSphereBounds CachedBounds;
	mutable FRenderAssetOwnerStreamingState StreamingState;

	TWeakObjectPtr<UNiagaraSystem> SystemAsset;
	TWeakObjectPtr<UWorld> WeakWorld;
	FSceneInterface* CachedScene = nullptr; // Raw pointer for cleanup — outlives WeakWorld during teardown
	bool bRenderStateDirty = false;
	bool bCastShadowCached = false;
};
