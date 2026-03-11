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

#include "HAL/Platform.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FJsonObject;
class UScriptStruct;

/**
 * Describes a single exposed UPROPERTY on a fragment struct.
 * Auto-populated from UE reflection metadata.
 */
struct ARCCORE_API FArcFragmentPropertyDescription
{
	FName PropertyName;
	FString TypeName;
	FString Description;
	FString Category;

	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Rich description of a fragment struct, queryable at runtime.
 * Base fields are auto-populated from USTRUCT/UPROPERTY metadata.
 * Override GetDescription() on fragment subclasses to fill
 * CommonPairings, Prerequisites, and UsageNotes.
 */
struct ARCCORE_API FArcFragmentDescription
{
	FName StructName;
	FString DisplayName;
	FString Description;
	FString Category;

	TArray<FArcFragmentPropertyDescription> Properties;
	TArray<FName> CommonPairings;
	TArray<FName> Prerequisites;
	FString UsageNotes;

	TSharedPtr<FJsonObject> ToJson() const;

	/**
	 * Build a description by reading USTRUCT and UPROPERTY metadata via UE reflection.
	 * Populates StructName, DisplayName, Description, Category, and Properties.
	 * Does NOT populate CommonPairings, Prerequisites, or UsageNotes (those are override-only).
	 */
	static FArcFragmentDescription BuildBaseDescription(const UScriptStruct* InStruct);
};
