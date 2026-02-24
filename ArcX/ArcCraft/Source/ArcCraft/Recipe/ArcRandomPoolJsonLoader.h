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

#include "ArcRandomPoolJsonLoader.generated.h"

class UArcRandomPoolDefinition;

/**
 * Loads UArcRandomPoolDefinition assets from JSON files.
 */
UCLASS()
class ARCCRAFT_API UArcRandomPoolJsonLoader : public UObject
{
	GENERATED_BODY()

public:
	/** Load a random pool definition from a JSON file. */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Random Pools")
	static UArcRandomPoolDefinition* LoadFromFile(UObject* Outer, const FString& FilePath);

	/** Parse a JSON object into an existing UArcRandomPoolDefinition. Returns true on success. */
	static bool ParseJson(const nlohmann::json& JsonObj, UArcRandomPoolDefinition* OutPool);
};
