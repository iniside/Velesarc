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

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "Items/ArcItemDefinition.h"
#include "ArcAssetDefinition_ArcItemDefinitionTemplate.generated.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_ArcCore"
/**
 * 
 */
UCLASS()
class ARCCOREEDITOR_API UArcAssetDefinition_ArcItemDefinitionTemplate : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("ArcItemDefinitionTemplate", "Arc Item Definition Template");
	}

	virtual FText GetAssetDisplayName(const FAssetData& AssetData) const override;

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(221, 67, 21));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcItemDefinitionTemplate::StaticClass();
	}
	
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("Gameplay"
				, "Gameplay"))
		};

		return AssetPaths;
	}

	//virtual EAssetCommandResult PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const override;
	//
	//virtual bool CanMerge() const override;
	//virtual EAssetCommandResult Merge(const FAssetAutomaticMergeArgs& MergeArgs) const override;
	//virtual EAssetCommandResult Merge(const FAssetManualMergeArgs& MergeArgs) const override;
};

#undef LOCTEXT_NAMESPACE