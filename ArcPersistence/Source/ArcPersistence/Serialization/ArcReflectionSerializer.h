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

class FArcSaveArchive;
class FArcLoadArchive;
class UStruct;
class FProperty;

/**
 * Reflection-based serializer that walks all SaveGame-flagged properties of a UStruct.
 * Used as the fallback when no external serializer is registered for a type.
 *
 * All methods are static — this class is never instantiated.
 */
class ARCPERSISTENCE_API FArcReflectionSerializer
{
public:
	/** Serialize all SaveGame properties of a UStruct instance into the archive. */
	static void Save(const UStruct* Type, const void* Data, FArcSaveArchive& Ar);

	/** Deserialize all SaveGame properties of a UStruct instance from the archive. */
	static void Load(const UStruct* Type, void* Data, FArcLoadArchive& Ar);

	/** Compute a schema version hash from SaveGame property names and types. */
	static uint32 ComputeSchemaVersion(const UStruct* Type);

private:
	static void SaveProperty(FProperty* Prop, const void* ContainerData, FArcSaveArchive& Ar);
	static void LoadProperty(FProperty* Prop, void* ContainerData, FArcLoadArchive& Ar);

	/**
	 * Save/Load a single value given a raw pointer to the value (not a container).
	 * Used for array element dispatch where ContainerPtrToValuePtr is not applicable.
	 * The Key is used for struct/array nesting context within the current scope.
	 */
	static void SaveValueDirect(FProperty* Prop, FName Key, const void* ValuePtr, FArcSaveArchive& Ar);
	static void LoadValueDirect(FProperty* Prop, FName Key, void* ValuePtr, FArcLoadArchive& Ar);
};
