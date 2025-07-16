/**
 * This file is part of ArcX.
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

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ArcAT_WaitGlobalTargetingHitResult.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGlobalTargetingHitResultDelegate, const FHitResult&, HitResult);
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcAT_WaitGlobalTargetingHitResult : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks"
	, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitGlobalTargetingHitResult* WaitGlobalTargetingHitResult(UGameplayAbility* OwningAbility
												   , FName TaskInstanceName
												   , FGameplayTag TargetingTag);

	virtual void Activate() override;

	virtual void OnDestroy(bool bInOwnerFinished) override;

	FGameplayTag TargetingTag;
	
	FDelegateHandle TargetingDelegateHandle;

	UPROPERTY(BlueprintAssignable)
	FWaitGlobalTargetingHitResultDelegate OnHitResultChanged;

	void HandleOnHitResultChanged(const FHitResult& HitResult);
};
