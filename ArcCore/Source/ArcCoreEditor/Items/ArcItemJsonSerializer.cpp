// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemJsonSerializer.h"

#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UObjectIterator.h"

//------------------------------------------------------------------------------
// FindStructByName
//------------------------------------------------------------------------------
UScriptStruct* FArcItemJsonSerializer::FindStructByName(const FString& InName)
{
	FString LookupName = InName;
	LookupName.RemoveFromStart(TEXT("F"));

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == LookupName)
		{
			return *It;
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// StructPropertiesToJson - export all editable properties
//------------------------------------------------------------------------------
TSharedPtr<FJsonObject> FArcItemJsonSerializer::StructPropertiesToJson(const UScriptStruct* Struct, const void* StructMemory)
{
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	if (!Struct || !StructMemory)
	{
		return Props;
	}

	// Get default instance for comparison
	const int32 StructSize = Struct->GetStructureSize();
	uint8* DefaultMemory = static_cast<uint8*>(FMemory::Malloc(StructSize, Struct->GetMinAlignment()));
	Struct->InitializeStruct(DefaultMemory);

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;

		// Skip properties that aren't editable
		if (!Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
		{
			continue;
		}

		// Skip transient properties
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructMemory);
		const void* DefaultPtr = Property->ContainerPtrToValuePtr<void>(DefaultMemory);

		// Skip properties that match defaults (keep output compact)
		if (Property->Identical(ValuePtr, DefaultPtr))
		{
			continue;
		}

		// Export property value to text
		FString ValueStr;
		Property->ExportTextItem_Direct(ValueStr, ValuePtr, DefaultPtr, nullptr, PPF_None);

		if (!ValueStr.IsEmpty())
		{
			Props->SetStringField(Property->GetName(), ValueStr);
		}
	}

	Struct->DestroyStruct(DefaultMemory);
	FMemory::Free(DefaultMemory);

	return Props;
}

//------------------------------------------------------------------------------
// SetPropertiesFromJson - import properties from JSON
//------------------------------------------------------------------------------
bool FArcItemJsonSerializer::SetPropertiesFromJson(void* StructMemory, const UScriptStruct* Struct, const TSharedPtr<FJsonObject>& Props, FString& OutError)
{
	if (!StructMemory || !Struct || !Props.IsValid())
	{
		return true;
	}

	for (const auto& Pair : Props->Values)
	{
		const FString& PropName = *Pair.Key;
		const TSharedPtr<FJsonValue>& Value = Pair.Value;

		FProperty* Property = Struct->FindPropertyByName(FName(*PropName));
		if (!Property)
		{
			OutError += FString::Printf(TEXT("Unknown property '%s' on %s. "), *PropName, *Struct->GetName());
			continue;
		}

		void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(StructMemory);

		FString ValueString;
		if (Value->TryGetString(ValueString))
		{
			Property->ImportText_Direct(*ValueString, PropertyValuePtr, nullptr, PPF_None);
		}
		else if (Value->Type == EJson::Number)
		{
			const double NumValue = Value->AsNumber();
			if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
			{
				FloatProp->SetPropertyValue(PropertyValuePtr, static_cast<float>(NumValue));
			}
			else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
			{
				DoubleProp->SetPropertyValue(PropertyValuePtr, NumValue);
			}
			else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
			{
				IntProp->SetPropertyValue(PropertyValuePtr, static_cast<int32>(NumValue));
			}
			else
			{
				const FString NumStr = FString::SanitizeFloat(NumValue);
				Property->ImportText_Direct(*NumStr, PropertyValuePtr, nullptr, PPF_None);
			}
		}
		else if (Value->Type == EJson::Boolean)
		{
			if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
			{
				BoolProp->SetPropertyValue(PropertyValuePtr, Value->AsBool());
			}
		}
		else if (Value->Type == EJson::Object)
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				FJsonObjectConverter::JsonObjectToUStruct(Value->AsObject().ToSharedRef(), StructProp->Struct, PropertyValuePtr);
			}
		}
		else if (Value->Type == EJson::Array)
		{
			// For arrays, use ImportText with the text representation
			FString ArrayStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayStr);
			FJsonSerializer::Serialize(Value->AsArray(), Writer);
			Writer->Close();
			Property->ImportText_Direct(*ArrayStr, PropertyValuePtr, nullptr, PPF_None);
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// FragmentToJson
//------------------------------------------------------------------------------
TSharedPtr<FJsonObject> FArcItemJsonSerializer::FragmentToJson(const FArcInstancedStruct& Fragment)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Fragment.IsValid())
	{
		return Obj;
	}

	const UScriptStruct* Struct = Fragment.GetScriptStruct();
	Obj->SetStringField(TEXT("type"), Struct->GetName());

	TSharedPtr<FJsonObject> Props = StructPropertiesToJson(Struct, Fragment.GetMemory());
	if (Props->Values.Num() > 0)
	{
		Obj->SetObjectField(TEXT("properties"), Props);
	}

	return Obj;
}

//------------------------------------------------------------------------------
// InstancedStructToJson
//------------------------------------------------------------------------------
TSharedPtr<FJsonObject> FArcItemJsonSerializer::InstancedStructToJson(const FInstancedStruct& Instance)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Instance.IsValid())
	{
		return Obj;
	}

	const UScriptStruct* Struct = Instance.GetScriptStruct();
	Obj->SetStringField(TEXT("type"), Struct->GetName());

	TSharedPtr<FJsonObject> Props = StructPropertiesToJson(Struct, Instance.GetMemory());
	if (Props->Values.Num() > 0)
	{
		Obj->SetObjectField(TEXT("properties"), Props);
	}

	return Obj;
}

//------------------------------------------------------------------------------
// ExportToJson
//------------------------------------------------------------------------------
TSharedPtr<FJsonObject> FArcItemJsonSerializer::ExportToJson(const UArcItemDefinition* ItemDef)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	if (!ItemDef)
	{
		return Root;
	}

	// Item type
	if (ItemDef->GetItemType().IsValid())
	{
		Root->SetStringField(TEXT("item_type"), ItemDef->GetItemType().ToString());
	}

	// Stack method
	// Access via reflection since StackMethod is protected
	if (const FProperty* StackProp = ItemDef->GetClass()->FindPropertyByName(TEXT("StackMethod")))
	{
		const FInstancedStruct* StackPtr = StackProp->ContainerPtrToValuePtr<FInstancedStruct>(ItemDef);
		if (StackPtr && StackPtr->IsValid())
		{
			Root->SetObjectField(TEXT("stack_method"), InstancedStructToJson(*StackPtr));
		}
	}

	// Fragments
	TArray<TSharedPtr<FJsonValue>> FragmentsArray;
	for (const FArcInstancedStruct& Fragment : ItemDef->GetFragmentSet())
	{
		FragmentsArray.Add(MakeShared<FJsonValueObject>(FragmentToJson(Fragment)));
	}
	if (FragmentsArray.Num() > 0)
	{
		Root->SetArrayField(TEXT("fragments"), FragmentsArray);
	}

	// Scalable float fragments
	TArray<TSharedPtr<FJsonValue>> ScalableArray;
	for (const FArcInstancedStruct& Fragment : ItemDef->GetScalableFloatFragments())
	{
		ScalableArray.Add(MakeShared<FJsonValueObject>(FragmentToJson(Fragment)));
	}
	if (ScalableArray.Num() > 0)
	{
		Root->SetArrayField(TEXT("scalable_float_fragments"), ScalableArray);
	}

	return Root;
}

//------------------------------------------------------------------------------
// ImportFromJson
//------------------------------------------------------------------------------
bool FArcItemJsonSerializer::ImportFromJson(UArcItemDefinition* ItemDef, const TSharedPtr<FJsonObject>& JsonObj, FString& OutError)
{
	if (!ItemDef || !JsonObj.IsValid())
	{
		OutError = TEXT("Invalid item definition or JSON object");
		return false;
	}

	// Item type
	FString ItemTypeStr;
	if (JsonObj->TryGetStringField(TEXT("item_type"), ItemTypeStr))
	{
		if (FProperty* TypeProp = ItemDef->GetClass()->FindPropertyByName(TEXT("ItemType")))
		{
			void* TypePtr = TypeProp->ContainerPtrToValuePtr<void>(ItemDef);
			TypeProp->ImportText_Direct(*ItemTypeStr, TypePtr, nullptr, PPF_None);
		}
	}

	// Stack method
	const TSharedPtr<FJsonObject>* StackObj;
	if (JsonObj->TryGetObjectField(TEXT("stack_method"), StackObj))
	{
		FString StackType;
		if ((*StackObj)->TryGetStringField(TEXT("type"), StackType))
		{
			UScriptStruct* StackStruct = FindStructByName(StackType);
			if (StackStruct)
			{
				if (FProperty* StackProp = ItemDef->GetClass()->FindPropertyByName(TEXT("StackMethod")))
				{
					FInstancedStruct* StackPtr = StackProp->ContainerPtrToValuePtr<FInstancedStruct>(ItemDef);
					StackPtr->InitializeAs(StackStruct);

					const TSharedPtr<FJsonObject>* StackProps;
					if ((*StackObj)->TryGetObjectField(TEXT("properties"), StackProps))
					{
						SetPropertiesFromJson(StackPtr->GetMutableMemory(), StackStruct, *StackProps, OutError);
					}
				}
			}
			else
			{
				OutError += FString::Printf(TEXT("Stack method type not found: %s. "), *StackType);
			}
		}
	}

	// Clear existing fragments — we rebuild from JSON
	// Access FragmentSet and ScalableFloatFragmentSet via reflection since they're protected
	auto ClearAndImportFragments = [&](const FString& JsonKey, const FString& PropertyName) -> bool
	{
		const TArray<TSharedPtr<FJsonValue>>* FragArray;
		if (!JsonObj->TryGetArrayField(JsonKey, FragArray))
		{
			return true; // No fragments of this type in JSON — leave as is
		}

		// Clear existing fragments of this type
		if (FProperty* SetProp = ItemDef->GetClass()->FindPropertyByName(*PropertyName))
		{
			FSetProperty* SetProperty = CastField<FSetProperty>(SetProp);
			if (SetProperty)
			{
				void* SetPtr = SetProperty->ContainerPtrToValuePtr<void>(ItemDef);
				FScriptSetHelper SetHelper(SetProperty, SetPtr);
				SetHelper.EmptyElements();
			}
		}

		// Import each fragment
		for (const TSharedPtr<FJsonValue>& FragValue : *FragArray)
		{
			const TSharedPtr<FJsonObject>& FragObj = FragValue->AsObject();
			if (!FragObj.IsValid())
			{
				continue;
			}

			FString FragType;
			if (!FragObj->TryGetStringField(TEXT("type"), FragType))
			{
				OutError += TEXT("Fragment missing 'type' field. ");
				continue;
			}

			UScriptStruct* FragStruct = FindStructByName(FragType);
			if (!FragStruct)
			{
				OutError += FString::Printf(TEXT("Fragment type not found: %s. "), *FragType);
				continue;
			}

			// Create the fragment via AddFragmentByType
			ItemDef->AddFragmentByType(FragStruct);

			// Now set properties on it
			const TSharedPtr<FJsonObject>* FragProps;
			if (FragObj->TryGetObjectField(TEXT("properties"), FragProps))
			{
				// Find the fragment we just added in the set
				if (FProperty* SetProp = ItemDef->GetClass()->FindPropertyByName(*PropertyName))
				{
					FSetProperty* SetProperty = CastField<FSetProperty>(SetProp);
					if (SetProperty)
					{
						void* SetPtr = SetProperty->ContainerPtrToValuePtr<void>(ItemDef);
						FScriptSetHelper SetHelper(SetProperty, SetPtr);

						// Find the element we just added by struct name
						for (int32 i = 0; i < SetHelper.Num(); ++i)
						{
							if (SetHelper.IsValidIndex(i))
							{
								FArcInstancedStruct* ArcInstance = reinterpret_cast<FArcInstancedStruct*>(SetHelper.GetElementPtr(i));
								if (ArcInstance && ArcInstance->GetScriptStruct() == FragStruct)
								{
									SetPropertiesFromJson(
										ArcInstance->InstancedStruct.GetMutableMemory(),
										FragStruct,
										*FragProps,
										OutError);
									break;
								}
							}
						}
					}
				}
			}
		}

		return true;
	};

	ClearAndImportFragments(TEXT("fragments"), TEXT("FragmentSet"));
	ClearAndImportFragments(TEXT("scalable_float_fragments"), TEXT("ScalableFloatFragmentSet"));

	// Mark dirty
	ItemDef->MarkPackageDirty();

	return OutError.IsEmpty();
}

//------------------------------------------------------------------------------
// String convenience wrappers
//------------------------------------------------------------------------------
FString FArcItemJsonSerializer::ExportToJsonString(const UArcItemDefinition* ItemDef)
{
	TSharedPtr<FJsonObject> JsonObj = ExportToJson(ItemDef);
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return OutputString;
}

bool FArcItemJsonSerializer::ImportFromJsonString(UArcItemDefinition* ItemDef, const FString& JsonString, FString& OutError)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		OutError = TEXT("Failed to parse JSON string");
		return false;
	}
	return ImportFromJson(ItemDef, JsonObj, OutError);
}
