// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Math/InterpCurve.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Templates/PimplPtr.h"
#include "ArcTerrainSplineModifier.generated.h"

class USplineComponent;
struct FArcTerrainSplineFieldScratch;

#if WITH_EDITOR
namespace ArcTerrainSplineModifier_Local
{
	class FSplineDelegateBinding;
}
#endif

USTRUCT()
struct FArcTerrainSplineEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Spline",
		meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SplineComponent"))
	FComponentReference Spline;

	/** Half width of the full-strength core, cm; the fallback when HalfWidthCurve is empty. */
	UPROPERTY(EditAnywhere, Category = "Spline", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float HalfWidthCm = 400.0f;

	/** Gradient band beyond the core, cm; the mask ramps 1 -> 0 across it. */
	UPROPERTY(EditAnywhere, Category = "Spline", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FalloffCm = 600.0f;

	/** Half width in cm, keyed on the spline's input key. Empty means a constant HalfWidthCm. */
	UPROPERTY()
	FInterpCurveFloat HalfWidthCurve;

	/** Terrain weight channel this spline paints. None paints no channel. */
	UPROPERTY(EditAnywhere, Category = "Spline", meta = (GetOptions = "GetTerrainChannels"))
	FName SurfaceChannel;
};

USTRUCT()
struct FArcTerrainSplineSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FInterpCurveVector Curve;

	UPROPERTY()
	FInterpCurveFloat HalfWidthCurve;

	UPROPERTY()
	float HalfWidthCm = 0.0f;

	UPROPERTY()
	float FalloffCm = 0.0f;

	UPROPERTY()
	FName Channel;
};

/** Additive is meaningless here: the field carries the curve's absolute height, not a delta. */
UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Spline Modifier"))
class ARCTERRAIN_API UArcTerrainSplineModifier : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcTerrainSplineModifier();

	UPROPERTY(EditAnywhere, Category = "Spline")
	TArray<FArcTerrainSplineEntry> Splines;

	//~ Begin UObject / UActorComponent / UArcTerrainStampComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	virtual void OnAffectedTerrainConfigChanged() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditComponentMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void DrawStampVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const override;
#endif
	//~ End UObject / UActorComponent / UArcTerrainStampComponent interface

	/** Rebuilds the baked fields from the current entries. Call after writing an entry's curves. */
	UFUNCTION(BlueprintCallable, Category = "Terrain|Stamp")
	void RefreshSplineSnapshot();

private:
	void RebuildSnapshot();

	bool IsSnapshotCurrent() const;

	void ResolveSplineComponents(TArray<USplineComponent*>& OutSplines) const;

#if WITH_EDITOR
	void BindSplineDelegates();
	void UnbindSplineDelegates();
	void HandleSplineEdited();
#endif

	TSharedRef<FArcTerrainSplineFieldScratch> AcquireScratch() const;

	UPROPERTY()
	TArray<FArcTerrainSplineSnapshot> SplineSnapshots;

	UPROPERTY()
	FIntPoint FieldDims = FIntPoint::ZeroValue;

	UPROPERTY()
	FGuid BedContentId;

	UPROPERTY()
	FGuid MaskContentId;

	UPROPERTY()
	TArray<FName> PlaneChannels;

	UPROPERTY()
	TArray<FGuid> PlaneContentIds;

	mutable TSharedPtr<FArcTerrainSplineFieldScratch> FieldScratch;

#if WITH_EDITOR
	TPimplPtr<ArcTerrainSplineModifier_Local::FSplineDelegateBinding> SplineDelegates;
#endif
};
