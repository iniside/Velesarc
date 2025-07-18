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

#include "ArcCore/Items/ArcItemsComponent.h"

#include "ArcCoreGameplayTags.h"
#include "ArcWorldDelegates.h"
#include "Components/GameFrameworkComponentManager.h"

const FName UArcItemsComponent::NAME_ActorFeatureName(TEXT("ArcItemsComponent"));

UArcItemsComponent::UArcItemsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
}

void UArcItemsComponent::OnRegister()
{
	Super::OnRegister();
	
	RegisterInitStateFeature();
}

// Called when the game starts
void UArcItemsComponent::BeginPlay()
{
	Super::BeginPlay();

	UArcWorldDelegates::Get(this)->BroadcastActorOnComponentBeginPlay(
		FArcComponentChannelKey(GetOwner(), GetClass())
		, this);

	UArcWorldDelegates::Get(this)->BroadcastClassOnComponentBeginPlay(GetClass(), this);
	
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(GetOwner(), GetClass()->GetFName());

	TryToChangeInitState(FArcCoreGameplayTags::Get().InitState_GameplayReady);
}
