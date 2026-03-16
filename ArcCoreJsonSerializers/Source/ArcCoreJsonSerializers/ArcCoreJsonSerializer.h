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
#include "ArcJsonIncludes.h"

#include "ArcCoreJsonSerializer.generated.h"

USTRUCT()
struct FArcItemIdJsonSerializer: public FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcItemIdJsonSerializer() override = default;

	virtual UStruct* GetSupportedType() const override;

	virtual bool ToJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const override;;
	virtual bool FromJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const override;;
	
	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
};

USTRUCT()
struct FArcNamedPrimaryAssetIdJsonSerializer : public FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcNamedPrimaryAssetIdJsonSerializer() override = default;

	virtual UStruct* GetSupportedType() const override;

	virtual bool ToJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const override;;
	virtual bool FromJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const override;;
	
	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
};

USTRUCT()
struct FArcItemSpecJsonSerializer : public FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcItemSpecJsonSerializer() override = default;

	virtual UStruct* GetSupportedType() const override;

	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
};

USTRUCT()
struct FArcGameplayTagJsonSerializer : public FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcGameplayTagJsonSerializer() override = default;

	virtual UStruct* GetSupportedType() const override;

	virtual bool ToJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const override;;
	virtual bool FromJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const override;;
	
	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
};

USTRUCT()
struct FArcJsonSerializer_ItemCopyContainerHelper : public FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcJsonSerializer_ItemCopyContainerHelper() override = default;

	virtual UStruct* GetSupportedType() const override;

	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
};

USTRUCT()
struct FArcJsonSerializer_ItemInstance_ItemStats : public FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcJsonSerializer_ItemInstance_ItemStats() override = default;

	virtual UStruct* GetSupportedType() const override;

	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const override;
};