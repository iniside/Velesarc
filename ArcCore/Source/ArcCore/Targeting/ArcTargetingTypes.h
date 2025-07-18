/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcTargetingTypes.generated.h"

class UTargetingPreset;
class APawn;
class UArcCoreAbilitySystemComponent;
struct FArcTargetingSourceContext;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcCoreGlobalTargetingEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<UTargetingPreset> TargetingPreset;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	TArray<FHitResult> HitResults;
	
	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bIsClient = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bIsServer = false;
	
	TWeakObjectPtr<UArcCoreAbilitySystemComponent> ArcASC;
	FGameplayTag TargetingTag;
	
	FTargetingRequestHandle TargetingHandle;
	
	void HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle);

	void StartTargeting(APawn* InPawn, UTargetingSubsystem* Targeting, const FArcTargetingSourceContext& Context, UArcCoreAbilitySystemComponent* InArcASC, const FGameplayTag& InTargetingTag);

	void EndTargeting(UTargetingSubsystem* Targeting);
};