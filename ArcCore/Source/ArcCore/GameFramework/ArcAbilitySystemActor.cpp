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

#include "ArcAbilitySystemActor.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AttributeSet.h"

// Sets default values
AArcAbilitySystemActor::AArcAbilitySystemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

void AArcAbilitySystemActor::PreInitializeComponents()
{
	for (const FArcAttributeSetWithTable& AS : DefaultAttributeSetsClasses)
	{
		if (AS.DefaultAttributeSetClass)
		{
			UAttributeSet* AttributeSet = NewObject<UAttributeSet>(this
				, AS.DefaultAttributeSetClass
				, AS.DefaultAttributeSetClass->GetFName());
			if (AS.AttributeSetTable != nullptr)
			{
				AttributeSet->InitFromMetaDataTable(AS.AttributeSetTable);
			}
			AttributeSet->SetNetAddressable();
		}
	}
	if (bSpawnAbiltySystemOnBeginPlay == true)
	{
		AbilitySystem = NewObject<UArcCoreAbilitySystemComponent>(this
			, UArcCoreAbilitySystemComponent::StaticClass()
			, "AbilitySystem");
		AbilitySystem->RegisterComponentWithWorld(GetWorld());
		AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
		AbilitySystem->SetNetAddressable();
		AbilitySystem->SetIsReplicated(true);
	}
	Super::PreInitializeComponents();
}

// Called when the game starts or when spawned
void AArcAbilitySystemActor::BeginPlay()
{
	Super::BeginPlay();
}

UAbilitySystemComponent* AArcAbilitySystemActor::GetAbilitySystemComponent() const
{
	if (AbilitySystem == nullptr && bSpawnAbiltySystemOnBeginPlay == false)
	{
		AbilitySystem = NewObject<UArcCoreAbilitySystemComponent>(const_cast<AArcAbilitySystemActor*>(this)
			, UArcCoreAbilitySystemComponent::StaticClass()
			, "AbilitySystem");
		AbilitySystem->RegisterComponentWithWorld(GetWorld());
		AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
		AbilitySystem->SetNetAddressable();
		AbilitySystem->SetIsReplicated(true);
	}
	return AbilitySystem;
}

void AArcAbilitySystemActor::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	GetAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
}
