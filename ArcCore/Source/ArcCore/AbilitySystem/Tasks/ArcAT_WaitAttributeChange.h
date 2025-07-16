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

#include "ArcDynamicDelegates.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ArcAT_WaitAttributeChange.generated.h"

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitAttributeChange : public UAbilityTask
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FArcFloatMulticastDynamic OnChange;

public:
	virtual void Activate() override;

	void OnAttributeChange(const FOnAttributeChangeData& CallbackData);

	/**
	 * @brief Listens for attribute changes.
	 * @param OwningAbility Hidden	 * @param Attribute Attribute to listen
	 * @param WithSrcTag This tag must be present (if set)
	 * @param WithoutSrcTag This tag must not be present (if set)
	 * @param TriggerOnce If true end task after first change
	 * @param OptionalExternalOwner If set use that instead of owner.
	 * @return 
	 */
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitAttributeChange* AuWaitAttributeChange(UGameplayAbility* OwningAbility
															 , FGameplayAttribute Attribute
															 , FGameplayTag WithSrcTag
															 , FGameplayTag WithoutSrcTag
															 , bool TriggerOnce = false
															 , AActor* OptionalExternalOwner = nullptr);

	FGameplayTag WithTag;
	FGameplayTag WithoutTag;
	FGameplayAttribute Attribute;
	bool bTriggerOnce = false;
	FDelegateHandle OnAttributeChangeDelegateHandle;

protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ExternalOwner;

	UAbilitySystemComponent* GetFocusedASC();

	virtual void OnDestroy(bool AbilityEnded) override;
};
