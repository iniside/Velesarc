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

#include "Pawn/ArcMassPawnData.h"
#include "Engine/AssetManager.h"

UArcMassPawnData::UArcMassPawnData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FPrimaryAssetId UArcMassPawnData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ArcMassPawnData"), GetFName());
}

#if WITH_EDITORONLY_DATA
void UArcMassPawnData::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();
	if (UAssetManager::IsInitialized() == false)
	{
		return;
	}
	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FInstancedStruct& IS : InitFragments)
	{
		if (IS.IsValid() == false)
		{
			continue;
		}
		AssetManager.InitializeAssetBundlesFromMetadata(IS.GetScriptStruct(), IS.GetMemory(), AssetBundleData, IS.GetScriptStruct()->GetFName());
	}
	for (const FInstancedStruct& IS : Components)
	{
		if (IS.IsValid() == false)
		{
			continue;
		}
		AssetManager.InitializeAssetBundlesFromMetadata(IS.GetScriptStruct(), IS.GetMemory(), AssetBundleData, IS.GetScriptStruct()->GetFName());
	}
}
#endif
