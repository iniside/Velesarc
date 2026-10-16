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

#include "ArcSaveArchive.h"

struct yyjson_mut_doc;
struct yyjson_mut_val;

/**
 * JSON-backed implementation of FArcSaveArchive.
 * Builds a yyjson mutable document, then serializes it to a pretty UTF-8 byte buffer on Finalize().
 */
class ARCPERSISTENCE_API FArcJsonSaveArchive : public FArcSaveArchive
{
public:
	FArcJsonSaveArchive();
	virtual ~FArcJsonSaveArchive() override;

	FArcJsonSaveArchive(FArcJsonSaveArchive&&);
	FArcJsonSaveArchive& operator=(FArcJsonSaveArchive&&);
	FArcJsonSaveArchive(const FArcJsonSaveArchive&) = delete;
	FArcJsonSaveArchive& operator=(const FArcJsonSaveArchive&) = delete;

	// ── Primitive property writers ──────────────────────────────────────

	virtual void WriteProperty(FName Key, bool Value) override;
	virtual void WriteProperty(FName Key, int32 Value) override;
	virtual void WriteProperty(FName Key, int64 Value) override;
	virtual void WriteProperty(FName Key, uint8 Value) override;
	virtual void WriteProperty(FName Key, uint16 Value) override;
	virtual void WriteProperty(FName Key, uint32 Value) override;
	virtual void WriteProperty(FName Key, uint64 Value) override;
	virtual void WriteProperty(FName Key, float Value) override;
	virtual void WriteProperty(FName Key, double Value) override;

	// ── String / name / text property writers ───────────────────────────

	virtual void WriteProperty(FName Key, const FString& Value) override;
	virtual void WriteProperty(FName Key, const FName& Value) override;
	virtual void WriteProperty(FName Key, const FText& Value) override;

	// ── Identifier property writers ─────────────────────────────────────

	virtual void WriteProperty(FName Key, const FGuid& Value) override;

	// ── Gameplay tag property writers ───────────────────────────────────

	virtual void WriteProperty(FName Key, const FGameplayTag& Value) override;
	virtual void WriteProperty(FName Key, const FGameplayTagContainer& Value) override;

	// ── Math property writers ───────────────────────────────────────────

	virtual void WriteProperty(FName Key, const FVector& Value) override;
	virtual void WriteProperty(FName Key, const FRotator& Value) override;
	virtual void WriteProperty(FName Key, const FTransform& Value) override;

	// ── Struct scope ────────────────────────────────────────────────────

	virtual void BeginStruct(FName Key) override;
	virtual void EndStruct() override;

	// ── Array scope ─────────────────────────────────────────────────────

	virtual void BeginArray(FName Key, int32 Count) override;
	virtual void BeginArrayElement(int32 Index) override;
	virtual void EndArrayElement() override;
	virtual void EndArray() override;

	// ── Finalization ────────────────────────────────────────────────────

	virtual TArray<uint8> Finalize() override;

private:
	struct FDocDeleter
	{
		void operator()(yyjson_mut_doc* InDoc) const;
	};

	yyjson_mut_val* Current() const;

	TUniquePtr<yyjson_mut_doc, FDocDeleter> Doc;
	yyjson_mut_val* Root = nullptr;
	yyjson_mut_val* Envelope = nullptr;
	TArray<yyjson_mut_val*> ScopeStack;
};
