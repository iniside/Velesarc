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

#include "ArcExperienceDefinition.h"
#include "GameMode/ArcCoreGameMode.h"
#include "Components/ActorComponent.h"
#include "GameFeatureAction.h"
#include "Engine/AssetManager.h"
#include "GameMode/ArcCoreGameState.h"
#include "Player/ArcCorePlayerController.h"
#include "Misc/DataValidation.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "ArcExperienceDefinition"

UArcExperienceActionSet::UArcExperienceActionSet()
{
}

#if WITH_EDITOR
EDataValidationResult UArcExperienceActionSet::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			EDataValidationResult ChildResult = Action->IsDataValid(Context);
			Result = CombineDataValidationResults(Result, ChildResult);
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("ActionEntryIsNull", "Null entry at index {0} in Actions"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UArcExperienceActionSet::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->AddAdditionalAssetBundleData(AssetBundleData);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void UArcPlayerControllerData::GiveComponents(AArcCorePlayerController* InPlayerController) const
{
}

UArcExperienceDefinition::UArcExperienceDefinition()
{
	
}
#if WITH_EDITOR
EDataValidationResult UArcExperienceDefinition::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context)
		, EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			EDataValidationResult ChildResult = Action->IsDataValid(Context);
			Result = CombineDataValidationResults(Result, ChildResult);
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("ActionEntryIsNull", "Null entry at index {0} in Actions"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UArcExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->AddAdditionalAssetBundleData(AssetBundleData);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void UArcExperienceDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	if (ExperienceId.IsValid() == false)
	{
		ExperienceId = FGuid::NewGuid();
	}

#if WITH_EDITORONLY_DATA
	UpdateAssetBundleData();
	
	if (UAssetManager::IsInitialized())
	{
		// Bundles may have changed, refresh
		UAssetManager::Get().RefreshAssetData(this);
	}
#endif
	
	Super::PreSave(SaveContext);
}

FPrimaryAssetId UArcExperienceDefinition::GetPrimaryAssetId() const
{
	FPrimaryAssetId Id1 = FPrimaryAssetId(TEXT("ArcExperienceDefinition"), *ExperienceId.ToString());
	return Id1;
}


#undef LOCTEXT_NAMESPACE
