// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "ArcMassEnvQueryGenerator_ProjectedPoints.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcMassEnvQueryGenerator_ProjectedPoints : public UEnvQueryGenerator
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FEnvTraceData ProjectionData;
	
public:
	UArcMassEnvQueryGenerator_ProjectedPoints(const FObjectInitializer& ObjectInitializer);
	
	virtual void ProjectAndFilterNavPoints(TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const;
};
