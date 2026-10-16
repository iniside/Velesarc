// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"
#include "InstancedStaticMeshSceneProxyDesc.h"
#include "PrimitiveSceneDesc.h"
#include "PrimitiveSceneInfoData.h"
#include "SceneTypes.h"
#include "UObject/StrongObjectPtr.h"

class FPrimitiveSceneProxy;
class FRDGBuilder;
class FSceneInterface;
class UMaterialInterface;
class UStaticMesh;
class UWorld;
struct FGPUSceneWriteDelegateParams;

/**
 * A componentless instanced-static-mesh primitive whose instance data lives only in GPUScene.
 * Transforms are produced by a compute shader through UpdateInstancesGPU; there is no CPU instance array.
 */
class ARCECOTOPE_API FArcEcotopeFoliagePrimitive
{
public:
	using FGpuInstanceUpdateFunction = TFunction<void(FRDGBuilder&, const FGPUSceneWriteDelegateParams&)>;

	FArcEcotopeFoliagePrimitive() = default;
	~FArcEcotopeFoliagePrimitive();

	FArcEcotopeFoliagePrimitive(const FArcEcotopeFoliagePrimitive&) = delete;
	FArcEcotopeFoliagePrimitive& operator=(const FArcEcotopeFoliagePrimitive&) = delete;

	/** InPrimitiveUObject must be an object of InWorld — the renderer compares its PIE prefix against the world's. */
	bool Initialize(
		UWorld* InWorld,
		UObject* InPrimitiveUObject,
		UStaticMesh* InMesh,
		TConstArrayView<TObjectPtr<UMaterialInterface>> InMaterialOverrides,
		const FArcEcotopeFoliageRenderSettings& InRenderSettings);

	void SetTransformAndBounds(const FTransform& InTransform, const FBoxSphereBounds& InLocalBounds);

	/**
	 * Recreates the proxy when NumRequiredInstances exceeds the allocated span, and returns that span.
	 * It stays at its high-water mark on a shrink, so a writer must size its dispatch from the return
	 * value and hide the tail past the live count.
	 *
	 * A non-positive request removes the primitive from the scene and returns zero.
	 */
	int32 EnsureInstanceAllocation(int32 NumRequiredInstances);

	void UpdateInstancesGPU(int32 NumRequiredInstances, FGpuInstanceUpdateFunction UpdateFunction);

	void RemoveFromScene();

	FPrimitiveSceneProxy* GetSceneProxy() const { return PrimitiveSceneData.SceneProxy; }
	int32 GetNumInstances() const { return NumInstances; }
	int32 GetNumAllocatedInstances() const { return NumAllocatedInstances; }
	const FTransform& GetTransform() const { return Transform; }

	/** Detaches from the scene and defers the delete to the render thread, which the proxy teardown requires. */
	static void Release(TUniquePtr<FArcEcotopeFoliagePrimitive>&& InPrimitive);

private:
	void RecreateProxy();
	FPrimitiveSceneProxy* CreateSceneProxy();

	/**
	 * Fills RenderSafeMaterials with the effective material per slot, substituting the default surface
	 * material wherever that material is not IsComplete(), and kicks its compile in editor. A
	 * componentless proxy has no component to kick the compile for it and none to carry an incomplete
	 * material safely: docs/reference/componentless-proxy-material-compilation.md.
	 */
	void BuildRenderSafeMaterials();

	void FillProxyDesc();
	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> BuildInstanceData();
	void AddToScene(FPrimitiveSceneProxy* SceneProxy);

	FSceneInterface* Scene = nullptr;
	UWorld* World = nullptr;
	TWeakObjectPtr<UObject> PrimitiveUObject;
	TStrongObjectPtr<UStaticMesh> MeshRef;

	/** Roots the overrides; a TObjectPtr outside a UPROPERTY is not a GC reference and this class is not a UObject. */
	TArray<TStrongObjectPtr<UMaterialInterface>> MaterialGCRoots;

	/** The authored overrides; MaterialGCRoots does the rooting. */
	TArray<TObjectPtr<UMaterialInterface>> MaterialRefs;

	/** Backs ProxyDesc.OverrideMaterials, which needs a TArrayView of TObjectPtr. Rebuilt per proxy. */
	TArray<TObjectPtr<UMaterialInterface>> RenderSafeMaterials;

	FArcEcotopeFoliageRenderSettings RenderSettings;

	FTransform Transform = FTransform::Identity;
	FBoxSphereBounds LocalBounds = FBoxSphereBounds(ForceInit);
	FBoxSphereBounds WorldBounds = FBoxSphereBounds(ForceInit);

	int32 NumInstances = 0;

	/** Slots GPUScene actually holds. Equals NumInstances only until the first shrink. */
	int32 NumAllocatedInstances = 0;

	FPrimitiveSceneInfoData PrimitiveSceneData;
	FPrimitiveSceneDesc SceneDesc;
	FInstancedStaticMeshSceneProxyDesc ProxyDesc;
	FCustomPrimitiveData CustomPrimitiveData;
};
