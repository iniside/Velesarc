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



#pragma once


#include "ArcItemsArray.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArcItemsStoreSubsystem.generated.h"

struct FArcItemData;
struct FArcItemId;
struct FArcItemCopyContainerHelper;
struct FArcItemSpec;
class UArcItemsStoreComponent;

struct FArcItemDefinitionData
{
	FPrimaryAssetId ItemDefinitionId;
	int32 StackSize;

	TArray<FArcItemId> AttachedItems;
	FArcItemId OwnerId;
};

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcItemsStoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void SetupItem(FArcItemData* ItemPtr, const FArcItemSpec& InSpec);
	
	FArcItemId AddItem(const FArcItemSpec& InItem, const FArcItemId& OwnerItemId, UArcItemsStoreComponent* InComponent);
	FArcItemId AddItem(FArcItemCopyContainerHelper& InContainer);
	
	FArcItemCopyContainerHelper GetItem(const FArcItemId& InItemId) const;

	const FArcItemData* GetItemPtr(const FArcItemId& InItemId) const;
	FArcItemData* GetItemPtr(const FArcItemId& InItemId);

	// Instanced items.
	TMap<FArcItemId, TSharedPtr<FArcItemData>> ItemsMap;

	TMap<FArcItemId, FArcItemDefinitionData> ItemDefinitionsMap;
};
