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


#include "ArcItemFactory.h"
#include "HAL/UnrealMemory.h"
const FArcItemFactory* FArcItemFactory::Instance = nullptr;

FArcItemData FArcItemFactory::CreateItem(UScriptStruct* InFactoryType
										  , UArcItemFactoryData* InFactoryData
										  , APlayerController* InPC
										  , FArcItemData* InBaseItem
										  , struct FArcItemCreationContext* InContext) const
{
	if (const TSharedPtr<FArcItemFactoryBase>* F = Factories.Find(InFactoryType))
	{
		return (*F)->CreateItem(InFactoryData
			, InPC
			, InBaseItem
			, InContext);
	}
	return FArcItemData();
}

void FArcItemFactory::RegisterFactory(UScriptStruct* FactoryType) const
{
	if (Factories.Contains(FactoryType))
	{
		return;
	}
	void* OutData = FMemory::Malloc(FactoryType->GetCppStructOps()->GetSize());
	FactoryType->GetCppStructOps()->Construct(OutData);
	Factories.Add(FactoryType
		, MakeShareable(static_cast<FArcItemFactoryBase*>(OutData)));
}
