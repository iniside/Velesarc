// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Lighting/ArcTerrainLightingMesh.h"

#include "ArcTerrainLightingComponent.generated.h"

class UMaterialInterface;
class UTexture2D;
class UTexture2DArray;

/**
 * One terrain cell's lighting-only primitive: a static, decimated triangle mesh that exists for ray
 * tracing and Lumen and never draws in the raster passes the CBT proxy owns.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient, HideCategories = (Physics, Collision, Activation, Cooking, HLOD, Navigation))
class UArcTerrainLightingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UArcTerrainLightingComponent();

	/** Rebuild path: the proxy is recreated, so every RHI resource built from this mesh is rebuilt with it. */
	void SetMesh(ArcTerrainLightingMesh::FMesh&& InMesh);

	const ArcTerrainLightingMesh::FMesh& GetMesh() const { return Mesh; }

	void SetHeightTexture(UTexture2DArray* InTexture) { HeightTexture = InTexture; }
	UTexture2DArray* GetHeightTexture() const { return HeightTexture; }

	void SetWeightTextures(TArray<TObjectPtr<UTexture2DArray>>&& InTextures) { WeightTextures = MoveTemp(InTextures); }
	const TArray<TObjectPtr<UTexture2DArray>>& GetWeightTextures() const { return WeightTextures; }

	void SetPackedHeightTexture(UTexture2D* InTexture) { PackedHeightTexture = InTexture; }
	UTexture2D* GetPackedHeightTexture() const { return PackedHeightTexture; }

	void SetCellSize(double InCellSize) { CellSize = InCellSize; }
	double GetCellSize() const { return CellSize; }

	//~ Begin UActorComponent interface
	virtual void OnRegister() override;
	//~ End UActorComponent interface

	//~ Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }
	//~ End UPrimitiveComponent interface

private:
	ArcTerrainLightingMesh::FMesh Mesh;

	double CellSize = 0.0;

	/** Slot-0 material — this cell's own MID over the terrain material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2DArray> HeightTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2DArray>> WeightTextures;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PackedHeightTexture;
};
