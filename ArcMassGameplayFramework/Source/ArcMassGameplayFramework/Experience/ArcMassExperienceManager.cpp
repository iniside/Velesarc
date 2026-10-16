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

#include "Experience/ArcMassExperienceManager.h"

#include "Experience/ArcMassExperienceDefinition.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"

void UArcMassExperienceManager::Initialize(FSubsystemCollectionBase& Collection)
{
	UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateUObject(this
		, &ThisClass::OnAssetManagerCreated));
}

#if WITH_EDITOR

void UArcMassExperienceManager::OnPlayInEditorBegun()
{
	ensure(ExperienceRequestCountMap.IsEmpty());
	ExperienceRequestCountMap.Empty();
}

void UArcMassExperienceManager::OnPlayInEditorFinished()
{
}

bool UArcMassExperienceManager::RequestToPerformActions(FPrimaryAssetId ExperienceID)
{
	if (GIsEditor)
	{
		UArcMassExperienceManager* Subsystem = GEngine->GetEngineSubsystem<UArcMassExperienceManager>();
		check(Subsystem);

		// Only the first PIE world to reach here activates the actions.
		int32& Count = Subsystem->ExperienceRequestCountMap.FindOrAdd(ExperienceID);
		++Count;
		return Count == 1;
	}

	return true;
}

bool UArcMassExperienceManager::RequestToRemoveActions(FPrimaryAssetId ExperienceID)
{
	if (GIsEditor)
	{
		UArcMassExperienceManager* Subsystem = GEngine->GetEngineSubsystem<UArcMassExperienceManager>();
		check(Subsystem);

		// Only the last PIE world to reach here deactivates the actions.
		int32& Count = Subsystem->ExperienceRequestCountMap.FindChecked(ExperienceID);
		--Count;

		if (Count == 0)
		{
			Subsystem->ExperienceRequestCountMap.Remove(ExperienceID);
			return true;
		}

		return false;
	}

	return true;
}

#endif

void UArcMassExperienceManager::OnAssetManagerCreated()
{
	TArray<FAssetData> AssetList;
	UAssetManager::Get().GetPrimaryAssetDataList(
		FPrimaryAssetType(UArcMassExperienceDefinition::StaticClass()->GetFName()),
		/*out*/ AssetList);

#if WITH_EDITORONLY_DATA
	for (const FAssetData& AssetData : AssetList)
	{
		UE_LOGF(LogInit
			, Log
			,"ArcMass Experience %ls", *AssetData.GetSoftObjectPath().ToString());
	}
#endif
}
