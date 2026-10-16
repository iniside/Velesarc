// Copyright Lukasz Baran. All Rights Reserved.

#include "Json/ArcPropertyJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Json/ArcStructTypeNames.h"
#include "JsonObjectConverter.h"
#include "Misc/PackageName.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/UnrealType.h"

namespace ArcPropertyJsonPrivate
{
	TSharedPtr<FJsonValue> CloneValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return Value;
		}
		if (Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> Items;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				Items.Add(CloneValue(Item));
			}
			return MakeShared<FJsonValueArray>(Items);
		}
		if (Value->Type == EJson::Object)
		{
			return MakeShared<FJsonValueObject>(ArcPropertyJson::CloneObject(*Value->AsObject()));
		}
		return Value;
	}

	void ShortenStructTypes(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return;
		}
		if (Value->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				ShortenStructTypes(Item);
			}
			return;
		}
		if (Value->Type != EJson::Object)
		{
			return;
		}
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		FString TypePath;
		if (Object->TryGetStringField(ArcStructTypeNames::StructTypeKey, TypePath))
		{
			if (const UScriptStruct* Type = FindObject<UScriptStruct>(nullptr, *TypePath))
			{
				Object->SetStringField(ArcStructTypeNames::StructTypeKey, ArcStructTypeNames::GetTypeName<UScriptStruct>(Type));
			}
		}
		for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			ShortenStructTypes(Pair.Value);
		}
	}

	void AddImportDiagnostics(
		const TArray<FJsonImportDiagnostic>& Diagnostics,
		const FString& Location,
		FArcPropertyJsonReport& Report)
	{
		for (const FJsonImportDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Kind == EJsonImportDiagnosticKind::UnmatchedKey)
			{
				Report.Error(FString::Printf(
					TEXT("%s%s: unknown or non-editable field '%s' on %s."),
					*Location,
					*Diagnostic.Path,
					*Diagnostic.JsonKey,
					Diagnostic.Struct ? *Diagnostic.Struct->GetName() : TEXT("<unknown>")));
			}
			else
			{
				Report.Error(FString::Printf(
					TEXT("%s%s: value type does not match property %s (%s)."),
					*Location,
					*Diagnostic.Path,
					Diagnostic.Property ? *Diagnostic.Property->GetName() : TEXT("<unknown>"),
					Diagnostic.Property ? *Diagnostic.Property->GetCPPType() : TEXT("")));
			}
		}
	}

	void PrepassStruct(
		const UStruct& Struct,
		FJsonObject& Object,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks);

#if WITH_METADATA
	const UScriptStruct* ResolveBaseStruct(const FProperty& Property)
	{
		const FString& BasePath = Property.GetMetaData(TEXT("BaseStruct"));
		return BasePath.IsEmpty() ? nullptr : ArcStructTypeNames::FindType<UScriptStruct>(BasePath);
	}
#endif

	void ReplaceField(FJsonObject& Object, const FString& Key, const TSharedPtr<FJsonValue>& Value)
	{
		for (TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object.Values)
		{
			if (FString(Pair.Key).Equals(Key, ESearchCase::IgnoreCase))
			{
				Pair.Value = Value;
				return;
			}
		}
	}

	TSharedPtr<FJsonValue> PrepassValue(
		const FProperty& Property,
		const FProperty& MetaProperty,
		const TSharedPtr<FJsonValue>& Value,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks)
	{
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			return TSharedPtr<FJsonValue>();
		}
		const FProperty* ElementProperty = nullptr;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			ElementProperty = ArrayProperty->Inner;
		}
		else if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			ElementProperty = SetProperty->ElementProp;
		}
		if (ElementProperty)
		{
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!Value->TryGetArray(Items))
			{
				return TSharedPtr<FJsonValue>();
			}
			TArray<TSharedPtr<FJsonValue>> RewrittenItems;
			for (int32 Index = 0; Index < Items->Num(); ++Index)
			{
				const TSharedPtr<FJsonValue> Rewritten = PrepassValue(
					*ElementProperty,
					MetaProperty,
					(*Items)[Index],
					FString::Printf(TEXT("%s[%d]"), *Location, Index),
					Report,
					Hooks);
				if (Rewritten.IsValid())
				{
					if (RewrittenItems.Num() == 0)
					{
						RewrittenItems = *Items;
					}
					RewrittenItems[Index] = Rewritten;
				}
			}
			if (RewrittenItems.Num() == 0)
			{
				return TSharedPtr<FJsonValue>();
			}
			return MakeShared<FJsonValueArray>(RewrittenItems);
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			const TSharedPtr<FJsonObject>* MapObject = nullptr;
			if (Value->TryGetObject(MapObject) && MapObject->IsValid())
			{
				for (TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : (*MapObject)->Values)
				{
					const TSharedPtr<FJsonValue> Rewritten = PrepassValue(
						*MapProperty->ValueProp,
						MetaProperty,
						Pair.Value,
						FString::Printf(TEXT("%s[%s]"), *Location, *FString(Pair.Key)),
						Report,
						Hooks);
					if (Rewritten.IsValid())
					{
						Pair.Value = Rewritten;
					}
				}
			}
			return TSharedPtr<FJsonValue>();
		}
		const FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!StructProperty || !Value->TryGetObject(ObjectPtr) || !ObjectPtr->IsValid())
		{
			return TSharedPtr<FJsonValue>();
		}
		FJsonObject& Object = **ObjectPtr;
		if (StructProperty->Struct == FInstancedPropertyBag::StaticStruct()
			|| (Hooks.SkipPrepass && Hooks.SkipPrepass(*StructProperty->Struct)))
		{
			return TSharedPtr<FJsonValue>();
		}
		if (StructProperty->Struct != FInstancedStruct::StaticStruct())
		{
			PrepassStruct(*StructProperty->Struct, Object, Location, Report, Hooks);
			return TSharedPtr<FJsonValue>();
		}
		if (Object.Values.Num() == 0)
		{
			return TSharedPtr<FJsonValue>();
		}
		FString TypeKey;
		FString TypeName;
		for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object.Values)
		{
			if (FString(Pair.Key).Equals(ArcStructTypeNames::StructTypeKey, ESearchCase::IgnoreCase))
			{
				TypeKey = FString(Pair.Key);
				if (Pair.Value.IsValid())
				{
					Pair.Value->TryGetString(TypeName);
				}
				break;
			}
		}
		if (TypeName.IsEmpty())
		{
			Report.Error(FString::Printf(
				TEXT("%s: an instanced struct value needs a '_structType' naming its struct type."),
				*Location));
			return TSharedPtr<FJsonValue>();
		}
		UScriptStruct* Type = ArcStructTypeNames::FindType<UScriptStruct>(TypeName);
		if (!Type)
		{
			Report.Error(FString::Printf(TEXT("%s: unknown struct type '%s'."), *Location, *TypeName));
			return TSharedPtr<FJsonValue>();
		}
		// Cooked builds have no metadata; struct-type restrictions are validated in the editor.
#if WITH_METADATA
		const UScriptStruct* BaseStruct = ResolveBaseStruct(MetaProperty);
		const bool bExcludeBase = MetaProperty.HasMetaData(TEXT("ExcludeBaseStruct"))
			|| Property.HasMetaData(TEXT("ExcludeBaseStruct"));
		if (!BaseStruct && &MetaProperty != &Property)
		{
			BaseStruct = ResolveBaseStruct(Property);
		}
		if (BaseStruct && !Type->IsChildOf(BaseStruct))
		{
			Report.Error(FString::Printf(
				TEXT("%s: struct type '%s' is not a %s."),
				*Location,
				*Type->GetName(),
				*BaseStruct->GetName()));
			return TSharedPtr<FJsonValue>();
		}
		if (BaseStruct && bExcludeBase && Type == BaseStruct)
		{
			Report.Error(FString::Printf(
				TEXT("%s: the base struct '%s' itself is not allowed here."),
				*Location,
				*BaseStruct->GetName()));
			return TSharedPtr<FJsonValue>();
		}
#endif
		Object.RemoveField(TypeKey);
		Object.SetStringField(ArcStructTypeNames::StructTypeKey, Type->GetPathName());
		PrepassStruct(*Type, Object, Location, Report, Hooks);
		return TSharedPtr<FJsonValue>();
	}

	void PrepassStruct(
		const UStruct& Struct,
		FJsonObject& Object,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks)
	{
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const FString Key = Struct.GetAuthoredNameForField(Property);
			const TSharedPtr<FJsonValue> Value = ArcPropertyJson::FindField(Object, Key);
			if (!Value.IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonValue> Rewritten = PrepassValue(
				*Property, *Property, Value, Location + TEXT(".") + Property->GetName(), Report, Hooks);
			if (Rewritten.IsValid())
			{
				ReplaceField(Object, Key, Rewritten);
			}
		}
	}

	bool ImportInstancedStruct(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* Value)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty || StructProperty->Struct != FInstancedStruct::StaticStruct() || !JsonValue.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!JsonValue->TryGetObject(Object) || !Object->IsValid())
		{
			return false;
		}
		FInstancedStruct& Instanced = *static_cast<FInstancedStruct*>(Value);
		if ((*Object)->Values.Num() == 0)
		{
			Instanced.Reset();
			return true;
		}
		FString TypePath;
		if ((*Object)->TryGetStringField(ArcStructTypeNames::StructTypeKey, TypePath))
		{
			const UScriptStruct* Type = FindObject<UScriptStruct>(nullptr, *TypePath);
			if (Type && Instanced.GetScriptStruct() != Type)
			{
				Instanced.InitializeAs(Type);
			}
		}
		return false;
	}

	FJsonObjectConverter::CustomImportCallback MakeImportCallback(
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks)
	{
		return FJsonObjectConverter::CustomImportCallback::CreateLambda(
			[Location, &Report, &Hooks](const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* Value) -> bool
		{
			if (Hooks.ImportStruct && Hooks.ImportStruct(JsonValue, Property, Value, Location, Report))
			{
				return true;
			}
			return ImportInstancedStruct(JsonValue, Property, Value);
		});
	}

#if WITH_EDITOR
	void SetNumericMeta(FJsonObject& Field, const FProperty& Property, const TCHAR* MetaKey, const TCHAR* JsonKey)
	{
		const FString& Meta = Property.GetMetaData(MetaKey);
		if (Meta.IsNumeric())
		{
			Field.SetNumberField(JsonKey, FCString::Atod(*Meta));
		}
	}
#endif

	bool IsSameDescSchema(const FPropertyBagPropertyDesc& A, const FPropertyBagPropertyDesc& B)
	{
		return A.Name == B.Name
			&& A.ValueType == B.ValueType
			&& A.ContainerTypes == B.ContainerTypes
			&& A.ValueTypeObject == B.ValueTypeObject;
	}
}

FString ArcPropertyJson::GetObjectTypeName(const UObject* Type)
{
	if (const UScriptStruct* Struct = Cast<UScriptStruct>(Type))
	{
		return ArcStructTypeNames::GetTypeName<UScriptStruct>(Struct);
	}
	if (const UEnum* Enum = Cast<UEnum>(Type))
	{
		return ArcStructTypeNames::GetTypeName<UEnum>(Enum);
	}
	if (const UClass* Class = Cast<UClass>(Type))
	{
		return ArcStructTypeNames::GetTypeName<UClass>(Class);
	}
	return Type ? Type->GetPathName() : FString();
}

TSharedPtr<FJsonValue> ArcPropertyJson::FindField(const FJsonObject& Object, const FString& Key)
{
	for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object.Values)
	{
		if (FString(Pair.Key).Equals(Key, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}
	return TSharedPtr<FJsonValue>();
}

TSharedRef<FJsonObject> ArcPropertyJson::CloneObject(const FJsonObject& Object)
{
	TSharedRef<FJsonObject> Copy = MakeShared<FJsonObject>();
	for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object.Values)
	{
		Copy->SetField(FString(Pair.Key), ArcPropertyJsonPrivate::CloneValue(Pair.Value));
	}
	return Copy;
}

TArray<TSharedPtr<FJsonValue>> ArcPropertyJson::ToJsonStrings(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FString& Value : Values)
	{
		Items.Add(MakeShared<FJsonValueString>(Value));
	}
	return Items;
}

void ArcPropertyJson::CheckKeys(
	const FJsonObject& Object,
	const TConstArrayView<const TCHAR*> Allowed,
	const FString& Location,
	FArcPropertyJsonReport& Report)
{
	for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object.Values)
	{
		const FString Key(Pair.Key);
		const bool bKnown = Allowed.ContainsByPredicate([&Key](const TCHAR* Candidate) -> bool
		{
			return Key.Equals(Candidate, ESearchCase::IgnoreCase);
		});
		if (!bKnown)
		{
			Report.Error(FString::Printf(
				TEXT("%s: unknown key '%s' (allowed: %s)."),
				*Location,
				*Key,
				*FString::Join(Allowed, TEXT(", "))));
		}
	}
}

TSharedPtr<FJsonObject> ArcPropertyJson::AsObject(
	const TSharedPtr<FJsonValue>& Value,
	const FString& Location,
	FArcPropertyJsonReport& Report)
{
	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid())
	{
		Report.Error(FString::Printf(TEXT("%s: expected a JSON object."), *Location));
		return TSharedPtr<FJsonObject>();
	}
	return *Object;
}

TSharedPtr<FJsonObject> ArcPropertyJson::GetObjectValue(
	const FJsonObject& Owner,
	const TCHAR* Key,
	const FString& Location,
	FArcPropertyJsonReport& Report)
{
	const TSharedPtr<FJsonValue> Value = FindField(Owner, Key);
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return TSharedPtr<FJsonObject>();
	}
	return AsObject(Value, FString::Printf(TEXT("%s.%s"), *Location, Key), Report);
}

TArray<TSharedPtr<FJsonValue>> ArcPropertyJson::GetArrayValue(
	const FJsonObject& Owner,
	const TCHAR* Key,
	const FString& Location,
	FArcPropertyJsonReport& Report)
{
	const TSharedPtr<FJsonValue> Value = FindField(Owner, Key);
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return TArray<TSharedPtr<FJsonValue>>();
	}
	if (!Value->TryGetArray(Items))
	{
		Report.Error(FString::Printf(TEXT("%s.%s: expected a JSON array."), *Location, Key));
		return TArray<TSharedPtr<FJsonValue>>();
	}
	return *Items;
}

bool ArcPropertyJson::GetBool(
	const FJsonObject& Owner,
	const TCHAR* Key,
	const FString& Location,
	FArcPropertyJsonReport& Report)
{
	const TSharedPtr<FJsonValue> Value = FindField(Owner, Key);
	bool bResult = false;
	if (Value.IsValid() && Value->Type != EJson::Null
		&& (Value->Type != EJson::Boolean || !Value->TryGetBool(bResult)))
	{
		Report.Error(FString::Printf(TEXT("%s.%s: expected a boolean."), *Location, Key));
	}
	return bResult;
}

bool ArcPropertyJson::GetString(
	const FJsonObject& Owner,
	const TCHAR* Key,
	const FString& Location,
	FString& OutValue,
	FArcPropertyJsonReport& Report)
{
	const TSharedPtr<FJsonValue> Value = FindField(Owner, Key);
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return false;
	}
	if (Value->Type != EJson::String || !Value->TryGetString(OutValue))
	{
		Report.Error(FString::Printf(TEXT("%s.%s: expected a string."), *Location, Key));
		return false;
	}
	return true;
}

TSharedPtr<FJsonValue> ArcPropertyJson::ExportPropertyValue(
	const FProperty& Property,
	const void* ValuePtr,
	const FArcPropertyJsonHooks& Hooks)
{
	const FJsonObjectConverter::CustomExportCallback Callback = FJsonObjectConverter::CustomExportCallback::CreateLambda(
		[&Hooks](FProperty* InProperty, const void* InValue) -> TSharedPtr<FJsonValue>
	{
		return Hooks.ExportStruct ? Hooks.ExportStruct(InProperty, InValue) : TSharedPtr<FJsonValue>();
	});
	const TSharedPtr<FJsonValue> Value = FJsonObjectConverter::UPropertyToJsonValue(
		const_cast<FProperty*>(&Property), ValuePtr, CPF_Edit, 0, &Callback);
	ArcPropertyJsonPrivate::ShortenStructTypes(Value);
	return Value;
}

bool ArcPropertyJson::IsIdenticalProperty(const FProperty& Property, const void* Container, const void* DefaultContainer)
{
	for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
	{
		if (!Property.Identical_InContainer(Container, DefaultContainer, Index, PPF_None))
		{
			return false;
		}
	}
	return true;
}

TSharedRef<FJsonObject> ArcPropertyJson::ExportStructDelta(
	const UScriptStruct& Struct,
	const void* Memory,
	const FArcPropertyJsonHooks& Hooks,
	const TFunctionRef<bool(const FProperty& Property)> SkipProperty)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	FInstancedStruct Defaults;
	Defaults.InitializeAs(&Struct);
	for (TFieldIterator<FProperty> It(&Struct); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit)
			|| Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
		{
			continue;
		}
		if (SkipProperty(*Property))
		{
			continue;
		}
		if (IsIdenticalProperty(*Property, Memory, Defaults.GetMemory()))
		{
			continue;
		}
		const TSharedPtr<FJsonValue> Value = ExportPropertyValue(
			*Property, Property->ContainerPtrToValuePtr<void>(Memory), Hooks);
		if (Value.IsValid())
		{
			Out->SetField(FJsonObjectConverter::StandardizeCase(Property->GetAuthoredName()), Value);
		}
	}
	return Out;
}

TArray<TSharedPtr<FJsonValue>> ArcPropertyJson::ExportBag(
	const FInstancedPropertyBag& Bag,
	const FArcPropertyJsonHooks& Hooks)
{
	TArray<TSharedPtr<FJsonValue>> Entries;
	const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
	if (!BagStruct)
	{
		return Entries;
	}
	FInstancedStruct Defaults;
	Defaults.InitializeAs(BagStruct);
	const void* Memory = Bag.GetValue().GetMemory();
	for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Desc.Name.ToString());
		Entry->SetStringField(
			TEXT("type"),
			StaticEnum<EPropertyBagPropertyType>()->GetNameStringByValue(static_cast<int64>(Desc.ValueType)));
		if (Desc.ValueTypeObject)
		{
			Entry->SetStringField(TEXT("valueType"), GetObjectTypeName(Desc.ValueTypeObject));
		}
		if (Desc.ContainerTypes.Num() == 1)
		{
			Entry->SetStringField(
				TEXT("container"),
				StaticEnum<EPropertyBagContainerType>()->GetNameStringByValue(
					static_cast<int64>(Desc.ContainerTypes.GetFirstContainerType())));
		}
		else if (Desc.ContainerTypes.Num() > 1)
		{
			Entry->SetStringField(TEXT("container"), TEXT("Nested"));
		}
		const FProperty* Property = BagStruct->FindPropertyByName(Desc.Name);
		if (Property && Memory && !IsIdenticalProperty(*Property, Memory, Defaults.GetMemory()))
		{
			const TSharedPtr<FJsonValue> Value = ExportPropertyValue(
				*Property, Property->ContainerPtrToValuePtr<void>(Memory), Hooks);
			if (Value.IsValid())
			{
				Entry->SetField(TEXT("value"), Value);
			}
		}
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Entries;
}

bool ArcPropertyJson::ImportStruct(
	const UStruct& Struct,
	void* Memory,
	const TSharedPtr<FJsonObject>& Json,
	const FString& Location,
	FArcPropertyJsonReport& Report,
	const FArcPropertyJsonHooks& Hooks)
{
	if (!Json.IsValid())
	{
		return true;
	}
	const int32 ErrorCount = Report.Errors.Num();
	ArcPropertyJsonPrivate::PrepassStruct(Struct, *Json, Location, Report, Hooks);
	if (Report.Errors.Num() != ErrorCount)
	{
		return false;
	}
	const FJsonObjectConverter::CustomImportCallback Callback =
		ArcPropertyJsonPrivate::MakeImportCallback(Location, Report, Hooks);
	TArray<FJsonImportDiagnostic> Diagnostics;
	FText FailReason;
	if (!FJsonObjectConverter::JsonObjectToUStruct(
		Json.ToSharedRef(), &Struct, Memory, CPF_Edit, 0, false, &FailReason, &Callback, &Diagnostics))
	{
		Report.Error(FString::Printf(TEXT("%s: %s"), *Location, *FailReason.ToString()));
	}
	ArcPropertyJsonPrivate::AddImportDiagnostics(Diagnostics, Location, Report);
	return Report.Errors.Num() == ErrorCount;
}

bool ArcPropertyJson::ImportPropertyValue(
	FProperty& Property,
	void* ValuePtr,
	const TSharedPtr<FJsonValue>& Value,
	const FString& Location,
	FArcPropertyJsonReport& Report,
	const FArcPropertyJsonHooks& Hooks)
{
	const int32 ErrorCount = Report.Errors.Num();
	const TSharedPtr<FJsonValue> Rewritten =
		ArcPropertyJsonPrivate::PrepassValue(Property, Property, Value, Location, Report, Hooks);
	if (Report.Errors.Num() != ErrorCount)
	{
		return false;
	}
	const FJsonObjectConverter::CustomImportCallback Callback =
		ArcPropertyJsonPrivate::MakeImportCallback(Location, Report, Hooks);
	TArray<FJsonImportDiagnostic> Diagnostics;
	FText FailReason;
	if (!FJsonObjectConverter::JsonValueToUProperty(
		Rewritten.IsValid() ? Rewritten : Value, &Property, ValuePtr, CPF_Edit, 0, false, &FailReason, &Callback, &Diagnostics))
	{
		Report.Error(FString::Printf(TEXT("%s: %s"), *Location, *FailReason.ToString()));
	}
	ArcPropertyJsonPrivate::AddImportDiagnostics(Diagnostics, Location, Report);
	return Report.Errors.Num() == ErrorCount;
}

void ArcPropertyJson::ImportBag(
	const TArray<TSharedPtr<FJsonValue>>& Entries,
	FInstancedPropertyBag& OutBag,
	const FString& Location,
	FArcPropertyJsonReport& Report,
	const FArcPropertyJsonHooks& Hooks,
	const UPropertyBag* ReuseDescsFrom)
{
	if (Entries.Num() == 0)
	{
		return;
	}
	const int32 ErrorCount = Report.Errors.Num();
	TArray<FPropertyBagPropertyDesc> Descs;
	TArray<TSharedPtr<FJsonValue>> Values;
	TSet<FString> Names;
	UEnum* TypeEnum = StaticEnum<EPropertyBagPropertyType>();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FString EntryLocation = FString::Printf(TEXT("%s[%d]"), *Location, Index);
		const TSharedPtr<FJsonObject> Entry = AsObject(Entries[Index], EntryLocation, Report);
		if (!Entry.IsValid())
		{
			continue;
		}
		CheckKeys(
			*Entry,
			{TEXT("name"), TEXT("type"), TEXT("valueType"), TEXT("container"), TEXT("value")},
			EntryLocation,
			Report);

		FString Name;
		if (!GetString(*Entry, TEXT("name"), EntryLocation, Name, Report) || Name.IsEmpty())
		{
			Report.Error(FString::Printf(TEXT("%s: 'name' is required."), *EntryLocation));
			continue;
		}
		if (Names.Contains(Name))
		{
			Report.Error(FString::Printf(TEXT("%s: duplicate bag property '%s'."), *EntryLocation, *Name));
			continue;
		}
		Names.Add(Name);

		FString TypeText;
		GetString(*Entry, TEXT("type"), EntryLocation, TypeText, Report);
		const int64 TypeValue = TypeText.IsEmpty() ? INDEX_NONE : TypeEnum->GetValueByNameString(TypeText);
		if (TypeValue == INDEX_NONE
			|| TypeValue == static_cast<int64>(EPropertyBagPropertyType::None)
			|| TypeValue >= static_cast<int64>(EPropertyBagPropertyType::Count))
		{
			Report.Error(FString::Printf(
				TEXT("%s: 'type' must be an EPropertyBagPropertyType name (Bool, Byte, Int32, Int64, Float, Double, Name, String, Text, Enum, Struct, Object, SoftObject, Class, SoftClass, ...)."),
				*EntryLocation));
			continue;
		}
		const EPropertyBagPropertyType ValueType = static_cast<EPropertyBagPropertyType>(TypeValue);

		FString ValueTypeName;
		GetString(*Entry, TEXT("valueType"), EntryLocation, ValueTypeName, Report);
		const UObject* ValueTypeObject = nullptr;
		const bool bNeedsValueType = ValueType == EPropertyBagPropertyType::Enum
			|| ValueType == EPropertyBagPropertyType::Struct
			|| ValueType == EPropertyBagPropertyType::Object
			|| ValueType == EPropertyBagPropertyType::SoftObject
			|| ValueType == EPropertyBagPropertyType::Class
			|| ValueType == EPropertyBagPropertyType::SoftClass;
		if (bNeedsValueType)
		{
			if (ValueType == EPropertyBagPropertyType::Enum)
			{
				ValueTypeObject = ArcStructTypeNames::FindType<UEnum>(ValueTypeName);
			}
			else if (ValueType == EPropertyBagPropertyType::Struct)
			{
				ValueTypeObject = ArcStructTypeNames::FindType<UScriptStruct>(ValueTypeName);
			}
			else
			{
				ValueTypeObject = ArcStructTypeNames::FindType<UClass>(ValueTypeName);
			}
			if (!ValueTypeObject)
			{
				Report.Error(FString::Printf(
					TEXT("%s: type %s needs a resolvable 'valueType' (got '%s')."),
					*EntryLocation,
					*TypeText,
					*ValueTypeName));
				continue;
			}
		}
		else if (!ValueTypeName.IsEmpty())
		{
			Report.Error(FString::Printf(
				TEXT("%s: 'valueType' is only used by Enum, Struct, Object, SoftObject, Class and SoftClass."),
				*EntryLocation));
			continue;
		}

		FString ContainerText;
		GetString(*Entry, TEXT("container"), EntryLocation, ContainerText, Report);
		EPropertyBagContainerType Container = EPropertyBagContainerType::None;
		if (ContainerText.Equals(TEXT("Array"), ESearchCase::IgnoreCase))
		{
			Container = EPropertyBagContainerType::Array;
		}
		else if (ContainerText.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
		{
			Container = EPropertyBagContainerType::Set;
		}
		else if (!ContainerText.IsEmpty() && !ContainerText.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			Report.Error(FString::Printf(
				TEXT("%s: 'container' must be Array or Set (got '%s')."),
				*EntryLocation,
				*ContainerText));
			continue;
		}

		const FPropertyBagPropertyDesc NewDesc(FName(*Name), Container, ValueType, ValueTypeObject);
		const FPropertyBagPropertyDesc* Existing = ReuseDescsFrom ? ReuseDescsFrom->FindPropertyDescByName(NewDesc.Name) : nullptr;
		if (Existing && ArcPropertyJsonPrivate::IsSameDescSchema(*Existing, NewDesc))
		{
			Descs.Add(*Existing);
		}
		else
		{
			Descs.Add(NewDesc);
		}
		Values.Add(FindField(*Entry, TEXT("value")));
	}
	if (Report.Errors.Num() != ErrorCount)
	{
		return;
	}

	const EPropertyBagAlterationResult Result = OutBag.AddProperties(Descs);
	if (Result != EPropertyBagAlterationResult::Success)
	{
		Report.Error(FString::Printf(
			TEXT("%s: the property bag rejected the schema (%s)."),
			*Location,
			*StaticEnum<EPropertyBagAlterationResult>()->GetNameStringByValue(static_cast<int64>(Result))));
		return;
	}
	const UPropertyBag* BagStruct = OutBag.GetPropertyBagStruct();
	void* Memory = OutBag.GetMutableValue().GetMemory();
	for (int32 Index = 0; Index < Descs.Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = Values[Index];
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			continue;
		}
		FProperty* Property = BagStruct ? BagStruct->FindPropertyByName(Descs[Index].Name) : nullptr;
		if (!Property || !Memory)
		{
			Report.Error(FString::Printf(
				TEXT("%s: bag property '%s' was not created."),
				*Location,
				*Descs[Index].Name.ToString()));
			continue;
		}
		ImportPropertyValue(
			*Property,
			Property->ContainerPtrToValuePtr<void>(Memory),
			Value,
			FString::Printf(TEXT("%s.%s.value"), *Location, *Descs[Index].Name.ToString()),
			Report,
			Hooks);
	}
}

#if WITH_EDITOR
TSharedRef<FJsonObject> ArcPropertyJson::DescribeField(
	const FProperty& Property,
	const void* DefaultContainer,
	const FArcPropertyJsonHooks& Hooks,
	const TFunctionRef<void(FJsonObject& Field)> AddRoleFields)
{
	FString ExtendedType;
	const FString CppType = Property.GetCPPType(&ExtendedType, 0);
	TSharedRef<FJsonObject> Field = MakeShared<FJsonObject>();
	Field->SetStringField(TEXT("name"), Property.GetName());
	Field->SetStringField(TEXT("type"), CppType + ExtendedType);
	AddRoleFields(*Field);
	const FString& BaseStruct = Property.GetMetaData(TEXT("BaseStruct"));
	if (!BaseStruct.IsEmpty())
	{
		Field->SetStringField(TEXT("baseStruct"), ArcStructTypeNames::GetTypeName<UScriptStruct>(ArcStructTypeNames::FindType<UScriptStruct>(BaseStruct)));
	}
	if (DefaultContainer)
	{
		const TSharedPtr<FJsonValue> Default = ExportPropertyValue(
			Property, Property.ContainerPtrToValuePtr<void>(DefaultContainer), Hooks);
		if (Default.IsValid())
		{
			Field->SetField(TEXT("default"), Default);
		}
	}
	const FString& Tooltip = Property.GetMetaData(TEXT("ToolTip"));
	if (!Tooltip.IsEmpty())
	{
		Field->SetStringField(TEXT("tooltip"), Tooltip);
	}
	ArcPropertyJsonPrivate::SetNumericMeta(*Field, Property, TEXT("ClampMin"), TEXT("clampMin"));
	ArcPropertyJsonPrivate::SetNumericMeta(*Field, Property, TEXT("ClampMax"), TEXT("clampMax"));
	const FString& Units = Property.GetMetaData(TEXT("Units"));
	if (!Units.IsEmpty())
	{
		Field->SetStringField(TEXT("units"), Units);
	}
	const UEnum* Enum = nullptr;
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
	{
		Enum = EnumProperty->GetEnum();
	}
	else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(&Property))
	{
		Enum = ByteProperty->Enum;
	}
	if (Enum)
	{
		TArray<TSharedPtr<FJsonValue>> EnumValues;
		const int32 Count = Enum->ContainsExistingMax() ? Enum->NumEnums() - 1 : Enum->NumEnums();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				EnumValues.Add(MakeShared<FJsonValueString>(Enum->GetAuthoredNameStringByIndex(Index)));
			}
		}
		Field->SetArrayField(TEXT("enumValues"), EnumValues);
	}
	return Field;
}
#endif

bool ArcPropertyJson::ParseAssetPath(
	const FString& InPath,
	const TCHAR* Example,
	FArcAssetPath& OutPath,
	FArcPropertyJsonReport& Report)
{
	const FString Path = InPath.TrimStartAndEnd();
	if (Path.IsEmpty())
	{
		Report.Error(FString::Printf(TEXT("Path is required (e.g. %s)."), Example));
		return false;
	}
	int32 Dot = INDEX_NONE;
	if (Path.FindChar(TEXT('.'), Dot))
	{
		OutPath.PackageName = Path.Left(Dot);
		OutPath.AssetName = Path.Mid(Dot + 1);
	}
	else
	{
		OutPath.PackageName = Path;
		OutPath.AssetName = FPackageName::GetShortName(Path);
	}
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(OutPath.PackageName, false, &Reason) || OutPath.AssetName.IsEmpty())
	{
		Report.Error(FString::Printf(TEXT("Invalid asset path '%s': %s"), *Path, *Reason.ToString()));
		return false;
	}
	const FString PackageShortName = FPackageName::GetShortName(OutPath.PackageName);
	if (!OutPath.AssetName.Equals(PackageShortName, ESearchCase::IgnoreCase))
	{
		Report.Error(FString::Printf(
			TEXT("Invalid asset path '%s': object name '%s' must match the package name '%s' (e.g. %s.%s)."),
			*Path,
			*OutPath.AssetName,
			*PackageShortName,
			*OutPath.PackageName,
			*PackageShortName));
		return false;
	}
	OutPath.ObjectPath = OutPath.PackageName + TEXT(".") + OutPath.AssetName;
	return true;
}
