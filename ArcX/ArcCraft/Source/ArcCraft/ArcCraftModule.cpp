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

#include "ArcCraftModule.h"

#include "Engine/AssetManager.h"
#include "Recipe/ArcRecipeDefinition.h"

#define LOCTEXT_NAMESPACE "FArcCraftModule"

void FArcCraftModule::StartupModule()
{
	// Register UArcRecipeDefinition as a primary asset type so it can be discovered by the asset manager.
	if (UAssetManager::IsInitialized())
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		AssetManager.ScanPathForPrimaryAssets(
			FPrimaryAssetType(TEXT("ArcRecipe")),
			TEXT("/Game/"),
			UArcRecipeDefinition::StaticClass(),
			true /* bHasBlueprintClasses */,
			false /* bIsEditorOnly */,
			false /* bForceSynchronousScan */);
	}
}

void FArcCraftModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcCraftModule, ArcCraft)