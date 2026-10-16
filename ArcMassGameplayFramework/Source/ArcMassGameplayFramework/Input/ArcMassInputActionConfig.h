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
#include "Input/ArcMassInputMappingContextAndPriority.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassInputActionConfig.generated.h"

class UInputAction;
class APawn;
struct FInputActionValue;

/**
 * FArcMassInputAction
 *
 *	Struct used to map an input action to a gameplay input tag.
 */
USTRUCT(BlueprintType)
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * FArcMassCustomInputAction
 *
 *	Polymorphic base for lightweight, non-GAS custom input reactions on
 *	Mass-driven player pawns. Subclass and override the trigger-phase
 *	virtuals you care about; default implementations are no-ops. Held by
 *	UArcMassInputActionConfig::CustomInputActions as TArray<FInstancedStruct>.
 */
USTRUCT(BlueprintType)
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassCustomInputAction
{
	GENERATED_BODY()

	virtual ~FArcMassCustomInputAction() = default;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	virtual void OnStarted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const {}
	virtual void OnTriggered(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const {}
	virtual void OnOngoing(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const {}
	virtual void OnCanceled(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const {}
	virtual void OnCompleted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const {}
};

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassInputConfig, Log, Log);

/**
 * UArcMassInputActionConfig
 *
 *	Non-mutable data asset that contains input configuration properties for the
 *	Mass game framework input layer.
 */
UCLASS(BlueprintType, Const)
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassInputActionConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UArcMassInputActionConfig(const FObjectInitializer& ObjectInitializer);

	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag
													, bool bLogNotFound = true) const;

	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag
													 , bool bLogNotFound = true) const;

public:
	// Soft InputMappingContext entries pushed onto the local player's EnhancedInput
	// subsystem when the pawn binds input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FArcMassInputMappingContextAndPriority> InputMappings;

	// List of input actions used by the owner.  These input actions are mapped to a
	// gameplay tag and must be manually bound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FArcMassInputAction> NativeInputActions;

	// List of input actions used by the owner.  These input actions are mapped to a
	// gameplay tag and are automatically bound to abilities with matching input tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FArcMassInputAction> AbilityInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Meta = (BaseStruct = "/Script/ArcMassGameplayFramework.ArcMassCustomInputAction"))
	TArray<FInstancedStruct> CustomInputActions;
};
