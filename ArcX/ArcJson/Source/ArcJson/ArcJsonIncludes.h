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

#include "nlohmann/json.hpp"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectKey.h"


#include "ArcJsonIncludes.generated.h"


ARCJSON_API void PropertyToJson(FProperty* InProperty, void* Data, nlohmann::json& JsonObject);

ARCJSON_API nlohmann::json ArrayToJsonObject(FArrayProperty* InArray, void* Data);

ARCJSON_API void StructToJson(UStruct* Type, void* Data, nlohmann::json& JsonObject, nlohmann::json* ParentJsonObject);

ARCJSON_API FString ToJsonString(UStruct* Type, void* Data);

ARCJSON_API nlohmann::json ToJsonObject(UStruct* Type, void* Data);

ARCJSON_API void JsonToStruct(UStruct* Type, void* Data, nlohmann::json& Json, nlohmann::json* ParentJsonObject);

ARCJSON_API void FromJsonString(UStruct* Type, void* Data, const FString& JsonString);

ARCJSON_API void JsonArrayToProperty(FArrayProperty* InArray, void* Data, nlohmann::json& JsonObject);

ARCJSON_API void JsonToProperty(FProperty* InProperty, void* Data, nlohmann::json& JsonObject);

template<typename T>
FString ArrayToJsonString(TArray<T>& Data)
{
	nlohmann::json JsonObject;

	for (T& Val : Data)
	{
		nlohmann::json obj = ToJsonObject(T::StaticStruct(), &Val);
		JsonObject.push_back(obj);
	}

	FString str = JsonObject.dump().c_str();

	return str;
}

template<typename T>
TArray<T> JsonStringToArray(FString& Data)
{
	TArray<T> Out;
	nlohmann::json JsonObject = nlohmann::json::parse(TCHAR_TO_ANSI(*Data));;
	
	for (nlohmann::json::iterator it = JsonObject.begin(); it != JsonObject.end(); ++it)
	{
		T Value;
		JsonToStruct(T::StaticStruct(), &Value, *it, nullptr);
		Out.Add(Value);
	}
	
	return Out;;
}

USTRUCT()
struct ARCJSON_API FArcCustomSerializer
{
	GENERATED_BODY()

public:
	virtual ~FArcCustomSerializer() = default;

	virtual UStruct* GetSupportedType() const { return nullptr;; }
	
	virtual bool ToJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const { return false; };
	virtual bool FromJsonParent(void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject) const { return false; };
	
	virtual void ToJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const {}
	virtual void FromJson(void* Data, const FString& FieldName, nlohmann::json& JsonObject) const {}
};

class ARCJSON_API FArcJsonSerializers
{
public:
	static TMap<FObjectKey, FInstancedStruct> Serializers;

	static bool ToJsonParent(UStruct* Type, void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject);
	static bool FromJsonParent(UStruct* Type, void* Data, const FString& FieldName, nlohmann::json* ParentJsonObject);
	
	static bool ToJson(UStruct* Type, void* Data, const FString& FieldName, nlohmann::json& JsonObject);
	static bool FromJson(UStruct* Type, void* Data, const FString& FieldName, nlohmann::json& JsonObject);
};
