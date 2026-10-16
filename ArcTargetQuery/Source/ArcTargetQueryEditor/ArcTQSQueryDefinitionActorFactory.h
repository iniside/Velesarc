// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "ArcTQSQueryDefinitionActorFactory.generated.h"

UCLASS()
class UArcTQSQueryDefinitionActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UArcTQSQueryDefinitionActorFactory(const FObjectInitializer& ObjectInitializer);

	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
};
