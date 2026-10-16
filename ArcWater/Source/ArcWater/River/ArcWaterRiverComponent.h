// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"

#include "ArcWaterRiverComponent.generated.h"

class FSceneView;
class UArcWaterRiverComponent;
class UMaterialInterface;
class UMaterialParameterCollection;
class UPostProcessComponent;
class UArcWaterRiverModifier;
struct FArcWaterRiverGeneratedData;

/** Per-spline-point river channel authoring, keyed on the spline's input key. */
UCLASS(MinimalAPI)
class UArcWaterRiverMetadata : public USplineMetadata
{
	GENERATED_UCLASS_BODY()

public:
	/** Distance from the channel centre to either bank, in centimetres. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River")
	FInterpCurveFloat HalfWidth;

	/** Signed flow speed along the spline tangent, in centimetres per second. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River")
	FInterpCurveFloat FlowSpeed;

	//~ Begin USplineMetadata interface
	ARCWATER_API virtual void InsertPoint(int32 Index, float t, bool bClosedLoop) override;
	ARCWATER_API virtual void UpdatePoint(int32 Index, float t, bool bClosedLoop) override;
	ARCWATER_API virtual void AddPoint(float InputKey) override;
	ARCWATER_API virtual void RemovePoint(int32 Index) override;
	ARCWATER_API virtual void DuplicatePoint(int32 Index) override;
	ARCWATER_API virtual void CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) override;
	ARCWATER_API virtual void Reset(int32 NumPoints) override;
	ARCWATER_API virtual void Fixup(int32 NumPoints, USplineComponent* SplineComp) override;
	//~ End USplineMetadata interface

#if WITH_EDITOR
	ARCWATER_API virtual void PostEditUndo() override;
#endif
};

/** No ShowCategories: keep USplineComponent's inherited category hides (Rendering, Lighting, Physics, Collision, ...). */
UCLASS(ClassGroup = (ArcWater), meta = (BlueprintSpawnableComponent, DisplayName = "ArcWater River"), HideCategories = (Variable, Tags, ComponentTick, ComponentReplication, Components, Activation, Cooking, AssetUserData, Sockets, Mobility, Trigger, PhysicsVolume, Replication, Development, Editor, Input))
class ARCWATER_API UArcWaterRiverComponent : public USplineComponent
{
	GENERATED_BODY()

public:
	UArcWaterRiverComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Half width a newly added spline point starts at. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "1.0", Units = "cm"))
	float NewPointHalfWidthCm = 300.0f;

	/** Flow speed a newly added spline point starts at; negative flows against the spline tangent. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (Units = "cm/s"))
	float NewPointFlowSpeedCmS = 150.0f;

	/** Longest arc-length step between ribbon stations. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River|Mesh", meta = (ClampMin = "10.0", Units = "cm"))
	float MaxStepCm = 200.0f;

	/** Ribbon quads across the channel. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River|Mesh", meta = (ClampMin = "1"))
	int32 WidthSegments = 4;

	/** Arc length that maps to one tile of the surface material. */
	UPROPERTY(EditAnywhere, Category = "ArcWater River|Mesh", meta = (ClampMin = "1.0", Units = "cm"))
	float TileCm = 500.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface")
	TObjectPtr<UMaterialInterface> RiverMaterial;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface")
	TObjectPtr<UMaterialInterface> UnderwaterMaterial;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface")
	TObjectPtr<UMaterialParameterCollection> RiverParameterCollection;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Water")
	FLinearColor AbsorptionTint = FLinearColor(3.2f, 1.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Water", meta = (ClampMin = "0.25", Units = "Meters"))
	float ClarityDistanceMeters = 4.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScatterAlbedo = 0.03f;

	mutable float CachedMinCurvatureRadiusCm = TNumericLimits<float>::Max();

	mutable float CachedMaskReachCm = 0.0f;

	float GetHalfWidthAtKey(float Key) const;
	float GetFlowSpeedAtKey(float Key) const;

	void SetUnderwaterPostProcess(UPostProcessComponent* InPostProcess);
	void SetGeneratedChannel(TSharedRef<const FArcWaterRiverGeneratedData> InGeneratedData,
		int32 InReachIndex);
	const FArcWaterRiverGeneratedData* GetGeneratedChannelData() const;
	int32 GetGeneratedReachIndex() const { return GeneratedReachIndex; }
	void PushClarity();

	UArcWaterRiverMetadata* GetRiverMetadata() const { return RiverMetadata; }

	//~ Begin USplineComponent interface
	virtual USplineMetadata* GetSplinePointsMetadata() override;
	virtual const USplineMetadata* GetSplinePointsMetadata() const override;
	virtual bool AllowsSplinePointScaleEditing() const override { return false; }
	//~ End USplineComponent interface

	//~ Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UPrimitiveComponent interface

	virtual void PostLoad() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
#endif

private:
	/** Resizes the channel curves to the spline's point count; the curves and the spline points are co-indexed by contract. */
	void SynchronizeMetadata();

	void HandleOwnSplineChanged();

	void InvalidateRiverGeometry();

	void RefreshUnderwaterBlendable();

	void SetUnderwaterEnabled(bool bInEnabled);

	void HandleBeginPostProcessSettings(FVector ViewLocation, FSceneView* View);

	mutable FBox CachedLocalBounds = FBox(ForceInit);

	mutable bool bLocalBoundsValid = false;

	UPROPERTY(Instanced)
	TObjectPtr<UArcWaterRiverMetadata> RiverMetadata;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> UnderwaterPostProcess;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "ArcWater River|Generated")
	TObjectPtr<UArcWaterRiverModifier> GeneratedSource;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "ArcWater River|Generated",
		meta = (MultiLine = true))
	FString RegenerationNotice;

	TSharedPtr<const FArcWaterRiverGeneratedData> GeneratedChannelData;
	int32 GeneratedReachIndex = INDEX_NONE;

	FDelegateHandle SplineChangedHandle;

	FDelegateHandle BeginPostProcessSettingsHandle;
};
