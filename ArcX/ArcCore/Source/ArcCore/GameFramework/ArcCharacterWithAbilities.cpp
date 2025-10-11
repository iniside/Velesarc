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

#include "GameFramework/ArcCharacterWithAbilities.h"

#include "ArcCharacterMovementComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Pawn/ArcPawnExtensionComponent.h"
#include "Perception/AIPerceptionComponent.h"

UAIPerceptionComponent* AArcCharacterWithAbilities::GetPerceptionComponent()
{
	return FindComponentByClass<UAIPerceptionComponent>();
}

const FName AArcCharacterWithAbilities::AbilitySystemName = TEXT("AbilitySystem");
const FName AArcCharacterWithAbilities::PawnExtenstionName = TEXT("PawnExtenstion");

AArcCharacterWithAbilities::AArcCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass(CharacterMovementComponentName, UArcCharacterMovementComponent::StaticClass()))
{
	AbilitySystem = CreateDefaultSubobject<UArcCoreAbilitySystemComponent>(AbilitySystemName);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AbilitySystem->SetIsReplicated(true);
	CreateDefaultSubobject<UArcPawnExtensionComponent>(PawnExtenstionName);
}

void AArcCharacterWithAbilities::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->InitializeAbilitySystem(AbilitySystem
			, this);
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCharacterWithAbilities::UnPossessed()
{
	Super::UnPossessed();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

UAbilitySystemComponent* AArcCharacterWithAbilities::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AArcCharacterWithAbilities::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	return GetAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
}
