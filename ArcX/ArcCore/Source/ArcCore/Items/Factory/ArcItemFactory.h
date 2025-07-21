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
#include "Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemData.h"

#include "ArcItemFactory.generated.h"

struct ARCCORE_API FArcItemCreationContext
{
};

USTRUCT()
struct ARCCORE_API FArcItemFactoryBase
{
	GENERATED_BODY()

	virtual FArcItemData CreateItem(class UArcItemFactoryData* InFactoryData
									 , class APlayerController* InPC
									 , struct FArcItemData* InBaseItem
									 , struct FArcItemCreationContext* InContext) const
	{
		return FArcItemData();
	}

	virtual ~FArcItemFactoryBase()
	{
	}
};

class ARCCORE_API FArcItemFactory
{
private:
	static const FArcItemFactory* Instance;

	mutable TMap<UScriptStruct*, TSharedPtr<FArcItemFactoryBase>> Factories;

public:
	static const FArcItemFactory* Get()
	{
		if (Instance == nullptr)
		{
			Instance = new FArcItemFactory();
			return Instance;
		}
		return Instance;
	}

	FArcItemData CreateItem(UScriptStruct* InFactoryType
							 , class UArcItemFactoryData* InFactoryData
							 , class APlayerController* InPC
							 , struct FArcItemData* InBaseItem
							 , struct FArcItemCreationContext* InContext) const;

	void RegisterFactory(UScriptStruct* FactoryType) const;

	template <typename TFactory>
	void RegisterFactory() const
	{
		if (Factories.Contains(TFactory::StaticStruct()))
		{
			return;
		}
		void* OutData = FMemory::Malloc(TFactory::StaticStruct()->GetCppStructOps()->GetSize());
		TFactory::StaticStruct()->GetCppStructOps()->Construct(OutData);
		Factories.Add(TFactory::StaticStruct()
			, MakeShareable(static_cast<FArcItemFactoryBase*>(OutData)));
	}
};
