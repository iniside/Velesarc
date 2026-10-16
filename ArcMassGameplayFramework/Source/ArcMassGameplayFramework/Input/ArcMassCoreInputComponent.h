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
#include "EnhancedInputComponent.h"
#include "Input/ArcMassInputActionConfig.h"
#include "GameplayTagContainer.h"

#include "ArcMassCoreInputComponent.generated.h"

/**
 * UArcMassCoreInputComponent
 *
 *	UEnhancedInputComponent subclass used by the Mass game framework pawn
 *	hero component to bind native and ability input actions from a
 *	UArcMassInputActionConfig.
 */
UCLASS(Config = Input)
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassCoreInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

private:
	TMap<FGameplayTag, TArray<uint32>> CustomActionsBinds;

public:
	UArcMassCoreInputComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;

	template <class UserClass, typename FuncType>
	void BindNativeAction(const UArcMassInputActionConfig* InputConfig
						  , const FGameplayTag& InputTag
						  , ETriggerEvent TriggerEvent
						  , UserClass* Object
						  , FuncType Func
						  , bool bLogIfNotFound);

	template <class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UArcMassInputActionConfig* InputConfig
							, UserClass* Object
							, PressedFuncType PressedFunc
							, ReleasedFuncType ReleasedFunc
							, TArray<uint32>& BindHandles);

	template <class UserClass, typename PressedFuncType, typename TriggeredFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UArcMassInputActionConfig* InputConfig
							, UserClass* Object
							, PressedFuncType PressedFunc
							, TriggeredFuncType TriggeredFunc
							, ReleasedFuncType ReleasedFunc
							, TArray<uint32>& BindHandles);

	template <class UserClass, typename... VarTypes>
	void BindCustomAction(UInputAction* InInputAction
						  , const FGameplayTag& ActionTag
						  , ETriggerEvent TriggerEvent
						  , UserClass* Object
						  , typename FEnhancedInputActionHandlerValueSignature::template TMethodPtr< UserClass, VarTypes... > Func, VarTypes... Vars);

	void RemoveCustomActions(const FGameplayTag& ActionTag);

	void RemoveBinds(TArray<uint32>& BindHandles);
};

template <class UserClass, typename FuncType>
void UArcMassCoreInputComponent::BindNativeAction(const UArcMassInputActionConfig* InputConfig
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
void UArcMassCoreInputComponent::BindCustomAction(UInputAction* InInputAction
						  , const FGameplayTag& ActionTag
						  , ETriggerEvent TriggerEvent
						  , UserClass* Object
						  , typename FEnhancedInputActionHandlerValueSignature::template TMethodPtr< UserClass, VarTypes... > Func, VarTypes... Vars)
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
void UArcMassCoreInputComponent::BindAbilityActions(const UArcMassInputActionConfig* InputConfig
													, UserClass* Object
													, PressedFuncType PressedFunc
													, ReleasedFuncType ReleasedFunc
													, TArray<uint32>& BindHandles)
{
	check(InputConfig);

	for (const FArcMassInputAction& Action : InputConfig->AbilityInputActions)
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

template <class UserClass, typename PressedFuncType, typename TriggeredFuncType, typename ReleasedFuncType>
void UArcMassCoreInputComponent::BindAbilityActions(const UArcMassInputActionConfig* InputConfig
													, UserClass* Object
													, PressedFuncType PressedFunc
													, TriggeredFuncType TriggeredFunc
													, ReleasedFuncType ReleasedFunc
													, TArray<uint32>& BindHandles)
{
	check(InputConfig);

	for (const FArcMassInputAction& Action : InputConfig->AbilityInputActions)
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
