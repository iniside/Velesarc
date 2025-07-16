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
#include "ArcItemAttachmentComponent.h"
#include "Core/ArcCoreAssetManager.h"
#include "Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

class ArcEquipmentUtils
{
public:
	template<typename T>
	static const T* GetAttachmentFragment(const FArcItemData* InItem, UArcItemDefinition*& VisualItemDefinition)
	{
		const T* AttachmentData = ArcItems::GetFragment<T>(InItem);
		if (AttachmentData == nullptr)
		{
			const FArcItemFragment_ItemVisualAttachment* VisualItemData = ArcItems::GetFragment<FArcItemFragment_ItemVisualAttachment>(InItem);

			const FArcItemInstance_ItemVisualAttachment* VisualInstance = ArcItems::FindInstance<FArcItemInstance_ItemVisualAttachment>(InItem);
			
			if (VisualInstance != nullptr)
			{
				if (VisualInstance->VisualItem.IsValid() == true)
				{
					VisualItemDefinition = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualInstance->VisualItem);
				}
				else if (VisualItemData != nullptr)
				{
					VisualItemDefinition = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualItemData->DefaultVisualItem);
				}
			}
			else if (VisualItemData != nullptr)
			{
				VisualItemDefinition = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualItemData->DefaultVisualItem);
			}
			
			if (VisualItemDefinition)
			{
				AttachmentData = VisualItemDefinition->FindFragment<T>();
				return AttachmentData;
			}
		}

		return AttachmentData;
	}

	template<typename T>
	static const T* GetAttachmentFragment(const FArcItemAttachment* ItemAttachment)
	{
		if (ItemAttachment->VisualItemDefinition)
		{
			const T* AttachmentData = ItemAttachment->VisualItemDefinition->FindFragment<T>();
			if (AttachmentData)
			{
				return AttachmentData;
			}
		}
	
		const T* AttachmentData = ItemAttachment->ItemDefinition->FindFragment<T>();
		return AttachmentData;
		
	}
};
