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

#include "ArcWorldDelegates.h"

#include "GameplayEffect.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"

FArcComponentChannelKey::FArcComponentChannelKey(AActor* InOwningActor, UClass* InComponentClass)
	: OwningActor(InOwningActor)
	, ComponentClass(InComponentClass)
{}

UArcWorldDelegates* UArcWorldDelegates::Get(UObject* WorldObject)
{
	if (UWorld* World = WorldObject->GetWorld())
	{
		return World->GetGameInstance()->GetSubsystem<UArcWorldDelegates>();
	}

	if (UWorld* World= Cast<UWorld>(WorldObject))
	{
		return World->GetGameInstance()->GetSubsystem<UArcWorldDelegates>();
	}

	return nullptr;
}

//bool UArcWorldDelegates::DoesSupportWorldType(const EWorldType::Type WorldType) const
//{
//	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
//}
