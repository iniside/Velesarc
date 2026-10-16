// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"

#include "ArcTownPathComponent.generated.h"

class UArcTownPathComponent;

/** Per-spline-point half width of a town path, in world centimetres, keyed on the spline's input key. */
UCLASS(MinimalAPI)
class UArcTownPathMetadata : public USplineMetadata
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "ArcTown Path")
	FInterpCurveFloat HalfWidth;

	//~ Begin USplineMetadata interface
	ARCTOWN_API virtual void InsertPoint(int32 Index, float t, bool bClosedLoop) override;
	ARCTOWN_API virtual void UpdatePoint(int32 Index, float t, bool bClosedLoop) override;
	ARCTOWN_API virtual void AddPoint(float InputKey) override;
	ARCTOWN_API virtual void RemovePoint(int32 Index) override;
	ARCTOWN_API virtual void DuplicatePoint(int32 Index) override;
	ARCTOWN_API virtual void CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) override;
	ARCTOWN_API virtual void Reset(int32 NumPoints) override;
	ARCTOWN_API virtual void Fixup(int32 NumPoints, USplineComponent* SplineComp) override;
	//~ End USplineMetadata interface
};

/** One authored path of a settlement: a spline on the town volume carrying a per-point half width. */
UCLASS(ClassGroup = (ArcTown), meta = (BlueprintSpawnableComponent, DisplayName = "ArcTown Path"), HideCategories = (Physics, Collision, Lighting, Navigation))
class ARCTOWN_API UArcTownPathComponent : public USplineComponent
{
	GENERATED_BODY()

public:
	UArcTownPathComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Half width a newly added spline point starts at. */
	UPROPERTY(EditAnywhere, Category = "ArcTown Path", meta = (ClampMin = "1.0", Units = "cm"))
	float DefaultHalfWidth = 200.0f;

	UPROPERTY(VisibleAnywhere, Category = "Road")
	uint8 Hierarchy = 0;

	UPROPERTY(VisibleAnywhere, Category = "Road")
	int32 StartNodeId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Road")
	int32 EndNodeId = INDEX_NONE;

	UArcTownPathMetadata* GetPathMetadata() const { return PathMetadata; }

	//~ Begin USplineComponent interface
	virtual USplineMetadata* GetSplinePointsMetadata() override;
	virtual const USplineMetadata* GetSplinePointsMetadata() const override;
	virtual bool AllowsSplinePointScaleEditing() const override { return false; }
	//~ End USplineComponent interface

	virtual void PostLoad() override;
	virtual void OnRegister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
#endif

private:
	/** Resizes the half-width curve to the spline's point count; the two are co-indexed by contract. */
	void SynchronizeMetadata();

	UPROPERTY(Instanced)
	TObjectPtr<UArcTownPathMetadata> PathMetadata;
};
