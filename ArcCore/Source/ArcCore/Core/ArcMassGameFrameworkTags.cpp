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

#include "Core/ArcMassGameFrameworkTags.h"

#include "GameplayTagsManager.h"

FArcMassGameFrameworkTags FArcMassGameFrameworkTags::GameplayTags;

void FArcMassGameFrameworkTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	GameplayTags.AddAllTags(Manager);
}

void FArcMassGameFrameworkTags::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(InitState_Spawned
		, "InitState.Spawned"
		, "1: Actor/component has initially spawned and can be extended");
	AddTag(InitState_DataAvailable
		, "InitState.DataAvailable"
		, "2: All required data has been loaded/replicated and is ready for initialization");
	AddTag(InitState_DataInitialized
		, "InitState.DataInitialized"
		, "3: The available data has been initialized for this actor/component, but it is not ready for full gameplay");
	AddTag(InitState_GameplayReady
		, "InitState.GameplayReady"
		, "4: The actor/component is fully ready for active gameplay");
}

void FArcMassGameFrameworkTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName)
		, FString(TEXT("(Native) ")) + FString(TagComment));
}
