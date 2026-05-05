// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "ArcMassItemAttachmentIntegrationTests.generated.h"

UCLASS()
class AArcMassAttachmentTestActor : public AActor
{
	GENERATED_BODY()
public:
	AArcMassAttachmentTestActor();

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
};
