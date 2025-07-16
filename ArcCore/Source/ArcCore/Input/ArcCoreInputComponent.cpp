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

#include "Input/ArcCoreInputComponent.h"

#include "GameFramework/Pawn.h"
#include "Player/ArcLocalPlayerBase.h"

UArcCoreInputComponent::UArcCoreInputComponent(const FObjectInitializer& ObjectInitializer)
{
}

void UArcCoreInputComponent::OnRegister()
{
	Super::OnRegister();
}

void UArcCoreInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}

void UArcCoreInputComponent::RemoveCustomActions(const FGameplayTag& ActionTag)
{
	if (TArray<uint32>* Binds = CustomActionsBinds.Find(ActionTag))
	{
		RemoveBinds(*Binds);
	}
}

void UArcCoreInputComponent::BindCustomInputHandlerActions(const UArcInputActionConfig* InputConfig
																  , TArray<uint32>& BindHandles)
{
	AActor* O = GetOwner();
	APawn* P = Cast<APawn>(O);
	if (P == nullptr)
	{
		return;
	}
	for (const FArcCustomInputAction& Action : InputConfig->CustomInputHandlers)
	{
		if (Action.InputAction && Action.CustomInputHandler)
		{
			{
				BindHandles.Add(BindAction(Action.InputAction.Get()
					, ETriggerEvent::Started
					, Action.CustomInputHandler.Get()
					, &UArcCustomInputHandler::InputPressed
					, P
					, Action.InputTag).GetHandle());
			}

			{
				BindHandles.Add(BindAction(Action.InputAction.Get()
					, ETriggerEvent::Triggered
					, Action.CustomInputHandler.Get()
					, &UArcCustomInputHandler::InputTriggered

					, P
					, Action.InputTag).GetHandle());
			}

			{
				BindHandles.Add(BindAction(Action.InputAction.Get()
					, ETriggerEvent::Completed
					, Action.CustomInputHandler.Get()
					, &UArcCustomInputHandler::InputReleased
					, P
					, Action.InputTag).GetHandle());
			}
		}
	}
}
