/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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
#include "Engine/DataAsset.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"

#include "ArcQualityBandPreset.generated.h"

class UAssetImportData;

/**
 * Reusable quality band definitions that can be shared across multiple rules.
 * When referenced by a FArcMaterialPropertyRule via QualityBandPreset,
 * these bands are used instead of the rule's inline QualityBands.
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCCRAFT_API UArcQualityBandPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Display name for editor and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FText PresetName;

	/** Quality bands, ordered from lowest to highest quality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	TArray<FArcMaterialQualityBand> QualityBands;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "ImportSettings")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

public:
	virtual void PostInitProperties() override;

	UFUNCTION(CallInEditor, Category = "Preset")
	void ExportToJson();

	UAssetImportData* GetAssetImportData() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
