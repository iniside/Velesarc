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

#include "ArcLoadArchive.h"
#include "ArcJsonIncludes.h"

/**
 * JSON-backed implementation of FArcLoadArchive.
 * Parses a UTF-8 JSON byte buffer and provides read access to the stored properties.
 */
class ARCPERSISTENCE_API FArcJsonLoadArchive : public FArcLoadArchive
{
public:
	FArcJsonLoadArchive() = default;
	virtual ~FArcJsonLoadArchive() override = default;

	// ── Initialization ──────────────────────────────────────────────────

	virtual bool InitializeFromData(const TArray<uint8>& Data) override;

	// ── Primitive property readers ──────────────────────────────────────

	virtual bool ReadProperty(FName Key, bool& OutValue) override;
	virtual bool ReadProperty(FName Key, int32& OutValue) override;
	virtual bool ReadProperty(FName Key, int64& OutValue) override;
	virtual bool ReadProperty(FName Key, uint8& OutValue) override;
	virtual bool ReadProperty(FName Key, uint16& OutValue) override;
	virtual bool ReadProperty(FName Key, uint32& OutValue) override;
	virtual bool ReadProperty(FName Key, uint64& OutValue) override;
	virtual bool ReadProperty(FName Key, float& OutValue) override;
	virtual bool ReadProperty(FName Key, double& OutValue) override;

	// ── String / name / text property readers ───────────────────────────

	virtual bool ReadProperty(FName Key, FString& OutValue) override;
	virtual bool ReadProperty(FName Key, FName& OutValue) override;
	virtual bool ReadProperty(FName Key, FText& OutValue) override;

	// ── Identifier property readers ─────────────────────────────────────

	virtual bool ReadProperty(FName Key, FGuid& OutValue) override;

	// ── Gameplay tag property readers ───────────────────────────────────

	virtual bool ReadProperty(FName Key, FGameplayTag& OutValue) override;
	virtual bool ReadProperty(FName Key, FGameplayTagContainer& OutValue) override;

	// ── Math property readers ───────────────────────────────────────────

	virtual bool ReadProperty(FName Key, FVector& OutValue) override;
	virtual bool ReadProperty(FName Key, FRotator& OutValue) override;
	virtual bool ReadProperty(FName Key, FTransform& OutValue) override;

	// ── Struct scope ────────────────────────────────────────────────────

	virtual bool BeginStruct(FName Key) override;
	virtual void EndStruct() override;

	// ── Array scope ─────────────────────────────────────────────────────

	virtual bool BeginArray(FName Key, int32& OutCount) override;
	virtual bool BeginArrayElement(int32 Index) override;
	virtual void EndArrayElement() override;
	virtual void EndArray() override;

	// ── Public accessor (testing) ───────────────────────────────────────

	const nlohmann::json& GetJsonObject() const { return Root; }

private:
	/** Returns the current read target: top of ScopeStack, or &Root if stack is empty. */
	nlohmann::json* Current();

	/** The parsed "data" object from the envelope. */
	nlohmann::json Root;

	/** Stack of pointers into the JSON tree for nested struct/array scoping. */
	TArray<nlohmann::json*> ScopeStack;
};
