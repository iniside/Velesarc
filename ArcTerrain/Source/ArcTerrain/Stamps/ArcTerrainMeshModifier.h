// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Stamps/ArcTerrainCollarBake.h"
#include "Assets/ArcTerrainBakedField.h"
#include "ArcTerrainMeshModifier.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Mesh Modifier"))
class ARCTERRAIN_API UArcTerrainMeshModifier : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcTerrainMeshModifier();

	/** Source geometry. Null = the modifier has no footprint and is gathered by no cell. */
	UPROPERTY(EditAnywhere, Category = "Mesh Modifier")
	TObjectPtr<UStaticMesh> Mesh;

	/** Material the merged geometry renders with; null = the mesh's own LOD0 material. */
	UPROPERTY(EditAnywhere, Category = "Mesh Modifier")
	TObjectPtr<UMaterialInterface> MaterialOverride;

	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float EdgeFalloffBand = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "16.0"))
	float EdgeSharpness = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Falloff")
	bool bCircleMask = false;

	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (EditCondition = "bCircleMask", ForceUnits = "cm"))
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", EditCondition = "bCircleMask", ForceUnits = "cm"))
	float CircleRadius = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", EditCondition = "bCircleMask", ForceUnits = "cm"))
	float CircleFalloffBand = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Bed", meta = (ClampMin = "25.0", ForceUnits = "cm"))
	float BedTexelSizeCm = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Bed", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float BedMaxRiseCm = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Bed", meta = (ClampMin = "1", ClampMax = "8"))
	int32 BedSamplesPerTexel = 4;

	UPROPERTY()
	FArcTerrainBakedField BedField;

	UPROPERTY(EditAnywhere, Category = "Collar")
	FArcTerrainCollarParams CollarParams;

	UPROPERTY()
	FArcTerrainCollarMesh CollarMesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid CollarBakeKey;

	UPROPERTY()
	FGuid BedSourceKey;

	UPROPERTY()
	FGuid BedContentKey;

	UPROPERTY()
	FVector2D BedCoverageXY = FVector2D::ZeroVector;

	UPROPERTY()
	bool bBedFieldBuilt = false;
#endif

	//~ Begin UArcTerrainStampComponent interface
	virtual FBox ComputeBounds() const override;
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	//~ End UArcTerrainStampComponent interface

	//~ Begin UActorComponent / USceneComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostEditComponentMove(bool bFinished) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	//~ End UActorComponent / USceneComponent interface

private:
	/** Derives CoverageXY + EdgeFalloffBand from the mesh box, SYMMETRIZED about the component pivot
	 *  (the descriptor carries no footprint offset, and SampleStamp tests |PatchXY| against the half
	 *  extents) and padded by the falloff band so the mask ramp lands outside the silhouette. */
	void UpdateDerivedCoverage();

	bool IsRasterPoseSupported() const;

	void SyncVisibleMesh();

	void DestroyVisibleMesh();

	void SyncCollarMesh();

	void DestroyCollarMesh();

	UPROPERTY(Transient, NonTransactional, DuplicateTransient, TextExportTransient)
	TObjectPtr<UStaticMeshComponent> VisibleMesh;

	UPROPERTY(Transient, NonTransactional, DuplicateTransient, TextExportTransient)
	TObjectPtr<UDynamicMeshComponent> Collar;

#if WITH_EDITOR
	void RebindMeshBoundsDelegate();

	void UpdateCollar(bool bModifyBeforeWrite);
	void UnbindMeshBoundsDelegate();

	void BuildBedField();

	bool NeedsBedRebuild() const;

	FGuid ComputeMeshSourceKey() const;

	FGuid ComputeCurrentBedContentId() const;

	void OnMeshPostBuild(UStaticMesh* BuiltMesh);

	/** Unlike an authored CoverageXY, this footprint is derived from a separate asset, so it can change
	 *  with no edit to this component: a reimport that resizes Mesh must re-dirty the vacated and newly
	 *  covered cells or they never rebuild. */
	void OnMeshExtendedBoundsChanged(const FBoxSphereBounds& NewBounds);

	/** The asset BoundsDelegateHandle is registered against — NOT necessarily Mesh, which may already
	 *  have been reassigned; unbinding reads this, never Mesh. */
	TWeakObjectPtr<UStaticMesh> BoundsDelegateMesh;

	FDelegateHandle BoundsDelegateHandle;

	FDelegateHandle PostMeshBuildHandle;
#endif
};
