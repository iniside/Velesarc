// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Lighting/ArcTerrainLightingMesh.h"

#include "ArcTerrainFarOcclusionComponent.generated.h"

class UMaterialInterface;

/**
 * One far block's occlusion-only primitive: a decimated triangle mesh that exists in the ray tracing
 * FarField layer and nowhere else. It never rasterizes, never captures a Lumen card and never
 * contributes radiance.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient, HideCategories = (Physics, Collision, Activation, Cooking, HLOD, Navigation))
class UArcTerrainFarOcclusionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UArcTerrainFarOcclusionComponent();

	void SetMesh(ArcTerrainLightingMesh::FMesh&& InMesh);

	const ArcTerrainLightingMesh::FMesh& GetMesh() const { return Mesh; }

	//~ Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }
	//~ End UPrimitiveComponent interface

private:
	ArcTerrainLightingMesh::FMesh Mesh;
};
