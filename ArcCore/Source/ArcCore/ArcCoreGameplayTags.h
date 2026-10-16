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
#include "Containers/UnrealString.h"
#include "GameplayTagContainer.h"
#include "HAL/Platform.h"

class UGameplayTagsManager;

/**
 * FLyraGameplayTags
 *
 *	Singleton containing native gameplay tags.
 */
struct ARCCORE_API FArcCoreGameplayTags
{

public:
	static const FArcCoreGameplayTags& Get()
	{
		return GameplayTags;
	}

	static void InitializeNativeTags();

	static FGameplayTag FindTagByString(FString TagString
										, bool bMatchPartialString = false);

public:
	TMap<uint8, FGameplayTag> MovementModeTagMap;
	TMap<uint8, FGameplayTag> CustomMovementModeTagMap;

	FGameplayTag Item_SlotActive;

protected:
	void AddAllTags(UGameplayTagsManager& Manager);

	void AddTag(FGameplayTag& OutTag
				, const ANSICHAR* TagName
				, const ANSICHAR* TagComment);

	void AddMovementModeTag(FGameplayTag& OutTag
							, const ANSICHAR* TagName
							, uint8 MovementMode);

	void AddCustomMovementModeTag(FGameplayTag& OutTag
								  , const ANSICHAR* TagName
								  , uint8 CustomMovementMode);

private:
	static FArcCoreGameplayTags GameplayTags;
};
