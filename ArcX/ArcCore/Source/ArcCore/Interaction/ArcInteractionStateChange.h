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
#include "NativeGameplayTags.h"
#include "UIExtensionSystem.h"
#include "ArcInteractionStateChange.generated.h"

class UMVVMViewModelBase;
class APawn;
class AController;
class UArcInteractionReceiverComponent;
class UArcInteractionLevelPlacedComponent;
class IArcInteractionObjectInterface;

/**
 * This is mainly for implementing when interactable object is selected for interaction and what should happen.
 * In case of AI interaction with interactable object we should probably skip it.
 */
USTRUCT()
struct ARCCORE_API FArcInteractionStateChange
{
	GENERATED_BODY()

public:
	virtual void OnSelected(APawn* InPawn
		, AController* InController
		, const UArcInteractionReceiverComponent* InReceiver
		, TScriptInterface<IArcInteractionObjectInterface> InteractionManager
																, const FGuid& InteractionId) const {}

	virtual void OnDeselected(APawn* InPawn
		, AController* InController
		, const UArcInteractionReceiverComponent* InReceiver
		, TScriptInterface<IArcInteractionObjectInterface> InteractionManager
																, const FGuid& InteractionId) const {}


	virtual ~FArcInteractionStateChange() = default;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcShowPromptMessageObjects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<UObject> ViewModelClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName DataName;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcShowPromptMessage
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag PromptSlot;

	UPROPERTY(BlueprintReadOnly)
	TSoftClassPtr<UUserWidget> WidgetClass;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<FArcShowPromptMessageObjects> ViewModelClasses;
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class UArcInteractionReceiverComponent> ReceiverComponent = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FGuid InteractionId;

	UPROPERTY(BlueprintReadOnly)
	TScriptInterface<IArcInteractionObjectInterface> InteractedObject;
};

USTRUCT()
struct ARCCORE_API FArcHidePromptMessage
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FUIExtensionHandle ExtensionHandle;
};

ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_Show_Message);
ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_Hide_Message);

USTRUCT()
struct ARCCORE_API FArcInteractionStateChange_ShowHUDWidget : public FArcInteractionStateChange
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UUserWidget> WidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TArray<FArcShowPromptMessageObjects> ViewModelClasses;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (Categories = "UI"))
	FGameplayTag PromptWidgetSlot;
	
public:
	virtual void OnSelected(APawn* InPawn
		, AController* InController
		, const UArcInteractionReceiverComponent* InReceiver
		, TScriptInterface<IArcInteractionObjectInterface> InteractionManager
																, const FGuid& InteractionId) const override;

	virtual void OnDeselected(APawn* InPawn
							  , AController* InController
							  , const UArcInteractionReceiverComponent* InReceiver
							  , TScriptInterface<IArcInteractionObjectInterface> InteractionManager
																, const FGuid& InteractionId) const override;

	virtual ~FArcInteractionStateChange_ShowHUDWidget() override = default;
};

/**
 * Picks all items from interacted object and then removes it.
 */
USTRUCT()
struct ARCCORE_API FArcInteractionStateChange_AutoPickItemRemoveInteraction : public FArcInteractionStateChange
{
	GENERATED_BODY()
	
public:
	virtual void OnSelected(APawn* InPawn
		, AController* InController
		, const UArcInteractionReceiverComponent* InReceiver
		, TScriptInterface<IArcInteractionObjectInterface> InteractionManager
		, const FGuid& InteractionId) const override;
	
	virtual ~FArcInteractionStateChange_AutoPickItemRemoveInteraction() override = default;
};
