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

#include "ArcExperienceManager.h"
#include "ArcExperienceDefinition.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "GameMode/ArcUserFacingExperienceDefinition.h"

void UArcExperienceManager::Initialize(FSubsystemCollectionBase& Collection)
{
	UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateUObject(this
		, &ThisClass::OnAssetManagerCreated));
}

#if WITH_EDITOR

void UArcExperienceManager::OnPlayInEditorBegun()
{
	ensure(ExperienceRequestCountMap.IsEmpty());
	ExperienceRequestCountMap.Empty();
}

void UArcExperienceManager::OnPlayInEditorFinished()
{
}

bool UArcExperienceManager::RequestToPerformActions(FPrimaryAssetId ExperienceID)
{
	if (GIsEditor)
	{
		UArcExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<UArcExperienceManager>();
		check(ExperienceManagerSubsystem);

		// Only let the first PIE world to get this far activate the actions
		int32& Count = ExperienceManagerSubsystem->ExperienceRequestCountMap.FindOrAdd(ExperienceID);
		++Count;
		return Count == 1;
	}
	else
	{
		return true;
	}
}

bool UArcExperienceManager::RequestToRemoveActions(FPrimaryAssetId ExperienceID)
{
	if (GIsEditor)
	{
		UArcExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<UArcExperienceManager>();
		check(ExperienceManagerSubsystem);

		// Only let the last PIE world to get this far deactivate the actions
		int32& Count = ExperienceManagerSubsystem->ExperienceRequestCountMap.FindChecked(ExperienceID);
		--Count;

		if (Count == 0)
		{
			ExperienceManagerSubsystem->ExperienceRequestCountMap.Remove(ExperienceID);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

#endif

void UArcExperienceManager::OnAssetManagerCreated()
{
	{
		TArray<FAssetData> AssetList;
		UAssetManager::Get().GetPrimaryAssetDataList(
			FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName())
			,
			/*out*/ AssetList);
#if WITH_EDITORONLY_DATA
		for (const FAssetData& AssetData : AssetList)
		{
			UE_LOG(LogInit
				, Log
				, TEXT("Experience %s")
				, *AssetData.GetSoftObjectPath().ToString());
		}
#endif
	}

	{
		TArray<FAssetData> AssetList;
		UAssetManager::Get().GetPrimaryAssetDataList(
			FPrimaryAssetType(UArcUserFacingExperienceDefinition::StaticClass()->GetFName())
			,
			/*out*/ AssetList);
#if WITH_EDITORONLY_DATA
		for (const FAssetData& AssetData : AssetList)
		{
			UE_LOG(LogInit
				, Log
				, TEXT("User facing experience %s")
				, *AssetData.GetSoftObjectPath().ToString());
		}
#endif
	}
}

FPrimaryAssetId UArcExperienceManager::GetPrimaryAssetIdFromUserFacingExperienceName(
	const FString& AdvertisedExperienceID)
{
	const FPrimaryAssetType Type(UArcUserFacingExperienceDefinition::StaticClass()->GetFName());
	return FPrimaryAssetId(Type
		, FName(*AdvertisedExperienceID));
}
