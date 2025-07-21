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


#include "Abilities/Tasks/AbilityTask.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcAT_WaitPerformTargetingWithVisualization.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcWaitPerformTargetingResult, const TArray<FHitResult>&, HitResults, FTargetingRequestHandle, TargetingRequestHandle);

class UArcTargetingObject;
class AArcTargetingVisualizationActor;

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcAT_WaitPerformTargetingWithVisualization : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitPerformTargetingWithVisualization* WaitPerformTargetingWithVisualization(UGameplayAbility* OwningAbility
		, UArcTargetingObject* InTargetingObject);

	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void ExecuteTargeting();

	void HandleExecuteTargetingComplete(FTargetingRequestHandle TargetingRequestHandle);

	void HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle);

	UPROPERTY()
	TObjectPtr<UArcTargetingObject> TargetingObject;

	UPROPERTY()
	TObjectPtr<AArcTargetingVisualizationActor> VisualizationActor;
	
	FTargetingRequestHandle AsyncTargetingHandle;
	FTargetingRequestHandle ExecuteTargetingHandle;

	UPROPERTY(BlueprintAssignable)
	FArcWaitPerformTargetingResult OnTargetingExcuted;
};
