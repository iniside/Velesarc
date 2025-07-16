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
#include "AssetTypeActions_Base.h"

class FArcAssetTypeActions_ComboTreeData : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions interface
	virtual FText GetName() const override;

	virtual FColor GetTypeColor() const override;

	virtual UClass* GetSupportedClass() const override;

	// virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return
	// false; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects
								 , TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<
									 IToolkitHost>()) override;

	virtual uint32 GetCategories() override
	{
		return EAssetTypeCategories::Gameplay;
	}

	// End of IAssetTypeActions interface
};
