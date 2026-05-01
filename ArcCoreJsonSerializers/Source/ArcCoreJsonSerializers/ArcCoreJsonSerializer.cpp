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

#include "ArcCoreJsonSerializer.h"

#include "ArcNamedPrimaryAssetId.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

UStruct* FArcItemIdJsonSerializer::GetSupportedType() const
{
	return FArcItemId::StaticStruct();
}

bool FArcItemIdJsonSerializer::ToJsonParent(void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject) const
{
	if (ParentJsonObject)
	{
		FArcItemId* Id = static_cast<FArcItemId*>(Data);
		(*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)] = std::string(TCHAR_TO_ANSI(*Id->GetGuid().ToString(EGuidFormats::DigitsLower)));
		
		return true;
	}
	
	return false;
}

bool FArcItemIdJsonSerializer::FromJsonParent(void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject) const
{
	if (ParentJsonObject)
	{
		FArcItemId* Id = static_cast<FArcItemId*>(Data);
		FGuid GuidId;
		FString str = (*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)].get<std::string>().c_str(); 
		FGuid::Parse(str, GuidId);
		(*Id) = GuidId;

		return true;
	}
	
	return false;
}

void FArcItemIdJsonSerializer::ToJson(void* Data
									  , const FString& FieldName
									  , nlohmann::json& JsonObject
	) const
{
	FArcItemId* Id = static_cast<FArcItemId*>(Data);
	JsonObject["Id"] = std::string(TCHAR_TO_ANSI(*Id->GetGuid().ToString(EGuidFormats::DigitsLower)));
}

void FArcItemIdJsonSerializer::FromJson(void* Data
, const FString& FieldName
	, nlohmann::json& JsonObject
	) const
{
	FArcItemId* Id = static_cast<FArcItemId*>(Data);
	FGuid GuidId;
	FString str = JsonObject["Id"].get<std::string>().c_str(); 
	FGuid::Parse(str, GuidId);
	(*Id) = GuidId;
}

UStruct* FArcNamedPrimaryAssetIdJsonSerializer::GetSupportedType() const
{
	return FArcNamedPrimaryAssetId::StaticStruct();
}

bool FArcNamedPrimaryAssetIdJsonSerializer::ToJsonParent(void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject) const
{
	FArcNamedPrimaryAssetId* Id = static_cast<FArcNamedPrimaryAssetId*>(Data);
	if (ParentJsonObject)
	{
		(*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)] = std::string(TCHAR_TO_ANSI(*Id->AssetId.ToString()));
		
		return true;
	}
	
	return false;
}

bool FArcNamedPrimaryAssetIdJsonSerializer::FromJsonParent(void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject) const
{
	FArcNamedPrimaryAssetId* Id = static_cast<FArcNamedPrimaryAssetId*>(Data);
	if (ParentJsonObject)
	{
		FString AssetId = (*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)].get<std::string>().c_str();
		Id->AssetId = FPrimaryAssetId::FromString(AssetId);

		return true;
	}
	return false;
}

void FArcNamedPrimaryAssetIdJsonSerializer::ToJson(void* Data
												   , const FString& FieldName
												   , nlohmann::json& JsonObject
	) const
{
	FArcNamedPrimaryAssetId* Id = static_cast<FArcNamedPrimaryAssetId*>(Data);
	JsonObject["AssetId"] = std::string(TCHAR_TO_ANSI(*Id->AssetId.ToString()));
}

void FArcNamedPrimaryAssetIdJsonSerializer::FromJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	
	) const
{
	FArcNamedPrimaryAssetId* Id = static_cast<FArcNamedPrimaryAssetId*>(Data);
	
	FString AssetId = JsonObject["AssetId"].get<std::string>().c_str();
	Id->AssetId = FPrimaryAssetId::FromString(AssetId);	
	
}

UStruct* FArcItemSpecJsonSerializer::GetSupportedType() const
{
	return FArcItemSpec::StaticStruct();
}

void FArcItemSpecJsonSerializer::ToJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	) const
{
	FArcItemSpec* Spec = static_cast<FArcItemSpec*>(Data);
	JsonObject["ItemId"] = std::string(TCHAR_TO_ANSI(*Spec->ItemId.ToString()));
	JsonObject["Amount"] = Spec->Amount;
	JsonObject["Level"] = Spec->Level;
	JsonObject["ItemDefinitionId"] = std::string(TCHAR_TO_ANSI(*Spec->ItemDefinitionId.ToString()));

	for (const FInstancedStruct& Inst : Spec->InitialInstanceData)
	{
		if (!Inst.IsValid())
		{
			continue;
		}
		nlohmann::json InstanceData;
		const UScriptStruct* ScriptStruct = Inst.GetScriptStruct();
		InstanceData["StructType"] = TCHAR_TO_ANSI(*ScriptStruct->GetStructPathName().ToString());
		FArcJsonSerializers::ToJson(const_cast<UScriptStruct*>(ScriptStruct), const_cast<uint8*>(Inst.GetMemory()), {}, InstanceData);

		JsonObject["InitialInstanceData"].push_back(InstanceData);
	}
}

void FArcItemSpecJsonSerializer::FromJson(void* Data
, const FString& FieldName
	, nlohmann::json& JsonObject
	) const
{
	FArcItemSpec* Spec = static_cast<FArcItemSpec*>(Data);
	Spec->ItemId.FromString(JsonObject["ItemId"].get<std::string>().c_str());
	Spec->Amount = JsonObject["Amount"].get<int32>();
	Spec->Level = JsonObject["Level"].get<uint8>();
	Spec->ItemDefinitionId.FromString(JsonObject["ItemDefinitionId"].get<std::string>().c_str());

	nlohmann::json InitialInstaces = JsonObject["InitialInstanceData"];
	for (nlohmann::json::iterator it = InitialInstaces.begin(); it != InitialInstaces.end(); ++it)
	{
		FString StructType = (*it)["StructType"].get<std::string>().c_str();
		FTopLevelAssetPath Path(StructType);
		UScriptStruct* ScriptStruct = FindObjectSafe<UScriptStruct>(Path);

		FInstancedStruct Instance;
		Instance.InitializeAs(ScriptStruct);
		FArcJsonSerializers::FromJson(ScriptStruct, Instance.GetMutableMemory(), {}, *it);

		Spec->InitialInstanceData.Add(MoveTemp(Instance));
	}
}

UStruct* FArcGameplayTagJsonSerializer::GetSupportedType() const
{
	return FGameplayTag::StaticStruct();
}

bool FArcGameplayTagJsonSerializer::ToJsonParent(void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject) const
{
	FGameplayTag* Tag = static_cast<FGameplayTag*>(Data);
	FString Name = Tag->ToString();
	if (ParentJsonObject && FieldName.Len())
	{
		if (Tag->IsValid())
		{
			(*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)] = TCHAR_TO_ANSI(*Tag->ToString());	
		}
		else
		{
			(*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)] = "";
		}
		
		return true;
	}
	if (ParentJsonObject)
	{
		if (Tag->IsValid())
		{
			(*ParentJsonObject)["Tag"] = TCHAR_TO_ANSI(*Tag->ToString());	
		}
		else
		{
			(*ParentJsonObject)["Tag"] = "";	
		}
		
		return true;
	}
	
	return false;
}

bool FArcGameplayTagJsonSerializer::FromJsonParent(void* Data
	, const FString& FieldName
	, nlohmann::json* ParentJsonObject) const
{
	FGameplayTag* Tag = static_cast<FGameplayTag*>(Data);
	FString TagName;
	
	if (ParentJsonObject && FieldName.Len())
	{
		TagName = (*ParentJsonObject)[TCHAR_TO_ANSI(*FieldName)].get<std::string>().c_str();

		if (TagName.Len() > 0)
		{
			*Tag = FGameplayTag::RequestGameplayTag(*TagName);	
		}
		
		return true;
	}
	if (ParentJsonObject)
	{
		TagName = (*ParentJsonObject)["Tag"].get<std::string>().c_str();
		if (TagName.Len() > 0)
		{
			*Tag = FGameplayTag::RequestGameplayTag(*TagName);
		}
		
		return true;
	}
	
	return false;
}

void FArcGameplayTagJsonSerializer::ToJson(void* Data
										   , const FString& FieldName
										   , nlohmann::json& JsonObject
	) const
{
	FGameplayTag* Tag = static_cast<FGameplayTag*>(Data);
	JsonObject["Tag"] = TCHAR_TO_ANSI(*Tag->ToString());
}

void FArcGameplayTagJsonSerializer::FromJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	
	) const
{
	FGameplayTag* Tag = static_cast<FGameplayTag*>(Data);
	FString TagName;
	
	TagName = JsonObject["Tag"].get<std::string>().c_str();
	
	*Tag = FGameplayTag::RequestGameplayTag(*TagName);
}

UStruct* FArcJsonSerializer_ItemCopyContainerHelper::GetSupportedType() const
{
	return FArcCustomSerializer::GetSupportedType();
}

void FArcJsonSerializer_ItemCopyContainerHelper::ToJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	) const
{
	FArcItemCopyContainerHelper* Helper = static_cast<FArcItemCopyContainerHelper*>(Data);
	{
		nlohmann::json ItemSpec;
	
		FArcJsonSerializers::ToJson(FArcItemSpec::StaticStruct(), &Helper->Item, {}, ItemSpec);

		JsonObject["Item"] = ItemSpec;
	}

	if (Helper->SlotId.IsValid())
	{
		JsonObject["SlotId"] = TCHAR_TO_ANSI(*Helper->SlotId.ToString());	
	}
	else
	{
		JsonObject["SlotId"] = "";
	}

	for (FArcAttachedItemHelper& Attach : Helper->ItemAttachments)
	{
		nlohmann::json Attachment;
		nlohmann::json ItemSpecAttach;
		
		FArcJsonSerializers::ToJson(FArcItemSpec::StaticStruct(), &Attach.Item, {}, ItemSpecAttach);
		Attachment["Item"] = ItemSpecAttach;

		if (Attach.SlotId.IsValid())
		{
			Attachment["SlotId"] = TCHAR_TO_ANSI(*Attach.SlotId.ToString());	
		}
		else
		{
			Attachment["SlotId"] = "";
		}
		
		JsonObject["ItemAttachments"].push_back(Attachment);
	}
}

void FArcJsonSerializer_ItemCopyContainerHelper::FromJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	) const
{
	FArcItemCopyContainerHelper* Helper = static_cast<FArcItemCopyContainerHelper*>(Data);
	
	FArcJsonSerializers::FromJson(FArcItemSpec::StaticStruct(), &Helper->Item, {}, JsonObject["Item"]);
	
	FString SlotId = JsonObject["SlotId"].get<std::string>().c_str();
	if (SlotId.Len())
	{
		Helper->SlotId = FGameplayTag::RequestGameplayTag(*SlotId);	
	}
	 

	nlohmann::json Attachments = JsonObject["ItemAttachments"];
	for (nlohmann::json::iterator it = Attachments.begin(); it != Attachments.end(); ++it)
	{
		FArcItemSpec Spec;
		FArcJsonSerializers::FromJson(FArcItemSpec::StaticStruct(), &Spec, {}, (*it)["Item"]);
		
		FString AttachSlotId = (*it)["SlotId"].get<std::string>().c_str();
		FGameplayTag AttachSlot;
		if (AttachSlotId.Len())
		{
			AttachSlot = FGameplayTag::RequestGameplayTag(*AttachSlotId);;	
		}
		
		
		Helper->ItemAttachments.Add({Spec.ItemId, Spec, AttachSlot});
	}
}

UStruct* FArcJsonSerializer_ItemInstance_ItemStats::GetSupportedType() const
{
	return FArcItemInstance_ItemStats::StaticStruct();
}

void FArcJsonSerializer_ItemInstance_ItemStats::ToJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	
	) const
{

}

void FArcJsonSerializer_ItemInstance_ItemStats::FromJson(void* Data
	, const FString& FieldName
	, nlohmann::json& JsonObject
	
	) const
{
	
}
