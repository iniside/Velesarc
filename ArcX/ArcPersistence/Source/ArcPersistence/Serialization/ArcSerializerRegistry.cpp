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

#include "Serialization/ArcSerializerRegistry.h"
#include "UObject/Class.h"

// ─────────────────────────────────────────────────────────────────────────────
// FArcPersistenceSerializerRegistryDelegates
// ─────────────────────────────────────────────────────────────────────────────

FArcPersistenceSerializerRegistryDelegates::FArcPersistenceSerializerRegistryDelegates() = default;
FArcPersistenceSerializerRegistryDelegates::~FArcPersistenceSerializerRegistryDelegates() = default;

// ─────────────────────────────────────────────────────────────────────────────
// FArcSerializerRegistry — singleton
// ─────────────────────────────────────────────────────────────────────────────

FArcSerializerRegistry& FArcSerializerRegistry::Get()
{
	static FArcSerializerRegistry Instance;
	return Instance;
}

void FArcSerializerRegistry::Register(FName StructName, const FArcPersistenceSerializerInfo& Info)
{
	Serializers.Add(StructName, Info);
}

void FArcSerializerRegistry::Unregister(FName StructName)
{
	Serializers.Remove(StructName);
}

const FArcPersistenceSerializerInfo* FArcSerializerRegistry::Find(const UStruct* Type) const
{
	for (const UStruct* Current = Type; Current != nullptr; Current = Current->GetSuperStruct())
	{
		if (const FArcPersistenceSerializerInfo* Found = Serializers.Find(Current->GetFName()))
		{
			return Found;
		}
	}
	return nullptr;
}

const FArcPersistenceSerializerInfo* FArcSerializerRegistry::FindByName(FName StructName) const
{
	return Serializers.Find(StructName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Free functions — delegates to singleton
// ─────────────────────────────────────────────────────────────────────────────

namespace ArcPersistence
{
	void RegisterSerializer(FName StructName, const FArcPersistenceSerializerInfo& Info)
	{
		FArcSerializerRegistry::Get().Register(StructName, Info);
	}

	void UnregisterSerializer(FName StructName)
	{
		FArcSerializerRegistry::Get().Unregister(StructName);
	}
}
