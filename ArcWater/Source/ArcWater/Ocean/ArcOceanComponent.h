// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "Ocean/ArcOceanRingGeometry.h"
#include "Ocean/ArcOceanWaves.h"

#include "ArcOceanComponent.generated.h"

class AArcTerrain;
class UMaterialParameterCollection;
class UPostProcessComponent;
class FSceneView;

namespace ArcOcean
{
	inline constexpr int32 MaxRings = 12;
	inline constexpr int32 UnderwaterStencilValue = 8;
}

struct FArcOceanViewPlane
{
	FVector PlanePoint = FVector::ZeroVector;
	FVector3f PlaneNormal = FVector3f::ZAxisVector;
	float NearPlaneCm = 0.0f;
	FVector2f TanHalfFov = FVector2f::ZeroVector;
	float SlopeMagnitude = 0.0f;
	float MaskBandHalfCm = 0.0f;
};

UCLASS(ClassGroup = (ArcWater), meta = (BlueprintSpawnableComponent))
class ARCWATER_API UArcOceanComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UArcOceanComponent();

	//~ Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FMatrix GetRenderMatrix() const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetNumMaterials() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UPrimitiveComponent interface

	//~ Begin UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UActorComponent interface

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void PushWaveParams();
	void PushSeabedSnapshot(const FSceneView& InView);
	void PushClarity();
	void PushSurfacePlane(const FArcOceanViewPlane& InViewPlane);
	void SetUnderwaterPostProcess(UPostProcessComponent* InPostProcess);

	const FArcOceanWaveSettings& GetWaveSettings() const { return WaveSettings; }
	float GetDeepDepthCm() const;
	float GetBaseCellSize() const { return BaseCellSize; }
	int32 GetRingCount() const { return RingCount; }
	int32 GetVertsPerSide() const { return VertsPerSide; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean")
	TObjectPtr<UMaterialInterface> OceanMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Waves")
	FArcOceanWaveSettings WaveSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Grid", meta = (ClampMin = "10.0", Units = "Centimeters"))
	float BaseCellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Grid", meta = (ClampMin = "1", ClampMax = "12"))
	int32 RingCount = ArcOcean::DefaultRingCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Grid", meta = (ClampMin = "9", ClampMax = "257"))
	int32 VertsPerSide = ArcOcean::DefaultVertsPerSide;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Water")
	TObjectPtr<UMaterialParameterCollection> WaterParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Water")
	FLinearColor AbsorptionTint = FLinearColor(3.2f, 1.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Water", meta = (ClampMin = "0.25", Units = "Meters"))
	float ClarityDistanceMeters = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScatterAlbedo = 0.0075f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Water")
	TObjectPtr<UMaterialInterface> UnderwaterPostProcessMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ocean|Seabed")
	TSoftObjectPtr<AArcTerrain> AffectedTerrain;

private:
	void HandleSeabedBeginPostProcessSettings(FVector InViewLocation, FSceneView* InView);
	void HandleBeginPostProcessSettings(FVector InViewLocation, FSceneView* InView);
	FArcOceanViewPlane SolveViewPlane(const FSceneView& InView) const;
	void UpdateUnderwaterCull(const FVector& InViewLocation, const FArcOceanViewPlane& InViewPlane);
	void RefreshUnderwaterBlendable();

#if WITH_EDITOR
	void ReportClampedWaveSettings() const;
#endif

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> UnderwaterPostProcess;

	FDelegateHandle SeabedBeginPostProcessSettingsHandle;
	FDelegateHandle BeginPostProcessSettingsHandle;
};
