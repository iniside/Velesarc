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

#include "ArcMassFrameworkInitStateSubsystem.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"

#include "Core/ArcMassGameFrameworkTags.h"

void UArcMassFrameworkInitStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency(UGameFrameworkComponentManager::StaticClass());

	UGameFrameworkComponentManager* ComponentManager = GetGameInstance()->GetSubsystem<UGameFrameworkComponentManager>();
	if (!ensure(ComponentManager))
	{
		return;
	}

	const FArcMassGameFrameworkTags& Tags = FArcMassGameFrameworkTags::Get();
	ComponentManager->RegisterInitState(Tags.InitState_Spawned, false, FGameplayTag());
	ComponentManager->RegisterInitState(Tags.InitState_DataAvailable, false, Tags.InitState_Spawned);
	ComponentManager->RegisterInitState(Tags.InitState_DataInitialized, false, Tags.InitState_DataAvailable);
	ComponentManager->RegisterInitState(Tags.InitState_GameplayReady, false, Tags.InitState_DataInitialized);
}
