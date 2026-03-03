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

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArcPersistenceSerializer.h"

class UStruct;

/**
 * Singleton registry of external persistence serializers.
 * When a type has a registered serializer, it is used instead of the reflection fallback.
 * Lookup walks the UStruct inheritance chain, so registering a serializer for a base type
 * also covers derived types (unless they have their own registration).
 */
class ARCPERSISTENCE_API FArcSerializerRegistry
{
public:
	/** Get the singleton instance. */
	static FArcSerializerRegistry& Get();

	/** Register an external serializer for a struct by name. */
	void Register(FName StructName, const FArcPersistenceSerializerInfo& Info);

	/** Remove a previously registered serializer. */
	void Unregister(FName StructName);

	/**
	 * Find a registered serializer for the given UStruct type.
	 * Walks the super-struct chain from most-derived to base.
	 * @return Pointer to the serializer info, or nullptr if none registered (use reflection fallback).
	 */
	const FArcPersistenceSerializerInfo* Find(const UStruct* Type) const;

	/**
	 * Find a serializer for the given type, falling back to a reflection-based
	 * default if no custom serializer is registered. Always returns non-null.
	 * The reflection fallback captures the UStruct* and delegates to FArcReflectionSerializer.
	 */
	const FArcPersistenceSerializerInfo* FindOrDefault(const UStruct* Type) const;

	/** Direct lookup by FName without inheritance walking. */
	const FArcPersistenceSerializerInfo* FindByName(FName StructName) const;

private:
	TMap<FName, FArcPersistenceSerializerInfo> Serializers;

	/** Transient reflection fallback, rebuilt per FindOrDefault call. */
	mutable FArcPersistenceSerializerInfo ReflectionFallback;
};
