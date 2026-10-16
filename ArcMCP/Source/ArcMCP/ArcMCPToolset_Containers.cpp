// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"

//------------------------------------------------------------------------------
// Generic FInstancedStruct-in-container editing.
//
// The engine's ObjectTools.set_properties treats a TSet/TArray/TMap as an opaque
// blob and structurally diffs it to decide inserts/removes, rebuilding elements
// from scratch. That rebuild path silently drops soft-object refs nested inside an
// FInstancedStruct (e.g. an item icon). These tools instead locate the live element
// and import only the requested fields into it, so soft-ptrs survive.
//------------------------------------------------------------------------------

namespace ArcMCPContainersPrivate
{
	using ArcMCPToolsetPrivate::JsonObjToString;
	using ArcMCPToolsetPrivate::StringToJsonObj;

	// NB: named Fail(), not MakeError() — the engine's global MakeError() template
	// (Misc/TValueOrError.h) is always visible and out-competes a same-named overload
	// for string-literal args, producing TValueOrError conversion errors.
	FString Fail(const FString& Message)
	{
		return ArcMCPToolsetPrivate::MakeError(Message);
	}

	// A resolved reference to the FInstancedStruct carried by one container element.
	// For a bare TSet/TArray/TMap<FInstancedStruct> the element IS the FInstancedStruct.
	// For a wrapper (e.g. FArcInstancedStruct { FInstancedStruct InstancedStruct; FName StructName; })
	// Inner points at the wrapped FInstancedStruct and NameProp/ElemPtr let us keep the
	// wrapper's cached StructName in sync when the type changes.
	struct FInnerRef
	{
		FInstancedStruct* Inner = nullptr;
		FNameProperty* NameProp = nullptr;
		void* ElemPtr = nullptr;
	};

	FString StripF(const FString& In)
	{
		FString Out = In;
		Out.RemoveFromStart(TEXT("F"));
		return Out;
	}

	bool StructNameMatches(const UScriptStruct* Struct, const FString& Wanted)
	{
		return Struct && Struct->GetName().Equals(StripF(Wanted), ESearchCase::IgnoreCase);
	}

	UScriptStruct* ResolveStruct(const FString& InName)
	{
		return FindFirstObject<UScriptStruct>(*StripF(InName), EFindFirstObjectOptions::NativeFirst);
	}

	UObject* ResolveObject(const FString& ObjectPath, FString& OutError)
	{
		if (ObjectPath.IsEmpty())
		{
			OutError = TEXT("Missing required parameter: ObjectPath");
			return nullptr;
		}
		UObject* Obj = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Obj)
		{
			OutError = FString::Printf(TEXT("Object not found: %s"), *ObjectPath);
		}
		return Obj;
	}

	// The FStructProperty describing the element (array Inner / set Element / map Value).
	FStructProperty* ElementStructProp(FProperty* ContainerProp)
	{
		if (FArrayProperty* Arr = CastField<FArrayProperty>(ContainerProp))
		{
			return CastField<FStructProperty>(Arr->Inner);
		}
		if (FSetProperty* Set = CastField<FSetProperty>(ContainerProp))
		{
			return CastField<FStructProperty>(Set->ElementProp);
		}
		if (FMapProperty* Map = CastField<FMapProperty>(ContainerProp))
		{
			return CastField<FStructProperty>(Map->ValueProp);
		}
		return nullptr;
	}

	// Find the FInstancedStruct carried by an element and the wrapper's StructName prop (if any).
	FInnerRef ResolveInner(const FStructProperty* ElemProp, void* ElemPtr)
	{
		FInnerRef Ref;
		Ref.ElemPtr = ElemPtr;
		if (!ElemProp || !ElemPtr)
		{
			return Ref;
		}
		UScriptStruct* ElemStruct = ElemProp->Struct;
		if (ElemStruct == FInstancedStruct::StaticStruct())
		{
			Ref.Inner = reinterpret_cast<FInstancedStruct*>(ElemPtr);
			return Ref;
		}
		for (TFieldIterator<FProperty> It(ElemStruct); It; ++It)
		{
			FStructProperty* StructField = CastField<FStructProperty>(*It);
			if (StructField && StructField->Struct == FInstancedStruct::StaticStruct())
			{
				Ref.Inner = StructField->ContainerPtrToValuePtr<FInstancedStruct>(ElemPtr);
				Ref.NameProp = CastField<FNameProperty>(ElemStruct->FindPropertyByName(FName(TEXT("StructName"))));
				break;
			}
		}
		return Ref;
	}

	// Keep a wrapper's cached StructName field aligned with the inner struct type.
	void SyncWrapperName(const FInnerRef& Ref)
	{
		if (Ref.NameProp && Ref.Inner && Ref.Inner->GetScriptStruct())
		{
			Ref.NameProp->SetPropertyValue_InContainer(Ref.ElemPtr, Ref.Inner->GetScriptStruct()->GetFName());
		}
	}

	// Element as JSON: { structType, fields:{...} }. Inner may be an empty (typeless) struct.
	TSharedRef<FJsonObject> ElementToJson(const FInstancedStruct& Inner)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		const UScriptStruct* Struct = Inner.GetScriptStruct();
		Out->SetStringField(TEXT("structType"), Struct ? Struct->GetName() : FString());
		TSharedRef<FJsonObject> Fields = MakeShared<FJsonObject>();
		if (Struct && Inner.GetMemory())
		{
			FJsonObjectConverter::UStructToJsonObject(Struct, Inner.GetMemory(), Fields, 0, 0);
		}
		Out->SetObjectField(TEXT("fields"), Fields);
		return Out;
	}

	// Selector: {"structType":"X"} | {"index":N} | {"key":"K"}.
	// Locates a matching element. Returns false with OutError when unmatched / malformed.
	bool LocateElement(UObject* Obj, FProperty* ContainerProp, const FString& SelectorJson,
		FInnerRef& OutRef, int32& OutIndex, FString& OutError)
	{
		FStructProperty* ElemProp = ElementStructProp(ContainerProp);
		if (!ElemProp)
		{
			OutError = TEXT("Container element is not a struct type");
			return false;
		}

		TSharedPtr<FJsonObject> Selector = StringToJsonObj(SelectorJson);
		if (!Selector.IsValid())
		{
			OutError = TEXT("Failed to parse Selector as a JSON object");
			return false;
		}

		FString WantType;
		const bool bByType = Selector->TryGetStringField(TEXT("structType"), WantType);
		double WantIndexD = 0.0;
		const bool bByIndex = Selector->TryGetNumberField(TEXT("index"), WantIndexD);
		FString WantKey;
		const bool bByKey = Selector->TryGetStringField(TEXT("key"), WantKey);

		if (!bByType && !bByIndex && !bByKey)
		{
			OutError = TEXT("Selector must provide one of: structType, index, key");
			return false;
		}

		if (FArrayProperty* Arr = CastField<FArrayProperty>(ContainerProp))
		{
			FScriptArrayHelper Helper(Arr, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
			if (bByIndex)
			{
				const int32 Idx = static_cast<int32>(WantIndexD);
				if (!Helper.IsValidIndex(Idx))
				{
					OutError = FString::Printf(TEXT("Array index out of range: %d (size %d)"), Idx, Helper.Num());
					return false;
				}
				OutIndex = Idx;
				OutRef = ResolveInner(ElemProp, Helper.GetRawPtr(Idx));
				return OutRef.Inner != nullptr;
			}
			for (int32 i = 0; i < Helper.Num(); ++i)
			{
				FInnerRef Ref = ResolveInner(ElemProp, Helper.GetRawPtr(i));
				if (Ref.Inner && StructNameMatches(Ref.Inner->GetScriptStruct(), WantType))
				{
					OutIndex = i;
					OutRef = Ref;
					return true;
				}
			}
			OutError = FString::Printf(TEXT("No array element with structType '%s'"), *WantType);
			return false;
		}

		if (FSetProperty* Set = CastField<FSetProperty>(ContainerProp))
		{
			FScriptSetHelper Helper(Set, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
			for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
			{
				if (!Helper.IsValidIndex(i))
				{
					continue;
				}
				FInnerRef Ref = ResolveInner(ElemProp, Helper.GetElementPtr(i));
				if (Ref.Inner && StructNameMatches(Ref.Inner->GetScriptStruct(), WantType))
				{
					OutIndex = i;
					OutRef = Ref;
					return true;
				}
			}
			OutError = FString::Printf(TEXT("No set element with structType '%s'"), *WantType);
			return false;
		}

		if (FMapProperty* Map = CastField<FMapProperty>(ContainerProp))
		{
			FScriptMapHelper Helper(Map, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
			for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
			{
				if (!Helper.IsValidIndex(i))
				{
					continue;
				}
				FString KeyText;
				Map->KeyProp->ExportTextItem_Direct(KeyText, Helper.GetKeyPtr(i), nullptr, nullptr, PPF_None);
				const bool bMatch = bByKey ? KeyText.Equals(WantKey, ESearchCase::IgnoreCase)
					: StructNameMatches(ResolveInner(ElemProp, Helper.GetValuePtr(i)).Inner
							? ResolveInner(ElemProp, Helper.GetValuePtr(i)).Inner->GetScriptStruct() : nullptr, WantType);
				if (bMatch)
				{
					OutIndex = i;
					OutRef = ResolveInner(ElemProp, Helper.GetValuePtr(i));
					return OutRef.Inner != nullptr;
				}
			}
			OutError = bByKey ? FString::Printf(TEXT("No map entry with key '%s'"), *WantKey)
				: FString::Printf(TEXT("No map value with structType '%s'"), *WantType);
			return false;
		}

		OutError = FString::Printf(TEXT("Property '%s' is not a TArray/TSet/TMap"), *ContainerProp->GetName());
		return false;
	}
}

//------------------------------------------------------------------------------
// GetStructInContainer
//------------------------------------------------------------------------------

FString UArcMCPToolset::GetStructInContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& Selector)
{
	using namespace ArcMCPContainersPrivate;

	FString Error;
	UObject* Obj = ResolveObject(ObjectPath, Error);
	if (!Obj)
	{
		return Fail(Error);
	}
	FProperty* ContainerProp = Obj->GetClass()->FindPropertyByName(FName(*ContainerProperty));
	if (!ContainerProp)
	{
		return Fail(FString::Printf(TEXT("Property not found: %s"), *ContainerProperty));
	}

	FInnerRef Ref;
	int32 Index = INDEX_NONE;
	if (!LocateElement(Obj, ContainerProp, Selector, Ref, Index, Error))
	{
		return Fail(Error);
	}

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("element"), ElementToJson(*Ref.Inner));
	return JsonObjToString(Result);
}

//------------------------------------------------------------------------------
// SetStructFieldInContainer
//------------------------------------------------------------------------------

FString UArcMCPToolset::SetStructFieldInContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& Selector, const FString& FieldValuesJson)
{
	using namespace ArcMCPContainersPrivate;

	FString Error;
	UObject* Obj = ResolveObject(ObjectPath, Error);
	if (!Obj)
	{
		return Fail(Error);
	}
	FProperty* ContainerProp = Obj->GetClass()->FindPropertyByName(FName(*ContainerProperty));
	if (!ContainerProp)
	{
		return Fail(FString::Printf(TEXT("Property not found: %s"), *ContainerProperty));
	}

	TSharedPtr<FJsonObject> Fields = StringToJsonObj(FieldValuesJson);
	if (!Fields.IsValid())
	{
		return Fail(TEXT("Failed to parse FieldValuesJson as a JSON object"));
	}

	FInnerRef Ref;
	int32 Index = INDEX_NONE;
	if (!LocateElement(Obj, ContainerProp, Selector, Ref, Index, Error))
	{
		return Fail(Error);
	}
	if (!Ref.Inner->GetScriptStruct() || !Ref.Inner->GetMutableMemory())
	{
		return Fail(TEXT("Selected element has no struct instance to edit"));
	}

	Obj->Modify();
	FText FailReason;
	const bool bOk = FJsonObjectConverter::JsonObjectToUStruct(
		Fields.ToSharedRef(), Ref.Inner->GetScriptStruct(), Ref.Inner->GetMutableMemory(), 0, 0, false, &FailReason);
	if (!bOk)
	{
		return Fail(FString::Printf(TEXT("Field import failed: %s"), *FailReason.ToString()));
	}
	Obj->MarkPackageDirty();

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("element"), ElementToJson(*Ref.Inner));
	return JsonObjToString(Result);
}

//------------------------------------------------------------------------------
// AddStructToContainer
//------------------------------------------------------------------------------

FString UArcMCPToolset::AddStructToContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& StructType, const FString& FieldValuesJson)
{
	using namespace ArcMCPContainersPrivate;

	FString Error;
	UObject* Obj = ResolveObject(ObjectPath, Error);
	if (!Obj)
	{
		return Fail(Error);
	}
	FProperty* ContainerProp = Obj->GetClass()->FindPropertyByName(FName(*ContainerProperty));
	if (!ContainerProp)
	{
		return Fail(FString::Printf(TEXT("Property not found: %s"), *ContainerProperty));
	}
	FStructProperty* ElemProp = ElementStructProp(ContainerProp);
	if (!ElemProp)
	{
		return Fail(TEXT("Container element is not a struct type"));
	}
	UScriptStruct* NewStruct = ResolveStruct(StructType);
	if (!NewStruct)
	{
		return Fail(FString::Printf(TEXT("Unknown struct type: %s"), *StructType));
	}

	// Optional field seed.
	TSharedPtr<FJsonObject> Fields;
	if (!FieldValuesJson.IsEmpty())
	{
		Fields = StringToJsonObj(FieldValuesJson);
		if (!Fields.IsValid())
		{
			return Fail(TEXT("Failed to parse FieldValuesJson as a JSON object"));
		}
	}

	Obj->Modify();

	if (FArrayProperty* Arr = CastField<FArrayProperty>(ContainerProp))
	{
		FScriptArrayHelper Helper(Arr, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
		const int32 NewIndex = Helper.AddValue();
		FInnerRef Ref = ResolveInner(ElemProp, Helper.GetRawPtr(NewIndex));
		Ref.Inner->InitializeAs(NewStruct);
		if (Fields.IsValid())
		{
			FJsonObjectConverter::JsonObjectToUStruct(Fields.ToSharedRef(), NewStruct, Ref.Inner->GetMutableMemory(), 0, 0);
		}
		SyncWrapperName(Ref);
		Obj->MarkPackageDirty();
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("index"), NewIndex);
		Result->SetObjectField(TEXT("element"), ElementToJson(*Ref.Inner));
		return JsonObjToString(Result);
	}

	if (FSetProperty* Set = CastField<FSetProperty>(ContainerProp))
	{
		FScriptSetHelper Helper(Set, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
		// Reject duplicate type: the set keys elements, so a second one would collapse on rehash.
		for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
		{
			if (!Helper.IsValidIndex(i))
			{
				continue;
			}
			FInnerRef Existing = ResolveInner(ElemProp, Helper.GetElementPtr(i));
			if (Existing.Inner && Existing.Inner->GetScriptStruct() == NewStruct)
			{
				return Fail(FString::Printf(TEXT("Set already contains a '%s' element; use SetStructFieldInContainer"), *NewStruct->GetName()));
			}
		}
		const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
		FInnerRef Ref = ResolveInner(ElemProp, Helper.GetElementPtr(NewIndex));
		Ref.Inner->InitializeAs(NewStruct);
		if (Fields.IsValid())
		{
			FJsonObjectConverter::JsonObjectToUStruct(Fields.ToSharedRef(), NewStruct, Ref.Inner->GetMutableMemory(), 0, 0);
		}
		SyncWrapperName(Ref); // must precede rehash: the set hashes on StructName
		Helper.Rehash();
		Obj->MarkPackageDirty();
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetObjectField(TEXT("element"), ElementToJson(*Ref.Inner));
		return JsonObjToString(Result);
	}

	if (CastField<FMapProperty>(ContainerProp))
	{
		return Fail(TEXT("AddStructToContainer does not support TMap (a key is required); add the entry via the item/asset JSON import, then SetStructFieldInContainer"));
	}

	return Fail(FString::Printf(TEXT("Property '%s' is not a TArray/TSet"), *ContainerProperty));
}

//------------------------------------------------------------------------------
// RemoveStructFromContainer
//------------------------------------------------------------------------------

FString UArcMCPToolset::RemoveStructFromContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& Selector)
{
	using namespace ArcMCPContainersPrivate;

	FString Error;
	UObject* Obj = ResolveObject(ObjectPath, Error);
	if (!Obj)
	{
		return Fail(Error);
	}
	FProperty* ContainerProp = Obj->GetClass()->FindPropertyByName(FName(*ContainerProperty));
	if (!ContainerProp)
	{
		return Fail(FString::Printf(TEXT("Property not found: %s"), *ContainerProperty));
	}

	FInnerRef Ref;
	int32 Index = INDEX_NONE;
	if (!LocateElement(Obj, ContainerProp, Selector, Ref, Index, Error))
	{
		return Fail(Error);
	}

	Obj->Modify();
	if (FArrayProperty* Arr = CastField<FArrayProperty>(ContainerProp))
	{
		FScriptArrayHelper Helper(Arr, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
		Helper.RemoveValues(Index, 1);
	}
	else if (FSetProperty* Set = CastField<FSetProperty>(ContainerProp))
	{
		FScriptSetHelper Helper(Set, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
		Helper.RemoveAt(Index);
	}
	else if (FMapProperty* Map = CastField<FMapProperty>(ContainerProp))
	{
		FScriptMapHelper Helper(Map, ContainerProp->ContainerPtrToValuePtr<void>(Obj));
		Helper.RemoveAt(Index);
	}
	Obj->MarkPackageDirty();

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("removed"), true);
	return JsonObjToString(Result);
}
