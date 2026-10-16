// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "River/ArcWaterRiverModifierTypes.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Templates/Function.h"

#include "ArcWaterRiverModifier.generated.h"

class AArcWaterRiver;
class AArcTerrain;
class UArcWaterRiverComponent;
class UCurveBase;
class UCurveFloat;
class UMaterialInterface;
class UMaterialParameterCollection;

UCLASS(ClassGroup = (ArcWater), meta = (BlueprintSpawnableComponent, DisplayName = "ArcWater River Modifier"))
class ARCWATER_API UArcWaterRiverModifier : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcWaterRiverModifier();

	UPROPERTY(EditAnywhere, Category = "ArcWater River")
	FArcWaterRiverModifierSettings Settings;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Generation",
		meta = (DisplayName = "Generation Bounds Size (Terrain Local Cm)",
			ToolTip = "Full size of the terrain-local XY rectangle used for Rivers axis sampling. Generated channel width, banks, and falloff may extend beyond these bounds."))
	FVector2D GenerationBoundsSize = FVector2D(100000.0, 100000.0);

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface")
	TObjectPtr<UMaterialInterface> RiverMaterial;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface")
	TObjectPtr<UMaterialInterface> UnderwaterMaterial;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface")
	TObjectPtr<UMaterialParameterCollection> RiverParameterCollection;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Water")
	FLinearColor AbsorptionTint = FLinearColor(3.2f, 1.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Water",
		meta = (ClampMin = "0.25", Units = "Meters"))
	float ClarityDistanceMeters = 4.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Water",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScatterAlbedo = 0.03f;

	const FArcWaterRiverGeneratedData& GetGeneratedData() const { return GeneratedData; }

	int32 GetGeneratedSplineModifierCount() const;
	FIntPoint GetGeneratedMaxFieldDims() const;
	double GetGeneratedFieldSpacingCm() const;
	int64 GetGeneratedEstimatedRasterBytes() const;
	void GetResolvedProfileRanges(float& OutMinHalfWidthCm, float& OutMaxHalfWidthCm,
		float& OutMinFalloffCm, float& OutMaxFalloffCm) const;
	bool GetGenerationBounds(FBox& OutBounds) const;

	bool ApplyGeneratedData(FArcWaterRiverGeneratedData InGeneratedData,
		const FTransform& GeneratedFrameWorldTransform, FText& OutError,
		TFunctionRef<bool()> ShouldContinue);
	void ClearGeneratedRivers();
	bool RebuildWaterComponents(FText& OutError);
	bool DoesGeneratedDataNeedRebuild() const { return bGeneratedDataNeedsRebuild; }

	virtual void PostLoad() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags,
		ETeleportType Teleport) override;
	virtual FBox ComputeBounds() const override;
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	virtual void OnAffectedTerrainConfigChanged() override;

#if WITH_EDITOR
	void SetRiverPreviewData(const FArcWaterRiverGeneratedData& InPreviewData,
		const FTransform& InPreviewFrame);
	void SetRiverPreviewVisible(bool bShouldBeVisible);
	void SetRiverPreviewThroughTerrain(bool bThroughTerrain);
	void ClearRiverPreview();
	bool HasRiverPreviewData() const { return bHasRiverPreviewData; }
	bool IsRiverPreviewTruncated() const { return bRiverPreviewTruncated; }
	int64 GetRiverPreviewSourceSegmentCount() const { return RiverPreviewSourceSegmentCount; }
	int32 GetRiverPreviewDrawnSegmentCount() const { return RiverPreviewDrawnSegmentCount; }
	int64 GetRiverPreviewSourceCrossSectionCount() const { return RiverPreviewSourceCrossSectionCount; }
	int32 GetRiverPreviewDrawnCrossSectionCount() const { return RiverPreviewDrawnCrossSectionCount; }
	bool IsRiverPreviewVisible() const { return bRiverPreviewVisible; }
	bool IsRiverPreviewThroughTerrain() const { return bRiverPreviewThroughTerrain; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void DrawStampVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void DestroyGeneratedRiverActors();
	bool IsOrdinaryWorldInstance() const;
	void ClearGeneratedData();

#if WITH_EDITOR
	void BuildRiverPreviewGeometry();
	bool RiverPreviewInputsStillMatch() const;
	void RefreshExternalCurveBindings();
	void ClearExternalCurveBindings();
	void HandleExternalCurveChanged(UCurveBase* Curve, EPropertyChangeType::Type);
#endif

	UPROPERTY(NonTransactional)
	FArcWaterRiverGeneratedData GeneratedData;

	UPROPERTY(VisibleInstanceOnly, NonTransactional, Category = "ArcWater River|Generated")
	bool bGeneratedDataNeedsRebuild = true;

	UPROPERTY(Transient, NonTransactional)
	TArray<TObjectPtr<AArcWaterRiver>> GeneratedRiverActors;

	TSharedPtr<const FArcWaterRiverGeneratedData> GeneratedDataSnapshot;
	bool bApplyingGeneratedState = false;
	bool bRebuildingOutputs = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, NonTransactional)
	FArcWaterRiverGeneratedData RiverPreviewData;

	UPROPERTY(Transient, NonTransactional)
	FTransform RiverPreviewFrame;

	UPROPERTY(Transient, NonTransactional)
	FArcWaterRiverModifierSettings RiverPreviewSettingsSnapshot;

	UPROPERTY(Transient, NonTransactional)
	TWeakObjectPtr<AArcTerrain> RiverPreviewTerrain;

	UPROPERTY(Transient, NonTransactional)
	FTransform RiverPreviewTerrainTransform;

	UPROPERTY(Transient, NonTransactional)
	FTransform RiverPreviewModifierTransform;

	UPROPERTY(Transient, NonTransactional)
	FVector RiverPreviewOwnerLocation = FVector::ZeroVector;

	UPROPERTY(Transient, NonTransactional)
	FVector2D RiverPreviewGenerationBoundsSize = FVector2D::ZeroVector;

	UPROPERTY(Transient, NonTransactional)
	TArray<FVector> RiverPreviewChannelVertices;

	UPROPERTY(Transient, NonTransactional)
	TArray<int32> RiverPreviewChannelIndices;

	UPROPERTY(Transient, NonTransactional)
	TArray<FVector> RiverPreviewCenterLines;

	UPROPERTY(Transient, NonTransactional)
	TArray<FVector> RiverPreviewBottomLines;

	UPROPERTY(Transient, NonTransactional)
	TArray<FVector> RiverPreviewFalloffLines;

	UPROPERTY(Transient, NonTransactional)
	TArray<FVector> RiverPreviewDepthLines;

	UPROPERTY(Transient, NonTransactional)
	bool bHasRiverPreviewData = false;

	UPROPERTY(Transient, NonTransactional)
	bool bRiverPreviewVisible = false;

	UPROPERTY(Transient, NonTransactional)
	bool bRiverPreviewThroughTerrain = false;

	UPROPERTY(Transient, NonTransactional)
	bool bRiverPreviewTruncated = false;

	UPROPERTY(Transient, NonTransactional)
	int64 RiverPreviewSourceSegmentCount = 0;

	UPROPERTY(Transient, NonTransactional)
	int32 RiverPreviewDrawnSegmentCount = 0;

	UPROPERTY(Transient, NonTransactional)
	int64 RiverPreviewSourceCrossSectionCount = 0;

	UPROPERTY(Transient, NonTransactional)
	int32 RiverPreviewDrawnCrossSectionCount = 0;

	TWeakObjectPtr<UCurveFloat> BoundSlopeWidthCurve;
	TWeakObjectPtr<UCurveFloat> BoundElevationWidthCurve;
	FDelegateHandle SlopeWidthCurveChangedHandle;
	FDelegateHandle ElevationWidthCurveChangedHandle;
#endif
};
