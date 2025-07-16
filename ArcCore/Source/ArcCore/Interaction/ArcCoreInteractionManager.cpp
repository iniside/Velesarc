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

#include "ArcCoreInteractionManager.h"
#include "Engine/World.h"
#include "Interaction/ArcCoreInteractionSubsystem.h"

#include "Net/UnrealNetwork.h"

void AArcCoreInteractionManager::OnRep_InteractionItems()
{
	if (InteractionItems.Items.Num() > 0)
	{
		
	}
}

AArcCoreInteractionManager::AArcCoreInteractionManager()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	bNetLoadOnClient = false;

	SetCallPreReplication(true);
}

void AArcCoreInteractionManager::PreInitializeComponents()
{
	InteractionItems.OwningObject = this;
	InteractionItems.Owner = this;
	Super::PreInitializeComponents();
}

void AArcCoreInteractionManager::BeginPlay()
{
	InteractionItems.OwningObject = this;
	InteractionItems.Owner = this;
	Super::BeginPlay();

	UArcCoreInteractionSubsystem* InteractionSubsystem = GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();

	InteractionSubsystem->NotifyInteractionManagerBeginPlay(this);
}


void AArcCoreInteractionManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(AArcCoreInteractionManager
		, InteractionItems
		, Params);
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}