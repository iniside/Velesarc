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



#include "ArcCoreGameInstance.h"
#include "ArcCoreGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Core/ArcCoreAssetManager.h"
#include "Development/ArcDeveloperSettings.h"
#include "GameMode/ArcExperienceDefinition.h"
#include "GameMode/ArcUserFacingExperienceDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CommandLine.h"
#include "Persistence/ArcCorePersistanceSubsystem.h"
#include "Templates/Casts.h"

UArcCoreGameInstance::UArcCoreGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcCoreGameInstance::Init()
{
	Super::Init();

	// Register our custom init states
	UGameFrameworkComponentManager* ComponentManager = GetSubsystem<UGameFrameworkComponentManager>(this);

	if (ensure(ComponentManager))
	{
		const FArcCoreGameplayTags& GameplayTags = FArcCoreGameplayTags::Get();

		ComponentManager->RegisterInitState(GameplayTags.InitState_Spawned
			, false
			, FGameplayTag());
		ComponentManager->RegisterInitState(GameplayTags.InitState_DataAvailable
			, false
			, GameplayTags.InitState_Spawned);
		ComponentManager->RegisterInitState(GameplayTags.InitState_DataInitialized
			, false
			, GameplayTags.InitState_DataAvailable);
		ComponentManager->RegisterInitState(GameplayTags.InitState_GameplayReady
			, false
			, GameplayTags.InitState_DataInitialized);
	}

	GetSubsystem<UArcCorePersistenceSubsystem>();
	if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		GetSelectedGameExperience();
	}
	//TODO: Extend it check for similiar states in PlayerStateExtension
}

void UArcCoreGameInstance::Shutdown()
{
	Super::Shutdown();
}

const TSoftObjectPtr<UArcExperienceDefinition>& UArcCoreGameInstance::GetSelectedGameExperience() const
{
	TSoftObjectPtr<UArcExperienceDefinition> NewExperience;
	UWorld* World = GetWorld();

	FURL WorldURL = World->URL;

	FString Options(TEXT(""));
	FString	Error(TEXT(""));
	for( int32 i=0; i<WorldURL.Op.Num(); i++ )
	{
		Options += TEXT("?");
		Options += WorldURL.Op[i];
	}
	
	FPrimaryAssetId ExperienceId;
	FString ExperienceIdSource;

	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(Options, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(Options, TEXT("Experience"));
		ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
		ExperienceIdSource = TEXT("OptionsString");
	}

	// "UserExperience="
	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(Options, TEXT("UserExperience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(Options, TEXT("UserExperience"));
		UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
		FPrimaryAssetId UserExperienceId = FPrimaryAssetId::FromString(ExperienceFromOptions);
		
		TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(UserExperienceId);
		if (ensure(Handle.IsValid()))
		{
			Handle->WaitUntilComplete();
		}
		UArcUserFacingExperienceDefinition* UserFacing = Cast<UArcUserFacingExperienceDefinition>(Handle->GetLoadedAsset());
		ExperienceIdSource = "User Facing Experience";
		ExperienceId = UserFacing->ExperienceID;
		
	}
	// see if the command line wants to set the experience
	if (!ExperienceId.IsValid())
	{
		FString ExperienceFromCommandLine;
		if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
		{
			ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
			if (!ExperienceId.PrimaryAssetType.IsValid())
			{
				ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
			}
			ExperienceIdSource = TEXT("CommandLine");
		}
	}

	// see if the world settings has a default experience
	if (!ExperienceId.IsValid())
	{
		if (AArcCoreWorldSettings* TypedWorldSettings = Cast<AArcCoreWorldSettings>(World->GetWorldSettings()))
		{
			ExperienceId = TypedWorldSettings->GetDefaultExperience();
			ExperienceIdSource = TEXT("WorldSettings");
		}
	}
	
#if WITH_EDITOR
	if (!ExperienceId.IsValid() && World->IsPlayInEditor())
	{
		ExperienceId = GetDefault<UArcDeveloperSettings>()->ExperienceOverride;
		ExperienceIdSource = TEXT("DeveloperSettings");
	}
#endif
	SelectedGameExperienceId = ExperienceId;
	SelectedGameExperience = TSoftObjectPtr<UArcExperienceDefinition>(UAssetManager::Get().GetPrimaryAssetPath(ExperienceId));
	return SelectedGameExperience;
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
	return FApp::GetProjectName();
}

#undef LOCTEXT_NAMESPACE