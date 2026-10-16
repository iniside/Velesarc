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



#include "ArcCoreGameInstance.h"
#include "Misc/App.h"

UArcCoreGameInstance::UArcCoreGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcCoreGameInstance::Init()
{
	Super::Init();

	// Init-state tag chain registration moved to UArcMassFrameworkInitStateSubsystem.
}

void UArcCoreGameInstance::Shutdown()
{
	Super::Shutdown();
}

FGuid UArcCoreGameInstance::GetAvatarId() const
{
#if WITH_EDITOR
	const UArcCorePersistenceEditorSettings* PersistenceSettings = GetDefault<UArcCorePersistenceEditorSettings>();
	if (PersistenceSettings->bEnableDebugAvatarId)
	{
		return PersistenceSettings->AvatarId;
	}
#endif
	return AvatarId;
}

FGuid UArcCoreAvatarSubsystem::GetSelectedAvatarId() const
{
#if WITH_EDITOR
	const UArcCorePersistenceEditorSettings* PersistenceSettings = GetDefault<UArcCorePersistenceEditorSettings>();
	if (PersistenceSettings->bEnableDebugAvatarId)
	{
		return PersistenceSettings->AvatarId;
	}
#endif
	return SelectedAvatarId;
}

FGuid UArcCoreAvatarSubsystem::GetSelectedAvatarId(const FString& PlayerId) const
{
#if WITH_EDITOR
	const UArcCorePersistenceEditorSettings* PersistenceSettings = GetDefault<UArcCorePersistenceEditorSettings>();
	if (PersistenceSettings->bEnableDebugAvatarId)
	{
		return PersistenceSettings->AvatarId;
	}
#endif
	if (PlayerToAvatarMap.Contains(PlayerId))
	{
		return PlayerToAvatarMap[PlayerId];
	}
	return SelectedAvatarId;
}

TArray<FArcCoreAvatarEntry> UArcCoreAvatarSubsystem::GetAvatarsForExperience(const FPrimaryAssetId& ExperienceId) const
{
	TArray<FArcCoreAvatarEntry> Result;
#if WITH_EDITOR
	const UArcCorePersistenceEditorSettings* PersistenceSettings = GetDefault<UArcCorePersistenceEditorSettings>();
	if (PersistenceSettings->bEnableDebugAvatarId)
	{
		for (const FArcCoreAvatarEntry& AvatarEntry : PersistenceSettings->DebugAvatars)
		{
			if (AvatarEntry.ExperienceId == ExperienceId)
			{
				Result.Add(AvatarEntry);
			}
		}
		return Result;
	}
#endif
	for (const FArcCoreAvatarEntry& AvatarEntry : Avatars)
	{
		if (AvatarEntry.ExperienceId == ExperienceId)
		{
			Result.Add(AvatarEntry);
		}
	}

	return Result;
}

void UArcCoreAvatarSubsystem::SetSelectedAvatarId(const FGuid& InId)
{
	SelectedAvatarId = InId;
}

bool UArcCoreGameInstance::CanJoinRequestedSession() const
{
	// Temporary first pass:  Always return true
	// This will be fleshed out to check the player's state
	if (!Super::CanJoinRequestedSession())
	{
		return false;
	}
	return true;
}


#define LOCTEXT_NAMESPACE "ArcDeveloperSettings"

UArcCorePersistenceEditorSettings::UArcCorePersistenceEditorSettings()
{
}

FName UArcCorePersistenceEditorSettings::GetCategoryName() const
{
	return FName(TEXT("Arc Game"));
}

#undef LOCTEXT_NAMESPACE
