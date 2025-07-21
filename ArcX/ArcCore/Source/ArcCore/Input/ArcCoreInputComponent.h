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

#include "Containers/Map.h"
#include "Delegates/DelegateCombinations.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/ArcCustomInputHandler.h"
#include "Input/ArcInputActionConfig.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "UObject/NameTypes.h"

#include "ArcCoreInputComponent.generated.h"

/**
 * UArcInputComponent
 *
 *	Component used to manage input mappings and bindings using an input config data asset.
 */
UCLASS(Config = Input)
class ARCCORE_API UArcCoreInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

private:
	TMap<FGameplayTag, TArray<uint32>> CustomActionsBinds;
	
public:
	UArcCoreInputComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;

	template <class UserClass, typename FuncType>
	void BindNativeAction(const UArcInputActionConfig* InputConfig
						  , const FGameplayTag& InputTag
						  , ETriggerEvent TriggerEvent
						  , UserClass* Object
						  , FuncType Func
						  , bool bLogIfNotFound);

	template <class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UArcInputActionConfig* InputConfig
							, UserClass* Object
							, PressedFuncType PressedFunc
							, ReleasedFuncType TriggeredFunc
							, ReleasedFuncType ReleasedFunc
							, TArray<uint32>& BindHandles);


	template <class UserClass, typename... VarTypes>
	void BindCustomAction(UInputAction* InInputAction
						  , const FGameplayTag& ActionTag
						  , ETriggerEvent TriggerEvent
						  , UserClass* Object
						  , typename FEnhancedInputActionHandlerSignature::template TMethodPtr< UserClass, VarTypes... > Func, VarTypes... Vars);

	void RemoveCustomActions(const FGameplayTag& ActionTag);
	
	void BindCustomInputHandlerActions(const UArcInputActionConfig* InputConfig
									   , TArray<uint32>& BindHandles);

	void RemoveBinds(TArray<uint32>& BindHandles);
};

template <class UserClass, typename FuncType>
void UArcCoreInputComponent::BindNativeAction(const UArcInputActionConfig* InputConfig
											  , const FGameplayTag& InputTag
											  , ETriggerEvent TriggerEvent
											  , UserClass* Object
											  , FuncType Func
											  , bool bLogIfNotFound)
{
	check(InputConfig);
	if (const UInputAction* IA = InputConfig->FindNativeInputActionForTag(InputTag
		, bLogIfNotFound))
	{
		BindAction(IA
			, TriggerEvent
			, Object
			, Func);
	}
}

template <class UserClass, typename... VarTypes>
void UArcCoreInputComponent::BindCustomAction(UInputAction* InInputAction
						  , const FGameplayTag& ActionTag
						  , ETriggerEvent TriggerEvent
						  , UserClass* Object
						  , typename FEnhancedInputActionHandlerSignature::template TMethodPtr< UserClass, VarTypes... > Func, VarTypes... Vars)
{
	check(InInputAction);
	const uint32 Handle = BindAction(InInputAction
									, TriggerEvent
									, Object
									, Func
									, Vars...).GetHandle();

	CustomActionsBinds.FindOrAdd(ActionTag).Add(Handle);
}

template <class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UArcCoreInputComponent::BindAbilityActions(const UArcInputActionConfig* InputConfig
												, UserClass* Object
												, PressedFuncType PressedFunc
												, ReleasedFuncType TriggeredFunc
												, ReleasedFuncType ReleasedFunc
												, TArray<uint32>& BindHandles)
{
	check(InputConfig);

	for (const FArcInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			if (PressedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction
					, ETriggerEvent::Started
					, Object
					, PressedFunc
					, Action.InputTag).GetHandle());
			}

			if (TriggeredFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction
					, ETriggerEvent::Triggered
					, Object
					, TriggeredFunc
					, Action.InputTag).GetHandle());
			}

			if (ReleasedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction
					, ETriggerEvent::Completed
					, Object
					, ReleasedFunc
					, Action.InputTag).GetHandle());
			}
		}
	}
}