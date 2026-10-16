// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"

#include "ArcTownRoadDefinition.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT()
struct ARCTOWN_API FArcTownRoadMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere)
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;
};

UCLASS()
class ARCTOWN_API UArcTownRoadDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Road")
	TArray<FArcTownRoadMeshEntry> SegmentMeshes;

	/** Terrain weight channel every spline of this road class paints. */
	UPROPERTY(EditAnywhere, Category = "Road")
	FName SurfaceChannel;

	/** Indexed by graph valence directly: [1] is a dead end, [3] a three-way, [4] a four-way; [0] and [2] can never occur. */
	UPROPERTY(EditAnywhere, Category = "Junction")
	TArray<FArcTownRoadMeshEntry> JunctionMeshesByValence;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
