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
#include "ArcJsonIncludes.h"

#include "ArcMaterialPropertyTableJsonLoader.generated.h"

class UArcMaterialPropertyTable;
class UArcQualityBandPreset;

/**
 * Loads UArcMaterialPropertyTable and UArcQualityBandPreset assets from JSON files.
 */
UCLASS()
class ARCCRAFT_API UArcMaterialPropertyTableJsonLoader : public UObject
{
	GENERATED_BODY()

public:
	/** Load a material property table from a JSON file. */
	static UArcMaterialPropertyTable* LoadFromFile(UObject* Outer, const FString& FilePath);

	/** Parse a JSON object into an existing UArcMaterialPropertyTable. Returns true on success. */
	static bool ParseJson(const nlohmann::json& JsonObj, UArcMaterialPropertyTable* OutTable);

	/** Load a quality band preset from a JSON file. */
	static UArcQualityBandPreset* LoadPresetFromFile(UObject* Outer, const FString& FilePath);

	/** Parse a JSON object into an existing UArcQualityBandPreset. Returns true on success. */
	static bool ParsePresetJson(const nlohmann::json& JsonObj, UArcQualityBandPreset* OutPreset);
};
