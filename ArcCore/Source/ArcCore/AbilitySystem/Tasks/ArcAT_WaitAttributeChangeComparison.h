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
#include "ArcAT_WaitAttributeChangeComparison.generated.h"

UENUM()
enum class EArcWaitAttributeComprison : uint8
{
	None
	, GreaterThan
	, LessThan
	, GreaterThanOrEqualTo
	, LessThanOrEqualTo
	, NotEqualTo
	, ExactlyEqualTo
	, MAX UMETA(Hidden)};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitAttributeChangeComparison : public UAbilityTask
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FArcFloatMulticastDynamic OnChange;

public:
	virtual void Activate() override;

	void OnAttributeChange(const FOnAttributeChangeData& CallbackData);

	/**
	 * @brief Listens for attribute change of owner or optionally on external actor.
	 * @param OwningAbility hidden
	 * @param Attribute Attribute we are observing
	 * @param InComparisonType How we are comparing value
	 * @param InComparisonValue Value to which attribute will be compared
	 * @param WithSrcTag This tag must be present (if set)
	 * @param WithoutSrcTag This tag must not be present (if set)
	 * @param TriggerOnce If true, do not listen for subsequent changes.
	 * @param OptionalExternalOwner If provided we will look for attribute change there.
	 * @return 
	 */
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitAttributeChangeComparison* AuWaitAttributeChangeComparison(UGameplayAbility* OwningAbility
																				 , FGameplayAttribute Attribute
																				 , EArcWaitAttributeComprison InComparisonType
																				 , float InComparisonValue
																				 , FGameplayTag WithSrcTag
																				 , FGameplayTag WithoutSrcTag
																				 , bool TriggerOnce = false
																				 , AActor* OptionalExternalOwner = nullptr);

	FGameplayTag WithTag;
	FGameplayTag WithoutTag;
	FGameplayAttribute Attribute;
	EArcWaitAttributeComprison ComparisonType;
	float ComparisonValue = 0;
	bool bTriggerOnce = false;
	FDelegateHandle OnAttributeChangeDelegateHandle;

protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ExternalOwner;

	UAbilitySystemComponent* GetFocusedASC();

	virtual void OnDestroy(bool AbilityEnded) override;
};
