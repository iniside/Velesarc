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

#include "ArcHUDBase.h"

#include "Components/GameFrameworkComponentManager.h"

// Sets default values
AArcHUDBase::AArcHUDBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it. PrimaryActorTick.bStartWithTickEnabled = false;
	// PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AArcHUDBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void AArcHUDBase::BeginPlay()
{
	Super::BeginPlay();
}

void AArcHUDBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}

void AArcHUDBase::MakeWidgets()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this
		, UGameFrameworkComponentManager::NAME_GameActorReady);
}
