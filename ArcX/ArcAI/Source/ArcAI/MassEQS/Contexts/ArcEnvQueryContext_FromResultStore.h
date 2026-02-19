// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

UENUM()
enum class EArcStoreItemType : uint8
{
	Entity,
	Actor,
	ActorLocation,
	ActorArray,
	Location,
	LocationArray
};

UCLASS(MinimalAPI, Blueprintable, EditInlineNew)
class UArcEnvQueryContext_FromGlobalStore : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, meta = (Categories = "GlobalStore"))
	FGameplayTag TagKey;
	
	UPROPERTY(EditAnywhere)
	EArcStoreItemType StoreItemType = EArcStoreItemType::Entity;
		
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};