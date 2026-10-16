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

#include "ArcCraft/Shared/ArcCraftRecords.h"

#include "Core/ArcCoreAssetManager.h"

namespace ArcCraftRecordsPrivate
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcCraftRecords, Log, All);

	TSet<FPrimaryAssetId> WarnedIds;
}

const void* ArcCraftRecords::FindRecord(const FPrimaryAssetId& Id, const UScriptStruct& Struct)
{
	if (!Id.IsValid())
	{
		return nullptr;
	}

	const void* Record = UArcCoreAssetManager::Get().FindJsonRecord(Id, Struct);
	if (Record == nullptr)
	{
		bool bAlreadyWarned = false;
		ArcCraftRecordsPrivate::WarnedIds.Add(Id, &bAlreadyWarned);
		if (!bAlreadyWarned)
		{
			UE_LOGF(ArcCraftRecordsPrivate::LogArcCraftRecords, Warning, "ArcCraft: record %ls (%ls) not found", *Id.ToString(), *Struct.GetName());
		}
	}
	return Record;
}
