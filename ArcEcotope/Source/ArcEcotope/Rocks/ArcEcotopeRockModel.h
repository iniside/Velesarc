// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"

#include "ArcEcotopeRockModel.generated.h"

UCLASS(BlueprintType)
class ARCECOTOPE_API UArcEcotopeRockModel : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<FArcEcotopeGroundFoliageMesh> Meshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	FArcEcotopeFoliageRenderSettings RenderSettings;
};
