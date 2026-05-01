// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshPartitionModifierComponent.h"
#include "RoadTool/ArcPathSmoother.h"
#include "ZoneGraphTypes.h"
#include "ArcRoadModifier.generated.h"

class AZoneShape;
class UZoneShapeComponent;

UENUM()
enum class EArcRoadSmoothingMethod : uint8
{
    Chaikin,
    CatmullRom
};

UCLASS(PrioritizeCategories = ("Road", "Pathfinding", "Smoothing", "Modifier"),
       meta = (BlueprintSpawnableComponent, MegaMeshClassVersion = "1"))
class UArcRoadModifier : public UE::MeshPartition::UModifierComponent
{
    GENERATED_BODY()

    using IModifierBackgroundOp = UE::MeshPartition::IModifierBackgroundOp;
    using EBuildType = UE::MeshPartition::EBuildType;
    using IDependencyInterface = UE::MeshPartition::IDependencyInterface;
    using EChangeType = UE::MeshPartition::EChangeType;

public:
    UArcRoadModifier();

    virtual TSharedPtr<const IModifierBackgroundOp> CreateBackgroundOp(const EBuildType InBuildType) const override;
    virtual TArray<FBox> ComputeBounds() const override;
    virtual bool IsContiguous() const override { return true; }
    virtual void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const override;
    virtual FGuid GetCodeVersionKey() const override;
    virtual void GatherDependencies(IDependencyInterface& Dependencies) const override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UFUNCTION(CallInEditor, Category = "Road")
    void GenerateZoneShape();

    UPROPERTY(EditAnywhere, Category = "Road")
    FVector StartPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Road")
    FVector EndPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Road", meta = (ClampMin = "50.0", UIMax = "2000.0", ForceUnits = "cm"))
    float RoadWidth = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Road", meta = (ClampMin = "10.0", UIMax = "1000.0", ForceUnits = "cm"))
    float FalloffDistance = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Road")
    FZoneLaneProfileRef ZoneShapeLaneProfile;

    UPROPERTY(EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float MaxSlopeAngleDegrees = 45.0f;

    UPROPERTY(EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float SteepSlopeThresholdDegrees = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "0.1", UIMax = "10.0"))
    float SlopeCostMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "1.0", UIMax = "5.0"))
    float UphillBias = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "0.0", UIMax = "5.0"))
    float CurvatureCostMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Smoothing")
    EArcRoadSmoothingMethod SmoothingMethod = EArcRoadSmoothingMethod::Chaikin;

    UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = "1", ClampMax = "10"))
    int32 ChaikinIterations = 3;

    UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = "0.0", UIMax = "1000.0", ForceUnits = "cm"))
    float ResampleSpacing = 200.0f;

private:
    class FBackgroundOp;

    mutable TSharedPtr<TArray<FVector>> SharedSmoothedPath = MakeShared<TArray<FVector>>();

    UPROPERTY()
    TSoftObjectPtr<AZoneShape> GeneratedZoneShape;
};
