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

#include "ArcCoreGameplayTags.h"

#include "GameplayTagsManager.h"
#include "UObject/NameTypes.h"

FArcCoreGameplayTags FArcCoreGameplayTags::GameplayTags;

void FArcCoreGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	GameplayTags.AddAllTags(Manager);

	// Notify manager that we are done adding native tags.
	// Manager.DoneAddingNativeTags();
}

void FArcCoreGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(Ability_ActivateFail_IsDead
		, "Ability.ActivateFail.IsDead"
		, "Ability failed to activate because its owner is dead.");
	AddTag(Ability_ActivateFail_Cooldown
		, "Ability.ActivateFail.Cooldown"
		, "Ability failed to activate because it is on cool down.");
	AddTag(Ability_ActivateFail_Cost
		, "Ability.ActivateFail.Cost"
		, "Ability failed to activate because it did not pass the cost checks.");
	AddTag(Ability_ActivateFail_TagsBlocked
		, "Ability.ActivateFail.TagsBlocked"
		, "Ability failed to activate because tags are blocking it.");
	AddTag(Ability_ActivateFail_TagsMissing
		, "Ability.ActivateFail.TagsMissing"
		, "Ability failed to activate because tags are missing.");
	AddTag(Ability_ActivateFail_Networking
		, "Ability.ActivateFail.Networking"
		, "Ability failed to activate because it did not pass the network checks.");
	AddTag(Ability_ActivateFail_ActivationGroup
		, "Ability.ActivateFail.ActivationGroup"
		, "Ability failed to activate because of its activation group.");

	AddTag(Ability_Behavior_SurvivesDeath
		, "Ability.Behavior.SurvivesDeath"
		, "An ability with this type tag should not be canceled due to death.");

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

	AddTag(Item_SlotActive
		, "SlotId.Active"
		, "Generic tag, which is used to slot item to make it active, when it does matter on which item slot it it.");
}

void FArcCoreGameplayTags::AddTag(FGameplayTag& OutTag
								  , const ANSICHAR* TagName
								  , const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName)
		, FString(TEXT("(Native) ")) + FString(TagComment));
}

void FArcCoreGameplayTags::AddMovementModeTag(FGameplayTag& OutTag
											  , const ANSICHAR* TagName
											  , uint8 MovementMode)
{
	AddTag(OutTag
		, TagName
		, "Character movement mode tag.");
	GameplayTags.MovementModeTagMap.Add(MovementMode
		, OutTag);
}

void FArcCoreGameplayTags::AddCustomMovementModeTag(FGameplayTag& OutTag
													, const ANSICHAR* TagName
													, uint8 CustomMovementMode)
{
	AddTag(OutTag
		, TagName
		, "Character custom movement mode tag.");
	GameplayTags.CustomMovementModeTagMap.Add(CustomMovementMode
		, OutTag);
}

FGameplayTag FArcCoreGameplayTags::FindTagByString(FString TagString
												   , bool bMatchPartialString)
{
	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString)
		, false);

	if (!Tag.IsValid() && bMatchPartialString)
	{
		FGameplayTagContainer AllTags;
		Manager.RequestAllGameplayTags(AllTags
			, true);

		for (const FGameplayTag TestTag : AllTags)
		{
			if (TestTag.ToString().Contains(TagString))
			{
				// UE_LOG(LogLyra, Display, TEXT("Could not find exact match for tag [%s]
				// but found partial match on tag
				// [%s]."), *TagString, *TestTag.ToString());
				Tag = TestTag;
				break;
			}
		}
	}

	return Tag;
}
