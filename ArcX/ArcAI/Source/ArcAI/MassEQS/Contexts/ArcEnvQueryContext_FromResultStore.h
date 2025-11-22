// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "ArcEnvQueryContext_FromResultStore.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcEnvQueryContext_FromResultStore : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	FName ResultName;
	
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
