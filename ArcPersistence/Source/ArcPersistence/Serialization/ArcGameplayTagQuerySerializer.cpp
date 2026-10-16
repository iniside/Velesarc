/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
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

#include "GameplayTagContainer.h"
#include "Misc/OutputDeviceNull.h"
#include "Serialization/ArcLoadArchive.h"
#include "Serialization/ArcPersistenceSerializer.h"
#include "Serialization/ArcSaveArchive.h"

/** FGameplayTagQuery fields are not SaveGame, so the SaveGame-only nested plan would drop the whole query. */
struct FArcGameplayTagQueryPersistenceSerializer
{
	using SourceType = FGameplayTagQuery;
	static constexpr uint32 Version = 1;

	static void Save(const FGameplayTagQuery& Source, FArcSaveArchive& Ar)
	{
		FString Text;
		FGameplayTagQuery::StaticStruct()->ExportText(Text, &Source, nullptr, nullptr, PPF_None, nullptr);
		Ar.WriteProperty(FName(TEXT("Query")), Text);
	}

	static void Load(FGameplayTagQuery& Target, FArcLoadArchive& Ar)
	{
		FString Text;
		if (!Ar.ReadProperty(FName(TEXT("Query")), Text))
		{
			return;
		}

		Target = FGameplayTagQuery();
		FOutputDeviceNull NullOutput;
		FGameplayTagQuery::StaticStruct()->ImportText(*Text, &Target, nullptr, PPF_None, &NullOutput, FString(TEXT("GameplayTagQuery")));
	}
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcGameplayTagQueryPersistenceSerializer, )
UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcGameplayTagQueryPersistenceSerializer, "GameplayTagQuery")
