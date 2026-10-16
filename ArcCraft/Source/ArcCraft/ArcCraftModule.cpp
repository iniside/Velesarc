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

#include "Engine/GameInstance.h"
#include "Recipe/ArcRecipeDefinitionData.h"
#include "MaterialCraft/ArcMaterialPropertyTableData.h"
#include "Recipe/ArcRandomPoolDefinitionData.h"
#include "StatTransfer/ArcStatTransferTableData.h"
#include "Static/ArcStaticModifierSetData.h"
#include "Rarity/ArcCraftRulesData.h"
#include "Rarity/ArcStatCatalogData.h"
#include "JsonAssets/ArcJsonAssetType.h"

#define LOCTEXT_NAMESPACE "FArcCraftModule"

void FArcCraftModule::StartupModule()
{
	FArcJsonAssetTypeInfo RecipeTypeInfo;
	RecipeTypeInfo.Extension = TEXT("arcrecipe");
	RecipeTypeInfo.Struct = FArcRecipeDefinitionData::StaticStruct();
	RecipeTypeInfo.PrimaryAssetType = FArcRecipeDefinitionData::JsonPrimaryAssetType();
	RecipeTypeInfo.TypeProperty = GET_MEMBER_NAME_CHECKED(FArcRecipeDefinitionData, RecipeType);
	RecipeTypeInfo.IdProperty = TEXT("RecipeId");
	RecipeTypeInfo.DisplayName = LOCTEXT("RecipeJsonDisplayName", "Recipe");
	RecipeTypeInfo.Color = FLinearColor(0.9f, 0.55f, 0.15f);
	ArcJsonAssets::RegisterType(RecipeTypeInfo);

	FArcJsonAssetTypeInfo MaterialTypeInfo;
	MaterialTypeInfo.Extension = TEXT("arcmaterialtable");
	MaterialTypeInfo.Struct = FArcMaterialPropertyTableData::StaticStruct();
	MaterialTypeInfo.PrimaryAssetType = FPrimaryAssetType(TEXT("ArcMaterialPropertyTable"));
	MaterialTypeInfo.IdProperty = TEXT("TableId");
	MaterialTypeInfo.DisplayName = LOCTEXT("MaterialJsonDisplayName", "Material Property Table");
	MaterialTypeInfo.Color = FLinearColor(FColor(72, 145, 191));
	ArcJsonAssets::RegisterType(MaterialTypeInfo);

	FArcJsonAssetTypeInfo PoolTypeInfo;
	PoolTypeInfo.Extension = TEXT("arcrandompool");
	PoolTypeInfo.Struct = FArcRandomPoolDefinitionData::StaticStruct();
	PoolTypeInfo.PrimaryAssetType = FPrimaryAssetType(TEXT("ArcRandomPool"));
	PoolTypeInfo.IdProperty = TEXT("PoolId");
	PoolTypeInfo.DisplayName = LOCTEXT("PoolJsonDisplayName", "Random Pool");
	PoolTypeInfo.Color = FLinearColor(FColor(145, 72, 191));
	ArcJsonAssets::RegisterType(PoolTypeInfo);

	FArcJsonAssetTypeInfo StatTransferTypeInfo;
	StatTransferTypeInfo.Extension = TEXT("arcstattransfer");
	StatTransferTypeInfo.Struct = FArcStatTransferTableData::StaticStruct();
	StatTransferTypeInfo.PrimaryAssetType = FPrimaryAssetType(TEXT("ArcStatTransferTable"));
	StatTransferTypeInfo.IdProperty = TEXT("TableId");
	StatTransferTypeInfo.DisplayName = LOCTEXT("StatTransferJsonDisplayName", "Stat Transfer Table");
	StatTransferTypeInfo.Color = FLinearColor(FColor(191, 145, 72));
	ArcJsonAssets::RegisterType(StatTransferTypeInfo);

	FArcJsonAssetTypeInfo StaticModsTypeInfo;
	StaticModsTypeInfo.Extension = TEXT("arcstaticmods");
	StaticModsTypeInfo.Struct = FArcStaticModifierSetData::StaticStruct();
	StaticModsTypeInfo.PrimaryAssetType = FPrimaryAssetType(TEXT("ArcStaticModifierSet"));
	StaticModsTypeInfo.IdProperty = TEXT("SetId");
	StaticModsTypeInfo.DisplayName = LOCTEXT("StaticModsJsonDisplayName", "Static Modifier Set");
	StaticModsTypeInfo.Color = FLinearColor(FColor(145, 72, 191));
	ArcJsonAssets::RegisterType(StaticModsTypeInfo);

	FArcJsonAssetTypeInfo RulesTypeInfo;
	RulesTypeInfo.Extension = TEXT("arccraftrules");
	RulesTypeInfo.Struct = FArcCraftRulesData::StaticStruct();
	RulesTypeInfo.PrimaryAssetType = FPrimaryAssetType(TEXT("ArcCraftRules"));
	RulesTypeInfo.IdProperty = TEXT("RulesId");
	RulesTypeInfo.DisplayName = LOCTEXT("RulesJsonDisplayName", "Craft Rules");
	RulesTypeInfo.Color = FLinearColor(FColor(191, 72, 72));
	ArcJsonAssets::RegisterType(RulesTypeInfo);

	FArcJsonAssetTypeInfo StatCatalogTypeInfo;
	StatCatalogTypeInfo.Extension = TEXT("arcstatcatalog");
	StatCatalogTypeInfo.Struct = FArcStatCatalogData::StaticStruct();
	StatCatalogTypeInfo.PrimaryAssetType = FPrimaryAssetType(TEXT("ArcStatCatalog"));
	StatCatalogTypeInfo.IdProperty = TEXT("CatalogId");
	StatCatalogTypeInfo.DisplayName = LOCTEXT("StatCatalogJsonDisplayName", "Stat Catalog");
	StatCatalogTypeInfo.Color = FLinearColor(FColor(72, 191, 145));
	ArcJsonAssets::RegisterType(StatCatalogTypeInfo);
}

void FArcCraftModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	ArcJsonAssets::UnregisterType(TEXT("arcrecipe"));
	ArcJsonAssets::UnregisterType(TEXT("arcmaterialtable"));
	ArcJsonAssets::UnregisterType(TEXT("arcrandompool"));
	ArcJsonAssets::UnregisterType(TEXT("arcstattransfer"));
	ArcJsonAssets::UnregisterType(TEXT("arcstaticmods"));
	ArcJsonAssets::UnregisterType(TEXT("arccraftrules"));
	ArcJsonAssets::UnregisterType(TEXT("arcstatcatalog"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcCraftModule, ArcCraft)