// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcStateTreeHelpers.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcStateTreeHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arc AI")
	static void SendGameplayAbilityEvent(FGameplayTag MessageId, const FGameplayTagContainer& AbilityTags, AActor* AffectedActor, AActor* Instigator);
	
	UFUNCTION(BlueprintCallable, Category = "Arc AI")
	static void SendGameplayAbilityEventInRadius(FGameplayTag MessageId, const FVector& Location, float Radius, const FGameplayTagContainer& AbilityTags, AActor* Instigator);
};
