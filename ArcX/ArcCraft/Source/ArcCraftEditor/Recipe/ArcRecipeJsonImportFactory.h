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

#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "nlohmann/json.hpp"

#include "ArcRecipeJsonImportFactory.generated.h"

class UArcRecipeDefinition;

/**
 * Factory that imports JSON files into UArcRecipeDefinition assets.
 * Also serves as a reimport handler — stores the source JSON path on the asset
 * and can re-parse it to update the recipe.
 *
 * Reuses UArcRecipeJsonLoader::ParseJson() for parsing (single source of truth).
 *
 * Supported JSON formats:
 *   { "$type": "ArcRecipeDefinition", ... }                  (single recipe)
 *   [ { "$type": "ArcRecipeDefinition", ... }, ... ]          (array of recipes)
 */
UCLASS()
class ARCCRAFTEDITOR_API UArcRecipeJsonImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UArcRecipeJsonImportFactory(const FObjectInitializer& ObjectInitializer);

	// ---- UFactory interface ----

	virtual bool FactoryCanImport(const FString& Filename) override;

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent,
		FName InName, EObjectFlags Flags, const FString& Filename,
		const TCHAR* Parms, FFeedbackContext* Warn,
		bool& bOutOperationCanceled) override;

	// ---- FReimportHandler interface ----

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;

private:
	/**
	 * Find the JSON object in an array that matches an existing recipe's Id.
	 * Falls back to the first element if no Id match is found.
	 */
	static const nlohmann::json* FindMatchingRecipeNode(
		const nlohmann::json& JsonRoot,
		const FGuid& RecipeId);

	/**
	 * Reset a recipe definition to default values before re-parsing from JSON.
	 */
	static void ResetRecipeToDefaults(UArcRecipeDefinition* Recipe);
};
