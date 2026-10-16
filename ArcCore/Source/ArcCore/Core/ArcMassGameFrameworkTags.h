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

#pragma once

#include "GameplayTagContainer.h"

class UGameplayTagsManager;

/**
 * Native init-state gameplay tags. Hosting these in ArcCore lets ArcCore drive the
 * UGameFrameworkComponentManager::RegisterInitState chain without taking a dependency
 * on ArcMassGameFramework; ArcMassGameFramework consumes the same tags from here.
 */
struct ARCCORE_API FArcMassGameFrameworkTags
{
public:
	static const FArcMassGameFrameworkTags& Get()
	{
		return GameplayTags;
	}

	static void InitializeNativeTags();

	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;

protected:
	void AddAllTags(UGameplayTagsManager& Manager);

	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

private:
	static FArcMassGameFrameworkTags GameplayTags;
};
