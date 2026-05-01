// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArcMassProcessorBrowserConfig.generated.h"

class UMassProcessor;

UCLASS()
class UArcMassProcessorBrowserConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Mass Processor Browser")
	TMap<TSubclassOf<UMassProcessor>, FString> CategoryOverrides;
};
