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


#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcInputActionConfig.generated.h"

class UInputAction;
class UInputMappingContext;
class UArcLocalPlayerBase;
class UArcCustomInputHandler;

/**
 * FArcInputAction
 *
 *	Struct used to map a input action to a gameplay input tag.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FArcCustomInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, Instanced)
	TObjectPtr<UArcCustomInputHandler> CustomInputHandler;

	FArcCustomInputAction()
		: InputAction(nullptr)
		, InputTag(FGameplayTag())
		, CustomInputHandler(nullptr)
	{
	}
};

DECLARE_LOG_CATEGORY_EXTERN(LogArcInputConfig, Log, Log);

/**
 * ArcInputConfig
 *
 *	Non-mutable data asset that contains input configuration properties.
 */
UCLASS(BlueprintType, Const)
class ARCCORE_API UArcInputActionConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UArcInputActionConfig(const FObjectInitializer& ObjectInitializer);

	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag
													, bool bLogNotFound = true) const;

	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag
													 , bool bLogNotFound = true) const;

public:
	// List of input actions used by the owner.  These input actions are mapped to a
	// gameplay tag and must be manually bound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FArcInputAction> NativeInputActions;

	// List of input actions used by the owner.  These input actions are mapped to a
	// gameplay tag and are automatically bound to abilities with matching input tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FArcInputAction> AbilityInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FArcCustomInputAction> CustomInputHandlers;
};
