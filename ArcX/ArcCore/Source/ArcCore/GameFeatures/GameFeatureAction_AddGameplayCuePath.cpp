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

#include "GameFeatureAction_AddGameplayCuePath.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GameFeatures"

UGameFeatureAction_AddGameplayCuePath::UGameFeatureAction_AddGameplayCuePath()
{
	// Add a default path that is commonly used
	DirectoryPathsToAdd.Add(FDirectoryPath{TEXT("/GameplayCues")});
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddGameplayCuePath::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	FText ErrorReason = FText::GetEmpty();
	for (const FDirectoryPath& Directory : DirectoryPathsToAdd)
	{
		if (Directory.Path.IsEmpty())
		{
			const FText InvalidCuePathError = FText::Format(LOCTEXT("InvalidCuePathError"
					, "'{0}' is not a valid path!")
				, FText::FromString(Directory.Path));
			Context.AddError(InvalidCuePathError);
			Context.AddError(ErrorReason);
			Result = CombineDataValidationResults(Result
				, EDataValidationResult::Invalid);
		}
	}

	return CombineDataValidationResults(Result
		, EDataValidationResult::Valid);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
