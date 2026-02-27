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

#include "ArcJsonIncludes.h"

#include "StructUtils/InstancedStruct.h"

void PropertyToJson(FProperty* InProperty
	, void* Data
	, nlohmann::json& JsonObject)
{
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(InProperty))
	{
		nlohmann::json JsonObj = ArrayToJsonObject(ArrayProp, Data);
		JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())] = JsonObj;
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(InProperty))
	{
		int32 Val = *IntProp->ContainerPtrToValuePtr<int32>(Data);
		JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())] = Val;
	}
	else if (FInt8Property*  Int8Prop = CastField<FInt8Property>(InProperty))
	{
		int32 Val = *Int8Prop->ContainerPtrToValuePtr<int32>(Data);
		JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())] = Val;
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(InProperty))
	{
		uint8 Val = *ByteProp->ContainerPtrToValuePtr<uint8>(Data);
		JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())] = Val;
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(InProperty))
	{
		void* NewData = StructProp->ContainerPtrToValuePtr<void*>(Data);
		nlohmann::json JsonObj;

		const bool bParentSuccess = FArcJsonSerializers::ToJsonParent(StructProp->Struct, NewData, StructProp->GetName(), &JsonObject);
		if (bParentSuccess)
		{
			return;
		}

		bool bSuccess = FArcJsonSerializers::ToJson(StructProp->Struct, NewData, StructProp->GetName(), JsonObj);
		if (bSuccess)
		{
			JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())] = JsonObj;
		}
		else
		{
			StructToJson(StructProp->Struct, NewData, JsonObj, &JsonObject);
			JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())] = JsonObj;	
		}
		
	}
}

nlohmann::json ArrayToJsonObject(FArrayProperty* InArray, void* Data)
{
	FScriptArrayHelper Helper(InArray, Data);

	nlohmann::json JsonObject;
	int32 Num = Helper.Num();
	for (int32 Idx = 0; Idx < Num; Idx++)
	{
		void* Element = (void*)Helper.GetElementPtr(Idx);

		nlohmann::json ArrayElement;
		PropertyToJson(InArray->Inner, Element, ArrayElement);
		JsonObject.push_back(ArrayElement);
	}

	return JsonObject;
}

void StructToJson(UStruct* Type
				  , void* Data
				  , nlohmann::json& JsonObject
				  , nlohmann::json* ParentJsonObject)
{
	const bool bSuccess = FArcJsonSerializers::ToJson(Type, Data, {}, JsonObject);
	if (!bSuccess)
	{
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			PropertyToJson(*It, Data, JsonObject);
		}
	}
}

FString ToJsonString(UStruct* Type
	, void* Data)
{
	nlohmann::json JsonObject;
	
	StructToJson(Type, Data, JsonObject, nullptr);
	
	FString OutString;
	OutString = JsonObject.dump().c_str();

	return OutString;
}

nlohmann::json ToJsonObject(UStruct* Type
	, void* Data)
{
	nlohmann::json JsonObject;
	
	StructToJson(Type, Data, JsonObject, nullptr);
	
	return JsonObject;
}

void JsonArrayToProperty(FArrayProperty* InArray
	, void* Data
	, nlohmann::json& JsonObject)
{
	FScriptArrayHelper Helper(InArray, Data);
	
	for (nlohmann::json::iterator it = JsonObject[TCHAR_TO_ANSI(*InArray->GetName())].begin(); it != JsonObject[TCHAR_TO_ANSI(*InArray->GetName())].end(); ++it)
	{
		int32 Idx = Helper.AddValue();
		void* Value = Helper.GetElementPtr(Idx);
		JsonToProperty(InArray->Inner, Value, *it);
	}
}

void JsonToProperty(FProperty* InProperty
	, void* Data
	, nlohmann::json& JsonObject)
{
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(InProperty))
	{
		JsonArrayToProperty(ArrayProp, Data, JsonObject);
	}
	if (FIntProperty* IntProp = CastField<FIntProperty>(InProperty))
	{
		int32 Value = JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())].get<int32>();
		IntProp->SetValue_InContainer(Data, Value);
	}
	else if (FInt8Property*  Int8Prop = CastField<FInt8Property>(InProperty))
	{
		int8 Value = JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())].get<int8>();
		Int8Prop->SetValue_InContainer(Data, Value);
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(InProperty))
	{
		uint8 Value = JsonObject[TCHAR_TO_ANSI(*InProperty->GetName())].get<uint8>();
		ByteProp->SetValue_InContainer(Data, Value);
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(InProperty))
	{
		FString Name = InProperty->GetName();
		if (!JsonObject.contains(TCHAR_TO_ANSI(*InProperty->GetName())))
		{
			UE_LOG(LogTemp, Log, TEXT("Missing Key"));
			return;
		}
				
		void* NewData = StructProp->ContainerPtrToValuePtr<void*>(Data);
		bool bFromParentSuccess = FArcJsonSerializers::FromJsonParent(StructProp->Struct, NewData, Name, &JsonObject);
		if (bFromParentSuccess)
		{
			return;
		}

		bool bSuccessRead = FArcJsonSerializers::FromJson(StructProp->Struct, NewData, Name, JsonObject[TCHAR_TO_ANSI(*Name)]);
		if (!bSuccessRead)
		{
			JsonToStruct(StructProp->Struct, NewData, JsonObject[TCHAR_TO_ANSI(*Name)], &JsonObject);
		}
	}
}

void JsonToStruct(UStruct* Type
	, void* Data
	, nlohmann::json& Json
	, nlohmann::json* ParentJsonObject)
{
	bool bFromParentSuccess = FArcJsonSerializers::FromJsonParent(Type, Data, {}, ParentJsonObject);
	if (bFromParentSuccess)
	{
		return;
	}
	
	bool bSuccess = FArcJsonSerializers::FromJson(Type, Data, {}, Json);
	if (!bSuccess)
	{
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			JsonToProperty(*It, Data, Json);
		}
	}
}

void FromJsonString(UStruct* Type
	, void* Data
	, const FString& JsonString)
{
	nlohmann::json JsonObject;
	JsonObject = nlohmann::json::parse(TCHAR_TO_ANSI(*JsonString));

	JsonToStruct(Type, Data, JsonObject, nullptr);
}

bool FArcJsonSerializers::ToJsonParent(UStruct* Type
	, void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject)
{
	if (FInstancedStruct* Serialize = Serializers.Find(Type))
	{
		return Serialize->GetPtr<FArcCustomSerializer>()->ToJsonParent(Data, FieldName, ParentJsonObject);
	}

	return false;
}

bool FArcJsonSerializers::FromJsonParent(UStruct* Type
	, void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject)
{
	if (FInstancedStruct* Serialize = Serializers.Find(Type))
	{
		return Serialize->GetPtr<FArcCustomSerializer>()->FromJsonParent(Data, FieldName, ParentJsonObject);
	}

	return false;
}

bool FArcJsonSerializers::ToJson(UStruct* Type
								 , void* Data
								 , const FString& FieldName
								 , nlohmann::json& JsonObject)
{
	if (FInstancedStruct* Serialize = Serializers.Find(Type))
	{
		Serialize->GetPtr<FArcCustomSerializer>()->ToJson(Data, FieldName, JsonObject);
		return true;
	}

	return false;
}

bool FArcJsonSerializers::FromJson(UStruct* Type
	, void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject)
{
	if (FInstancedStruct* Serialize = Serializers.Find(Type))
	{
		Serialize->GetPtr<FArcCustomSerializer>()->FromJson(Data, FieldName, JsonObject);
		return true;
	}

	return false;
}


TMap<FObjectKey, FInstancedStruct> FArcJsonSerializers::Serializers = TMap<FObjectKey, FInstancedStruct>();