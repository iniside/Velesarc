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



#pragma once

#include "ArcNamedPrimaryAssetId.h"
#include "CommonGameInstance.h"
#include "Engine/DeveloperSettings.h"
#include "ArcCoreGameInstance.generated.h"

class UArcExperienceDefinition;
/**
 *
 */
UCLASS()
class ARCCORE_API UArcCoreGameInstance : public UCommonGameInstance
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FGuid AvatarId;

	UPROPERTY()
	FUniqueNetIdRepl MyNetId;
	
	/**
	 * Currently selected game experience. This should be set from frontend menus.
	 * And then be used when starting new game, either single player or when calling backend services.
	 *
	 * Can be used to custom menus per game experience (item in shop, characters etc).
	 */
	UPROPERTY()
	mutable TSoftObjectPtr<UArcExperienceDefinition> SelectedGameExperience;

	UPROPERTY()
	mutable FPrimaryAssetId SelectedGameExperienceId;
	
public:
	UArcCoreGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanJoinRequestedSession() const override;

protected:
	virtual void Init() override;

	virtual void Shutdown() override;

public:
	const FPrimaryAssetId& GetSelectedGameExperienceId() const
	{
		return SelectedGameExperienceId;
	}
	
	const TSoftObjectPtr<UArcExperienceDefinition>& GetSelectedGameExperience() const;

	void SetSelectedGameExperience(const TSoftObjectPtr<UArcExperienceDefinition>& InSelectedGameExperience)
	{
		SelectedGameExperience = InSelectedGameExperience;
	}

	FGuid GetAvatarId() const;

	const FUniqueNetIdRepl& GetMyNetId() const
	{
		return MyNetId;
	}

	void SetMyNetId(FUniqueNetIdRepl& InId)
	{
		MyNetId = InId;
	}
};

USTRUCT(BlueprintType)
struct FArcCoreAvatarEntry
{
	GENERATED_BODY()

	/**
	 * Unique Id of player avatar.
	 */
	UPROPERTY(EditAnywhere)
	FGuid AvatarId;

	/**
	 * Id of experience this avatar belongs to.
	 */
	UPROPERTY(EditAnywhere)
	FArcNamedPrimaryAssetId ExperienceId;

	/**
	 * If false player avatar is experience agnostic
	 */
	UPROPERTY(EditAnywhere)
	bool bIsExperienceSpecific = false;

	bool operator==(const FArcCoreAvatarEntry& Other) const
	{
		return AvatarId == Other.AvatarId;
	}

	bool operator==(const FPrimaryAssetId& InExperienceId) const
	{
		return ExperienceId == InExperienceId;
	}

	bool operator==(const FGuid& InAvatarId) const
	{
		return AvatarId == InAvatarId;
	}

	friend uint32 GetTypeHash(const FArcCoreAvatarEntry& InAvatarEntry)
	{
		return GetTypeHash(InAvatarEntry.AvatarId);
	}
};

UCLASS()
class UArcCoreAvatarSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FGuid SelectedAvatarId;

	// Maps player account id to selected avatar id
	UPROPERTY()
	TMap<FString, FGuid> PlayerToAvatarMap;
	
	UPROPERTY()
	TArray<FArcCoreAvatarEntry> Avatars;

public:
	FGuid GetSelectedAvatarId() const;

	FGuid GetSelectedAvatarId(const FString& PlayerId) const;

	TArray<FArcCoreAvatarEntry> GetAvatarsForExperience(const FPrimaryAssetId& ExperienceId) const;
	
	void SetSelectedAvatarId(const FGuid& InId);
};


/**
 * Editor settings for testing persistence.
 */
UCLASS(config = EditorPerProjectUserSettings)
class ARCCORE_API UArcCorePersistenceEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcCorePersistenceEditorSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override;

	//~End of UDeveloperSettings interface

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config)
	bool bEnableDebugAvatarId = false;

	/**
	 * Avatar Id for editor. This represents Player character in game.
	 * It is used to test persistence in editor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config)
	FGuid AvatarId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config)
	TArray<FArcCoreAvatarEntry> DebugAvatars;
};
