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
#include "GameplayTagContainer.h"

/**
 * Abstract read interface for persistence deserialization.
 * Serializers read from concrete implementations of this archive.
 * ReadProperty methods return false if the key is missing; the output value is left unchanged.
 */
class ARCPERSISTENCE_API FArcLoadArchive
{
public:
	virtual ~FArcLoadArchive() = default;

	// ── Primitive property readers ──────────────────────────────────────

	virtual bool ReadProperty(FName Key, bool& OutValue) = 0;
	virtual bool ReadProperty(FName Key, int32& OutValue) = 0;
	virtual bool ReadProperty(FName Key, int64& OutValue) = 0;
	virtual bool ReadProperty(FName Key, uint8& OutValue) = 0;
	virtual bool ReadProperty(FName Key, uint16& OutValue) = 0;
	virtual bool ReadProperty(FName Key, uint32& OutValue) = 0;
	virtual bool ReadProperty(FName Key, uint64& OutValue) = 0;
	virtual bool ReadProperty(FName Key, float& OutValue) = 0;
	virtual bool ReadProperty(FName Key, double& OutValue) = 0;

	// ── String / name / text property readers ───────────────────────────

	virtual bool ReadProperty(FName Key, FString& OutValue) = 0;
	virtual bool ReadProperty(FName Key, FName& OutValue) = 0;
	virtual bool ReadProperty(FName Key, FText& OutValue) = 0;

	// ── Identifier property readers ─────────────────────────────────────

	virtual bool ReadProperty(FName Key, FGuid& OutValue) = 0;

	// ── Gameplay tag property readers ───────────────────────────────────

	virtual bool ReadProperty(FName Key, FGameplayTag& OutValue) = 0;
	virtual bool ReadProperty(FName Key, FGameplayTagContainer& OutValue) = 0;

	// ── Math property readers ───────────────────────────────────────────

	virtual bool ReadProperty(FName Key, FVector& OutValue) = 0;
	virtual bool ReadProperty(FName Key, FRotator& OutValue) = 0;
	virtual bool ReadProperty(FName Key, FTransform& OutValue) = 0;

	// ── Struct scope ────────────────────────────────────────────────────

	virtual bool BeginStruct(FName Key) = 0;
	virtual void EndStruct() = 0;

	// ── Array scope ─────────────────────────────────────────────────────

	virtual bool BeginArray(FName Key, int32& OutCount) = 0;
	virtual bool BeginArrayElement(int32 Index) = 0;
	virtual void EndArrayElement() = 0;
	virtual void EndArray() = 0;

	// ── Version ─────────────────────────────────────────────────────────

	uint32 GetVersion() const { return Version; }

	// ── Initialization ──────────────────────────────────────────────────

	/** Initialize the archive from a previously serialized byte buffer. Returns false on failure. */
	virtual bool InitializeFromData(const TArray<uint8>& Data) = 0;

protected:
	uint32 Version = 0;
};
