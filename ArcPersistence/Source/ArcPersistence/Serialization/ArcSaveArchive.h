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
 * Abstract write interface for persistence serialization.
 * Serializers write to concrete implementations of this archive.
 */
class ARCPERSISTENCE_API FArcSaveArchive
{
public:
	virtual ~FArcSaveArchive() = default;

	// ── Primitive property writers ──────────────────────────────────────

	virtual void WriteProperty(FName Key, bool Value) = 0;
	virtual void WriteProperty(FName Key, int32 Value) = 0;
	virtual void WriteProperty(FName Key, int64 Value) = 0;
	virtual void WriteProperty(FName Key, uint8 Value) = 0;
	virtual void WriteProperty(FName Key, uint16 Value) = 0;
	virtual void WriteProperty(FName Key, uint32 Value) = 0;
	virtual void WriteProperty(FName Key, uint64 Value) = 0;
	virtual void WriteProperty(FName Key, float Value) = 0;
	virtual void WriteProperty(FName Key, double Value) = 0;

	// ── String / name / text property writers ───────────────────────────

	virtual void WriteProperty(FName Key, const FString& Value) = 0;
	virtual void WriteProperty(FName Key, const FName& Value) = 0;
	virtual void WriteProperty(FName Key, const FText& Value) = 0;

	// ── Identifier property writers ─────────────────────────────────────

	virtual void WriteProperty(FName Key, const FGuid& Value) = 0;

	// ── Gameplay tag property writers ───────────────────────────────────

	virtual void WriteProperty(FName Key, const FGameplayTag& Value) = 0;
	virtual void WriteProperty(FName Key, const FGameplayTagContainer& Value) = 0;

	// ── Math property writers ───────────────────────────────────────────

	virtual void WriteProperty(FName Key, const FVector& Value) = 0;
	virtual void WriteProperty(FName Key, const FRotator& Value) = 0;
	virtual void WriteProperty(FName Key, const FTransform& Value) = 0;

	// ── Struct scope ────────────────────────────────────────────────────

	virtual void BeginStruct(FName Key) = 0;
	virtual void EndStruct() = 0;

	// ── Array scope ─────────────────────────────────────────────────────

	virtual void BeginArray(FName Key, int32 Count) = 0;
	virtual void BeginArrayElement(int32 Index) = 0;
	virtual void EndArrayElement() = 0;
	virtual void EndArray() = 0;

	// ── Version ─────────────────────────────────────────────────────────

	void SetVersion(uint32 InVersion) { Version = InVersion; }
	uint32 GetVersion() const { return Version; }

	// ── Finalization ────────────────────────────────────────────────────

	/** Finalize the archive and return the serialized data as a byte buffer. */
	virtual TArray<uint8> Finalize() = 0;

protected:
	uint32 Version = 0;
};
