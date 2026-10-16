// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"

#include "ArcTownRoadCatalog.generated.h"

class UArcTownRoadDefinition;

UCLASS()
class ARCTOWN_API UArcTownRoadCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UArcTownRoadDefinition>> ClassesByHierarchy;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
