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

#include "Abilities/GameplayAbilityTypes.h"
#include "ArcGameplayAbilityActorInfo.generated.h"

class UArcQuickBarComponent;
class UArcEquipmentComponent;
class UArcItemAttachmentComponent;
class APawn;
class AActor;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcGameplayAbilityActorInfo : public FGameplayAbilityActorInfo
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	TObjectPtr<UArcItemAttachmentComponent> ItemAttachmentComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	TObjectPtr<UArcEquipmentComponent> EquipmentComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	TObjectPtr<UArcQuickBarComponent> QuickBarComponent;
	
	TWeakObjectPtr<APawn> OwnerPawn = nullptr;

	FArcGameplayAbilityActorInfo()
		: FGameplayAbilityActorInfo()
		, ItemAttachmentComponent(nullptr)
		, EquipmentComponent(nullptr)
		, OwnerPawn(nullptr)
	{
	}

	virtual void InitFromActor(AActor* InOwnerActor
							   , AActor* InAvatarActor
							   , UAbilitySystemComponent* InAbilitySystemComponent) override;
};