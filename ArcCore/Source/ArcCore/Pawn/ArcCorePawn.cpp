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

#include "Pawn/ArcCorePawn.h"

#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Pawn/ArcPawnExtensionComponent.h"

// Sets default values
AArcCorePawn::AArcCorePawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AArcCorePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePawn::UnPossessed()
{
	Super::UnPossessed();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandlePlayerStateReplicated();
	}
	// disable Ticking on camera on remote proxies, since we will update it from server
	// regardless.
}

// Called when the game starts or when spawned
void AArcCorePawn::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AArcCorePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AArcCorePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->SetupPlayerInputComponent();
	}
}

AArcCorePawnAbilitySystem::AArcCorePawnAbilitySystem()
{
	AbilitySystemComponent = CreateDefaultSubobject<UArcCoreAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void AArcCorePawnAbilitySystem::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySet && AbilitySystemComponent)
	{
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
	}
}

UAbilitySystemComponent* AArcCorePawnAbilitySystem::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AArcCorePawnAbilitySystem::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	return GetAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
}
