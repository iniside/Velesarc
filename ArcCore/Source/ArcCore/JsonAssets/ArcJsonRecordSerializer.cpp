// Copyright Lukasz Baran. All Rights Reserved.

#include "JsonAssets/ArcJsonRecordSerializer.h"

#include "ArcYyJson.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "CoreGlobals.h"
#include "Curves/RichCurve.h"
#include "GameplayTagContainer.h"
#include "Internationalization/Text.h"
#include "Json/ArcStructTypeNames.h"
#include "Logging/LogMacros.h"
#include "Misc/Guid.h"
#include "Misc/StringBuilder.h"
#include "Misc/StringOutputDevice.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcJsonRecord, Log, All);

namespace ArcJsonRecordSerializerPrivate
{
	struct FPath
	{
		const FPath* Parent = nullptr;
		FStringView Field;
		FUtf8StringView Key;
		int32 Index = INDEX_NONE;
		bool bKey = false;
	};

	FPath MakeFieldPath(const FPath* Parent, FStringView Field)
	{
		FPath Path;
		Path.Parent = Parent;
		Path.Field = Field;
		return Path;
	}

	FPath MakeIndexPath(const FPath* Parent, int32 Index)
	{
		FPath Path;
		Path.Parent = Parent;
		Path.Index = Index;
		return Path;
	}

	FPath MakeKeyPath(const FPath* Parent, FUtf8StringView Key)
	{
		FPath Path;
		Path.Parent = Parent;
		Path.Key = Key;
		Path.bKey = true;
		return Path;
	}

	void AppendPath(const FPath* Path, FStringBuilderBase& Out)
	{
		if (Path == nullptr)
		{
			return;
		}
		AppendPath(Path->Parent, Out);
		if (Path->bKey)
		{
			Out << TEXT('[') << FString::ConstructFromPtrSize(Path->Key.GetData(), Path->Key.Len()) << TEXT(']');
		}
		else if (Path->Index != INDEX_NONE)
		{
			Out << TEXT('[') << Path->Index << TEXT(']');
		}
		else
		{
			if (Out.Len() > 0)
			{
				Out << TEXT('.');
			}
			Out << Path->Field;
		}
	}

	void AddError(FArcJsonRecordReport& Report, const FPath* Path, const FString& Message)
	{
		TStringBuilder<256> Builder;
		AppendPath(Path, Builder);
		if (Builder.Len() > 0)
		{
			Builder << TEXT(": ");
		}
		Builder << Message;
		Report.Errors.Emplace(Builder.ToView());
	}

	FUtf8StringView ToUtf8View(const yyjson_val* Value)
	{
		return FUtf8StringView(reinterpret_cast<const UTF8CHAR*>(yyjson_get_str(Value)), static_cast<int32>(yyjson_get_len(Value)));
	}

	FUtf8StringView StructTypeKeyUtf8()
	{
		static const FTCHARToUTF8 Converted(ArcStructTypeNames::StructTypeKey);
		return FUtf8StringView(reinterpret_cast<const UTF8CHAR*>(Converted.Get()), Converted.Length());
	}

	bool IsStructTypeKey(const yyjson_val* Key)
	{
		const FUtf8StringView TypeKey = StructTypeKeyUtf8();
		return yyjson_equals_strn(Key, reinterpret_cast<const char*>(TypeKey.GetData()), static_cast<size_t>(TypeKey.Len()));
	}

	yyjson_mut_val* CopyUtf8(yyjson_mut_doc* Doc, FUtf8StringView Text)
	{
		return yyjson_mut_strncpy(Doc, reinterpret_cast<const char*>(Text.GetData()), static_cast<size_t>(Text.Len()));
	}

	struct FKeyEntry
	{
		const FProperty* Property = nullptr;
		FString Name;
		TArray<ANSICHAR> Utf8Name;
	};

	struct FKeyTable
	{
		TArray<FKeyEntry> Entries;
	};

	struct FInstancedStructWrapper
	{
		const FStructProperty* InstancedStructProperty = nullptr;
		TFunction<void(void* WrapperValue)> PostImport;
	};

	TMap<const UScriptStruct*, FInstancedStructWrapper>& GetInstancedStructWrappers()
	{
		static TMap<const UScriptStruct*, FInstancedStructWrapper> Wrappers;
		return Wrappers;
	}

	const FInstancedStructWrapper* FindInstancedStructWrapper(const UScriptStruct& Struct)
	{
		check(IsInGameThread());
		return GetInstancedStructWrappers().Find(&Struct);
	}

	void BuildKeyTable(const UScriptStruct& Struct, FKeyTable& Out)
	{
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!ArcJsonRecord::IsSerializedField(Struct, *Property))
			{
				continue;
			}
			FKeyEntry& Entry = Out.Entries.AddDefaulted_GetRef();
			Entry.Property = Property;
			Entry.Name = Struct.GetAuthoredNameForField(Property);
			const FTCHARToUTF8 Utf8(*Entry.Name, Entry.Name.Len());
			Entry.Utf8Name.Append(reinterpret_cast<const ANSICHAR*>(Utf8.Get()), Utf8.Length());
		}
	}

	TMap<const UScriptStruct*, TUniquePtr<FKeyTable>>& GetKeyTableCache()
	{
		static TMap<const UScriptStruct*, TUniquePtr<FKeyTable>> Cache;
		return Cache;
	}

	const FKeyTable& GetKeyTable(const UScriptStruct& Struct, FKeyTable& Scratch)
	{
		check(IsInGameThread());
		if ((Struct.StructFlags & STRUCT_Native) == 0)
		{
			BuildKeyTable(Struct, Scratch);
			return Scratch;
		}
		TUniquePtr<FKeyTable>& Slot = GetKeyTableCache().FindOrAdd(&Struct);
		if (!Slot.IsValid())
		{
			Slot = MakeUnique<FKeyTable>();
			BuildKeyTable(Struct, *Slot);
		}
		return *Slot;
	}

	int32 FindKey(const FKeyTable& Table, const yyjson_val* Key)
	{
		for (int32 Index = 0; Index < Table.Entries.Num(); ++Index)
		{
			const TArray<ANSICHAR>& Name = Table.Entries[Index].Utf8Name;
			if (yyjson_equals_strn(Key, Name.GetData(), static_cast<size_t>(Name.Num())))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	const UEnum* FindEnum(const FProperty& Property)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
		{
			return EnumProperty->GetEnum();
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(&Property))
		{
			return ByteProperty->Enum;
		}
		return nullptr;
	}

	const FNumericProperty* GetEnumStorage(const FProperty& Property)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
		{
			return EnumProperty->GetUnderlyingProperty();
		}
		return CastField<FNumericProperty>(&Property);
	}

	bool IsStringForm(const FProperty& Property)
	{
		if (Property.IsA<FStrProperty>() || Property.IsA<FNameProperty>() || Property.IsA<FSoftObjectProperty>())
		{
			return true;
		}
		if (FindEnum(Property) != nullptr)
		{
			return true;
		}
		const FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
		if (StructProperty == nullptr)
		{
			return false;
		}
		const UScriptStruct* Struct = StructProperty->Struct;
		return Struct == FGameplayTag::StaticStruct()
			|| Struct == TBaseStructure<FGuid>::Get()
			|| Struct->IsChildOf(TBaseStructure<FSoftObjectPath>::Get())
			|| (Struct != FInstancedStruct::StaticStruct()
				&& Struct != FGameplayTagContainer::StaticStruct()
				&& Struct != FInstancedPropertyBag::StaticStruct()
				&& FindInstancedStructWrapper(*Struct) == nullptr
				&& ArcJsonRecord::IsNativeTextStruct(*Struct));
	}

	bool ExpectString(const yyjson_val* Json, const FPath* Path, const TCHAR* What, FArcJsonRecordReport& Report)
	{
		if (yyjson_is_str(Json))
		{
			return true;
		}
		AddError(Report, Path, FString::Printf(TEXT("expected %s."), What));
		return false;
	}

#if WITH_METADATA
	const UScriptStruct* ResolveBaseStruct(const FProperty& Property)
	{
		const FString& BasePath = Property.GetMetaData(TEXT("BaseStruct"));
		return BasePath.IsEmpty() ? nullptr : ArcStructTypeNames::FindType<UScriptStruct>(BasePath);
	}
#endif

	void ImportValue(
		const FProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report);

	void ImportFields(
		const UScriptStruct& Struct,
		void* Memory,
		yyjson_val* Object,
		const FPath* Path,
		bool bInstanced,
		FArcJsonRecordReport& Report);

	void ImportProperty(const FProperty& Property, void* Container, yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (Property.ArrayDim == 1)
		{
			ImportValue(Property, Property, Property.ContainerPtrToValuePtr<void>(Container), Json, Path, Report);
			return;
		}
		if (!yyjson_is_arr(Json) || yyjson_arr_size(Json) != static_cast<size_t>(Property.ArrayDim))
		{
			AddError(Report, Path, FString::Printf(TEXT("expected an array of %d values."), Property.ArrayDim));
			return;
		}
		yyjson_arr_iter Iter = yyjson_arr_iter_with(Json);
		for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
		{
			yyjson_val* Item = yyjson_arr_iter_next(&Iter);
			const FPath ItemPath = MakeIndexPath(Path, Index);
			ImportValue(Property, Property, Property.ContainerPtrToValuePtr<void>(Container, Index), Item, &ItemPath, Report);
		}
	}

	void ImportFields(
		const UScriptStruct& Struct,
		void* Memory,
		yyjson_val* Object,
		const FPath* Path,
		bool bInstanced,
		FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_obj(Object))
		{
			AddError(Report, Path, TEXT("expected an object."));
			return;
		}
		FKeyTable Scratch;
		const FKeyTable& Table = GetKeyTable(Struct, Scratch);
		TBitArray<> Seen(false, Table.Entries.Num());
		yyjson_obj_iter Iter = yyjson_obj_iter_with(Object);
		for (yyjson_val* Key = yyjson_obj_iter_next(&Iter); Key != nullptr; Key = yyjson_obj_iter_next(&Iter))
		{
			yyjson_val* Value = yyjson_obj_iter_get_val(Key);
			if (bInstanced && IsStructTypeKey(Key))
			{
				continue;
			}
			const int32 Index = FindKey(Table, Key);
			if (Index == INDEX_NONE)
			{
				const FString KeyName = ArcJson::ToFString(Key);
				const FPath UnknownPath = MakeFieldPath(Path, KeyName);
				AddError(Report, &UnknownPath, FString::Printf(TEXT("unknown field on %s."), *Struct.GetName()));
				continue;
			}
			const FKeyEntry& Entry = Table.Entries[Index];
			const FPath FieldPath = MakeFieldPath(Path, Entry.Name);
			if (Seen[Index])
			{
				AddError(Report, &FieldPath, TEXT("duplicate field."));
				continue;
			}
			Seen[Index] = true;
			ImportProperty(*Entry.Property, Memory, Value, &FieldPath, Report);
		}
	}

	void ImportInstancedStruct(
		const FStructProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_obj(Json))
		{
			AddError(Report, Path, TEXT("expected an object."));
			return;
		}
		FInstancedStruct& Instanced = *static_cast<FInstancedStruct*>(Value);
		if (yyjson_obj_size(Json) == 0)
		{
			Instanced.Reset();
			return;
		}
		const FUtf8StringView TypeKey = StructTypeKeyUtf8();
		const yyjson_val* TypeValue = yyjson_obj_getn(Json, reinterpret_cast<const char*>(TypeKey.GetData()), static_cast<size_t>(TypeKey.Len()));
		const FString TypeName = yyjson_is_str(TypeValue) ? ArcJson::ToFString(TypeValue) : FString();
		if (TypeName.IsEmpty())
		{
			AddError(Report, Path, FString::Printf(TEXT("an instanced struct needs a '%s' naming its struct type."), ArcStructTypeNames::StructTypeKey));
			return;
		}
		UScriptStruct* Type = ArcStructTypeNames::FindType<UScriptStruct>(TypeName);
		if (Type == nullptr)
		{
			AddError(Report, Path, FString::Printf(TEXT("unknown struct type '%s'."), *TypeName));
			return;
		}
#if WITH_METADATA
		const UScriptStruct* BaseStruct = ResolveBaseStruct(MetaProperty);
		const bool bExcludeBase = MetaProperty.HasMetaData(TEXT("ExcludeBaseStruct"))
			|| Property.HasMetaData(TEXT("ExcludeBaseStruct"));
		if (BaseStruct == nullptr && &MetaProperty != &Property)
		{
			BaseStruct = ResolveBaseStruct(Property);
		}
		if (BaseStruct != nullptr && !Type->IsChildOf(BaseStruct))
		{
			AddError(Report, Path, FString::Printf(TEXT("struct type '%s' is not a %s."), *Type->GetName(), *BaseStruct->GetName()));
			return;
		}
		if (BaseStruct != nullptr && bExcludeBase && Type == BaseStruct)
		{
			AddError(Report, Path, FString::Printf(TEXT("the base struct '%s' itself is not allowed here."), *BaseStruct->GetName()));
			return;
		}
#endif
		Instanced.InitializeAs(Type);
		ImportFields(*Type, Instanced.GetMutableMemory(), Json, Path, true, Report);
	}

	void ImportGameplayTag(FGameplayTag& OutTag, const yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (!ExpectString(Json, Path, TEXT("a gameplay tag name"), Report))
		{
			return;
		}
		const FString Name = ArcJson::ToFString(Json);
		if (Name.IsEmpty())
		{
			OutTag = FGameplayTag();
			return;
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name), false);
		if (!Tag.IsValid())
		{
			AddError(Report, Path, FString::Printf(TEXT("unknown gameplay tag '%s'."), *Name));
			return;
		}
		OutTag = Tag;
	}

	void ImportGameplayTagContainer(FGameplayTagContainer& OutTags, yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_arr(Json))
		{
			AddError(Report, Path, TEXT("expected an array of gameplay tag names."));
			return;
		}
		OutTags.Reset();
		yyjson_arr_iter Iter = yyjson_arr_iter_with(Json);
		int32 Index = 0;
		for (yyjson_val* Item = yyjson_arr_iter_next(&Iter); Item != nullptr; Item = yyjson_arr_iter_next(&Iter), ++Index)
		{
			const FPath ItemPath = MakeIndexPath(Path, Index);
			FGameplayTag Tag;
			const int32 ErrorCount = Report.Errors.Num();
			ImportGameplayTag(Tag, Item, &ItemPath, Report);
			if (Report.Errors.Num() != ErrorCount)
			{
				continue;
			}
			if (!Tag.IsValid())
			{
				AddError(Report, &ItemPath, TEXT("empty gameplay tag name."));
				continue;
			}
			OutTags.AddTag(Tag);
		}
	}

	void ImportTagQueryExpression(FGameplayTagQueryExpression& OutExpr, yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_obj(Json) || yyjson_obj_size(Json) != 1)
		{
			AddError(Report, Path, TEXT("expected an object with one tag query expression type key."));
			return;
		}
		yyjson_obj_iter Iter = yyjson_obj_iter_with(Json);
		yyjson_val* Key = yyjson_obj_iter_next(&Iter);
		yyjson_val* Items = yyjson_obj_iter_get_val(Key);
		const FString KeyName = ArcJson::ToFString(Key);
		const FPath ExprPath = MakeFieldPath(Path, KeyName);
		const UEnum* Enum = StaticEnum<EGameplayTagQueryExprType>();
		const int64 EnumValue = Enum->GetValueByNameString(KeyName);
		OutExpr.ExprType = EnumValue == INDEX_NONE ? EGameplayTagQueryExprType::Undefined : static_cast<EGameplayTagQueryExprType>(EnumValue);
		if (!OutExpr.UsesTagSet() && !OutExpr.UsesExprSet())
		{
			AddError(Report, &ExprPath, FString::Printf(TEXT("'%s' is not a tag query expression type."), *KeyName));
			return;
		}
		if (!yyjson_is_arr(Items))
		{
			AddError(Report, &ExprPath, OutExpr.UsesTagSet() ? TEXT("expected an array of gameplay tag names.") : TEXT("expected an array of tag query expressions."));
			return;
		}
		yyjson_arr_iter ItemIter = yyjson_arr_iter_with(Items);
		int32 Index = 0;
		for (yyjson_val* Item = yyjson_arr_iter_next(&ItemIter); Item != nullptr; Item = yyjson_arr_iter_next(&ItemIter), ++Index)
		{
			const FPath ItemPath = MakeIndexPath(&ExprPath, Index);
			if (!OutExpr.UsesTagSet())
			{
				FGameplayTagQueryExpression& Child = OutExpr.ExprSet.Add_GetRef(FGameplayTagQueryExpression());
				ImportTagQueryExpression(Child, Item, &ItemPath, Report);
				continue;
			}
			FGameplayTag Tag;
			const int32 ErrorCount = Report.Errors.Num();
			ImportGameplayTag(Tag, Item, &ItemPath, Report);
			if (Report.Errors.Num() != ErrorCount)
			{
				continue;
			}
			if (!Tag.IsValid())
			{
				AddError(Report, &ItemPath, TEXT("empty gameplay tag name."));
				continue;
			}
			OutExpr.TagSet.Add(Tag);
		}
	}

	void ImportGameplayTagQuery(FGameplayTagQuery& OutQuery, yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_obj(Json))
		{
			AddError(Report, Path, TEXT("expected an object."));
			return;
		}
		if (yyjson_obj_size(Json) == 0)
		{
			OutQuery.Clear();
			return;
		}
		FGameplayTagQueryExpression Root = FGameplayTagQueryExpression();
		const int32 ErrorCount = Report.Errors.Num();
		ImportTagQueryExpression(Root, Json, Path, Report);
		if (Report.Errors.Num() == ErrorCount)
		{
			OutQuery = FGameplayTagQuery::BuildQuery(Root);
		}
	}

	void ImportObjectReference(const FObjectProperty& Property, void* Value, const yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (Property.HasAnyPropertyFlags(CPF_InstancedReference))
		{
			AddError(Report, Path, TEXT("instanced object references are not supported."));
			return;
		}
		if (yyjson_is_null(Json))
		{
			Property.SetObjectPropertyValue(Value, nullptr);
			return;
		}
		const FString Text = yyjson_is_str(Json) ? ArcJson::ToFString(Json) : FString();
		if (Text.IsEmpty())
		{
			AddError(Report, Path, TEXT("expected an object path or null."));
			return;
		}
		UObject* Object = StaticFindObject(UObject::StaticClass(), nullptr, Text);
		if (Object == nullptr)
		{
			Object = StaticLoadObject(UObject::StaticClass(), nullptr, Text, {}, LOAD_NoWarn | LOAD_Quiet);
		}
		if (Object == nullptr)
		{
			AddError(Report, Path, FString::Printf(TEXT("cannot find object '%s'."), *Text));
			return;
		}
		if (!Object->IsA(Property.PropertyClass))
		{
			AddError(Report, Path, FString::Printf(TEXT("'%s' is not a %s."), *Text, *Property.PropertyClass->GetName()));
			return;
		}
		if (const FClassProperty* ClassProperty = CastField<FClassProperty>(&Property))
		{
			if (!CastChecked<UClass>(Object)->IsChildOf(ClassProperty->MetaClass))
			{
				AddError(Report, Path, FString::Printf(TEXT("'%s' is not a subclass of %s."), *Text, *ClassProperty->MetaClass->GetName()));
				return;
			}
		}
		Property.SetObjectPropertyValue(Value, Object);
	}

	void ImportNativeText(const FStructProperty& Property, void* Value, const yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (!ExpectString(Json, Path, TEXT("a string"), Report))
		{
			return;
		}
		const FString Text = ArcJson::ToFString(Json);
		if (Text.IsEmpty())
		{
			Property.ClearValue(Value);
			return;
		}
		FStringOutputDevice Errors;
		const TCHAR* End = Property.ImportText_Direct(*Text, Value, nullptr, PPF_None, &Errors);
		while (End != nullptr && FChar::IsWhitespace(*End))
		{
			++End;
		}
		if (End == nullptr || *End != TEXT('\0'))
		{
			AddError(Report, Path, FString::Printf(TEXT("cannot read '%s' as %s. %s"), *Text, *Property.Struct->GetName(), *Errors));
		}
	}

	void ImportStructValue(
		const FStructProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		const UScriptStruct* Struct = Property.Struct;
		if (Struct == FInstancedStruct::StaticStruct())
		{
			ImportInstancedStruct(Property, MetaProperty, Value, Json, Path, Report);
			return;
		}
		if (const FInstancedStructWrapper* Wrapper = FindInstancedStructWrapper(*Struct))
		{
			ImportInstancedStruct(Property, MetaProperty, Wrapper->InstancedStructProperty->ContainerPtrToValuePtr<void>(Value), Json, Path, Report);
			if (Wrapper->PostImport)
			{
				Wrapper->PostImport(Value);
			}
			return;
		}
		if (Struct == FGameplayTag::StaticStruct())
		{
			ImportGameplayTag(*static_cast<FGameplayTag*>(Value), Json, Path, Report);
			return;
		}
		if (Struct == FGameplayTagContainer::StaticStruct())
		{
			ImportGameplayTagContainer(*static_cast<FGameplayTagContainer*>(Value), Json, Path, Report);
			return;
		}
		if (Struct == FGameplayTagQuery::StaticStruct())
		{
			ImportGameplayTagQuery(*static_cast<FGameplayTagQuery*>(Value), Json, Path, Report);
			return;
		}
		if (Struct == TBaseStructure<FGuid>::Get())
		{
			if (!ExpectString(Json, Path, TEXT("a GUID of 32 hex digits"), Report))
			{
				return;
			}
			const FString Text = ArcJson::ToFString(Json);
			if (!FGuid::ParseExact(Text, EGuidFormats::Digits, *static_cast<FGuid*>(Value)))
			{
				AddError(Report, Path, FString::Printf(TEXT("'%s' is not a GUID of 32 hex digits."), *Text));
			}
			return;
		}
		if (Struct->IsChildOf(TBaseStructure<FSoftObjectPath>::Get()))
		{
			if (!ExpectString(Json, Path, TEXT("an object path"), Report))
			{
				return;
			}
			const FString Text = ArcJson::ToFString(Json);
			FSoftObjectPath& ObjectPath = *static_cast<FSoftObjectPath*>(Value);
			ObjectPath.SetPath(Text);
			if (!Text.IsEmpty() && ObjectPath.IsNull())
			{
				AddError(Report, Path, FString::Printf(TEXT("'%s' is not an object path."), *Text));
			}
			return;
		}
		if (Struct == FInstancedPropertyBag::StaticStruct())
		{
			AddError(Report, Path, TEXT("property bags are not supported."));
			return;
		}
		if (ArcJsonRecord::IsNativeTextStruct(*Struct))
		{
			ImportNativeText(Property, Value, Json, Path, Report);
			return;
		}
		ImportFields(*Struct, Value, Json, Path, false, Report);
	}

	void ImportNumber(const FNumericProperty& Numeric, void* Value, const yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (Numeric.IsFloatingPoint())
		{
			if (!yyjson_is_num(Json))
			{
				AddError(Report, Path, TEXT("expected a number."));
				return;
			}
			Numeric.SetFloatingPointPropertyValue(Value, yyjson_get_num(Json));
			return;
		}
		if (yyjson_is_uint(Json))
		{
			const uint64 Number = yyjson_get_uint(Json);
			Numeric.SetIntPropertyValue(Value, Number);
			if (Numeric.GetUnsignedIntPropertyValue(Value) != Number || (Number > static_cast<uint64>(MAX_int64) && !Numeric.IsA<FUInt64Property>()))
			{
				AddError(Report, Path, FString::Printf(TEXT("%llu is out of range for %s."), Number, *Numeric.GetCPPType()));
			}
			return;
		}
		if (yyjson_is_sint(Json))
		{
			const int64 Number = yyjson_get_sint(Json);
			Numeric.SetIntPropertyValue(Value, Number);
			if (Numeric.GetSignedIntPropertyValue(Value) != Number || (Number < 0 && Numeric.IsA<FUInt64Property>()))
			{
				AddError(Report, Path, FString::Printf(TEXT("%lld is out of range for %s."), Number, *Numeric.GetCPPType()));
			}
			return;
		}
		AddError(Report, Path, TEXT("expected an integer."));
	}

	void ImportEnum(const FProperty& Property, const UEnum& Enum, void* Value, const yyjson_val* Json, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (!ExpectString(Json, Path, TEXT("an enum value name"), Report))
		{
			return;
		}
		const FString Name = ArcJson::ToFString(Json);
		const int64 EnumValue = Enum.GetValueByNameString(Name);
		if (EnumValue == INDEX_NONE)
		{
			AddError(Report, Path, FString::Printf(TEXT("'%s' is not a value of %s."), *Name, *Enum.GetName()));
			return;
		}
		GetEnumStorage(Property)->SetIntPropertyValue(Value, EnumValue);
	}

	void ImportArray(
		const FArrayProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_arr(Json))
		{
			AddError(Report, Path, TEXT("expected an array."));
			return;
		}
		FScriptArrayHelper Helper(&Property, Value);
		const int32 Num = static_cast<int32>(yyjson_arr_size(Json));
		Helper.EmptyAndAddValues(Num);
		yyjson_arr_iter Iter = yyjson_arr_iter_with(Json);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			yyjson_val* Item = yyjson_arr_iter_next(&Iter);
			const FPath ItemPath = MakeIndexPath(Path, Index);
			ImportValue(*Property.Inner, MetaProperty, Helper.GetRawPtr(Index), Item, &ItemPath, Report);
		}
	}

	void ImportSet(
		const FSetProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		if (!yyjson_is_arr(Json))
		{
			AddError(Report, Path, TEXT("expected an array."));
			return;
		}
		FScriptSetHelper Helper(&Property, Value);
		Helper.EmptyElements(static_cast<int32>(yyjson_arr_size(Json)));
		yyjson_arr_iter Iter = yyjson_arr_iter_with(Json);
		int32 Index = 0;
		for (yyjson_val* Item = yyjson_arr_iter_next(&Iter); Item != nullptr; Item = yyjson_arr_iter_next(&Iter), ++Index)
		{
			const FPath ItemPath = MakeIndexPath(Path, Index);
			const FDefaultConstructedPropertyElement Element(Property.ElementProp);
			const int32 ErrorCount = Report.Errors.Num();
			ImportValue(*Property.ElementProp, MetaProperty, Element.GetObjAddress(), Item, &ItemPath, Report);
			if (Report.Errors.Num() != ErrorCount)
			{
				continue;
			}
			if (Helper.FindElementIndexFromHash(Element.GetObjAddress()) != INDEX_NONE)
			{
				AddError(Report, &ItemPath, TEXT("duplicate set element."));
				continue;
			}
			Helper.AddElement(Element.GetObjAddress());
		}
	}

	void AddMapEntry(
		const FMapProperty& Property,
		const FProperty& MetaProperty,
		FScriptMapHelper& Helper,
		yyjson_val* KeyJson,
		yyjson_val* ValueJson,
		const FPath* KeyPath,
		const FPath* ValuePath,
		FArcJsonRecordReport& Report)
	{
		const FDefaultConstructedPropertyElement KeyElement(Property.KeyProp);
		const int32 ErrorCount = Report.Errors.Num();
		ImportValue(*Property.KeyProp, MetaProperty, KeyElement.GetObjAddress(), KeyJson, KeyPath, Report);
		if (Report.Errors.Num() != ErrorCount)
		{
			return;
		}
		const int32 NumBefore = Helper.Num();
		void* EntryValue = Helper.FindOrAdd(KeyElement.GetObjAddress());
		if (Helper.Num() == NumBefore)
		{
			AddError(Report, KeyPath, TEXT("duplicate map key."));
			return;
		}
		if (ValueJson != nullptr)
		{
			ImportValue(*Property.ValueProp, MetaProperty, EntryValue, ValueJson, ValuePath, Report);
		}
	}

	void ImportMap(
		const FMapProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		FScriptMapHelper Helper(&Property, Value);
		if (IsStringForm(*Property.KeyProp))
		{
			if (!yyjson_is_obj(Json))
			{
				AddError(Report, Path, TEXT("expected an object."));
				return;
			}
			Helper.EmptyValues(static_cast<int32>(yyjson_obj_size(Json)));
			yyjson_obj_iter Iter = yyjson_obj_iter_with(Json);
			for (yyjson_val* Key = yyjson_obj_iter_next(&Iter); Key != nullptr; Key = yyjson_obj_iter_next(&Iter))
			{
				const FPath EntryPath = MakeKeyPath(Path, ToUtf8View(Key));
				AddMapEntry(Property, MetaProperty, Helper, Key, yyjson_obj_iter_get_val(Key), &EntryPath, &EntryPath, Report);
			}
			return;
		}
		if (!yyjson_is_arr(Json))
		{
			AddError(Report, Path, TEXT("expected an array of { \"Key\", \"Value\" } objects."));
			return;
		}
		Helper.EmptyValues(static_cast<int32>(yyjson_arr_size(Json)));
		yyjson_arr_iter Iter = yyjson_arr_iter_with(Json);
		int32 Index = 0;
		for (yyjson_val* Item = yyjson_arr_iter_next(&Iter); Item != nullptr; Item = yyjson_arr_iter_next(&Iter), ++Index)
		{
			const FPath EntryPath = MakeIndexPath(Path, Index);
			if (!yyjson_is_obj(Item))
			{
				AddError(Report, &EntryPath, TEXT("expected an object with \"Key\" and \"Value\"."));
				continue;
			}
			yyjson_val* KeyJson = nullptr;
			yyjson_val* ValueJson = nullptr;
			yyjson_obj_iter EntryIter = yyjson_obj_iter_with(Item);
			for (yyjson_val* Key = yyjson_obj_iter_next(&EntryIter); Key != nullptr; Key = yyjson_obj_iter_next(&EntryIter))
			{
				if (yyjson_equals_str(Key, "Key"))
				{
					KeyJson = yyjson_obj_iter_get_val(Key);
				}
				else if (yyjson_equals_str(Key, "Value"))
				{
					ValueJson = yyjson_obj_iter_get_val(Key);
				}
				else
				{
					const FString KeyName = ArcJson::ToFString(Key);
					const FPath UnknownPath = MakeFieldPath(&EntryPath, KeyName);
					AddError(Report, &UnknownPath, TEXT("unknown field on a map entry."));
				}
			}
			if (KeyJson == nullptr)
			{
				AddError(Report, &EntryPath, TEXT("a map entry needs a \"Key\"."));
				continue;
			}
			const FPath KeyPath = MakeFieldPath(&EntryPath, TEXT("Key"));
			const FPath ValuePath = MakeFieldPath(&EntryPath, TEXT("Value"));
			AddMapEntry(Property, MetaProperty, Helper, KeyJson, ValueJson, &KeyPath, &ValuePath, Report);
		}
	}

	void ImportValue(
		const FProperty& Property,
		const FProperty& MetaProperty,
		void* Value,
		yyjson_val* Json,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			ImportStructValue(*StructProperty, MetaProperty, Value, Json, Path, Report);
			return;
		}
		if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
		{
			if (!yyjson_is_bool(Json))
			{
				AddError(Report, Path, TEXT("expected a boolean."));
				return;
			}
			BoolProperty->SetPropertyValue(Value, yyjson_get_bool(Json));
			return;
		}
		if (const UEnum* Enum = FindEnum(Property))
		{
			ImportEnum(Property, *Enum, Value, Json, Path, Report);
			return;
		}
		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(&Property))
		{
			ImportNumber(*NumericProperty, Value, Json, Path, Report);
			return;
		}
		if (Property.IsA<FStrProperty>())
		{
			if (ExpectString(Json, Path, TEXT("a string"), Report))
			{
				*static_cast<FString*>(Value) = ArcJson::ToFString(Json);
			}
			return;
		}
		if (Property.IsA<FNameProperty>())
		{
			if (ExpectString(Json, Path, TEXT("a string"), Report))
			{
				*static_cast<FName*>(Value) = FName(*ArcJson::ToFString(Json));
			}
			return;
		}
		if (Property.IsA<FTextProperty>())
		{
			if (ExpectString(Json, Path, TEXT("a string"), Report))
			{
				*static_cast<FText*>(Value) = FText::FromString(ArcJson::ToFString(Json));
			}
			return;
		}
		if (Property.IsA<FSoftObjectProperty>())
		{
			if (!ExpectString(Json, Path, TEXT("an object path"), Report))
			{
				return;
			}
			const FString Text = ArcJson::ToFString(Json);
			FSoftObjectPath ObjectPath;
			ObjectPath.SetPath(Text);
			if (!Text.IsEmpty() && ObjectPath.IsNull())
			{
				AddError(Report, Path, FString::Printf(TEXT("'%s' is not an object path."), *Text));
				return;
			}
			*static_cast<FSoftObjectPtr*>(Value) = FSoftObjectPtr(ObjectPath);
			return;
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			ImportArray(*ArrayProperty, MetaProperty, Value, Json, Path, Report);
			return;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			ImportSet(*SetProperty, MetaProperty, Value, Json, Path, Report);
			return;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			ImportMap(*MapProperty, MetaProperty, Value, Json, Path, Report);
			return;
		}
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(&Property))
		{
			ImportObjectReference(*ObjectProperty, Value, Json, Path, Report);
			return;
		}
		if (Property.IsA<FObjectPropertyBase>())
		{
			AddError(Report, Path, TEXT("weak and lazy object references are not supported."));
			return;
		}
		AddError(Report, Path, FString::Printf(TEXT("unsupported property type %s."), *Property.GetCPPType()));
	}

	yyjson_mut_val* ExportValue(
		yyjson_mut_doc* Doc,
		const FProperty& Property,
		const void* Value,
		const FPath* Path,
		FArcJsonRecordReport& Report);

	void ExportFields(
		yyjson_mut_doc* Doc,
		yyjson_mut_val* Object,
		const UScriptStruct& Struct,
		const void* Memory,
		const void* Defaults,
		const FPath* Path,
		FArcJsonRecordReport& Report);

	bool IsIdenticalProperty(const FProperty& Property, const void* Container, const void* DefaultContainer)
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

	yyjson_mut_val* ExportProperty(yyjson_mut_doc* Doc, const FProperty& Property, const void* Container, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (Property.ArrayDim == 1)
		{
			return ExportValue(Doc, Property, Property.ContainerPtrToValuePtr<void>(Container), Path, Report);
		}
		yyjson_mut_val* Array = yyjson_mut_arr(Doc);
		for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
		{
			const FPath ItemPath = MakeIndexPath(Path, Index);
			yyjson_mut_val* Item = ExportValue(Doc, Property, Property.ContainerPtrToValuePtr<void>(Container, Index), &ItemPath, Report);
			if (Item != nullptr)
			{
				yyjson_mut_arr_append(Array, Item);
			}
		}
		return Array;
	}

	void ExportFields(
		yyjson_mut_doc* Doc,
		yyjson_mut_val* Object,
		const UScriptStruct& Struct,
		const void* Memory,
		const void* Defaults,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		FKeyTable Scratch;
		const FKeyTable& Table = GetKeyTable(Struct, Scratch);
		for (const FKeyEntry& Entry : Table.Entries)
		{
			if (Defaults != nullptr && IsIdenticalProperty(*Entry.Property, Memory, Defaults))
			{
				continue;
			}
			const FPath FieldPath = MakeFieldPath(Path, Entry.Name);
			yyjson_mut_val* Value = ExportProperty(Doc, *Entry.Property, Memory, &FieldPath, Report);
			if (Value != nullptr)
			{
				yyjson_mut_obj_add(Object, yyjson_mut_strncpy(Doc, Entry.Utf8Name.GetData(), static_cast<size_t>(Entry.Utf8Name.Num())), Value);
			}
		}
	}

	yyjson_mut_val* ExportInstancedStruct(yyjson_mut_doc* Doc, const FInstancedStruct& Instanced, const FPath* Path, FArcJsonRecordReport& Report)
	{
		yyjson_mut_val* Object = yyjson_mut_obj(Doc);
		if (const UScriptStruct* Type = Instanced.GetScriptStruct())
		{
			yyjson_mut_obj_add(
				Object,
				CopyUtf8(Doc, StructTypeKeyUtf8()),
				ArcJson::CopyString(Doc, ArcStructTypeNames::GetTypeName<UScriptStruct>(Type)));
			ExportFields(Doc, Object, *Type, Instanced.GetMemory(), nullptr, Path, Report);
		}
		return Object;
	}

	yyjson_mut_val* ExportTagQueryExpression(yyjson_mut_doc* Doc, const FGameplayTagQueryExpression& Expr, const FPath* Path, FArcJsonRecordReport& Report)
	{
		const UEnum* Enum = StaticEnum<EGameplayTagQueryExprType>();
		const FString Name = Expr.UsesTagSet() || Expr.UsesExprSet()
			? Enum->GetNameStringByValue(static_cast<int64>(Expr.ExprType))
			: FString();
		if (Name.IsEmpty())
		{
			AddError(Report, Path, FString::Printf(TEXT("%d is not a tag query expression type."), static_cast<int32>(Expr.ExprType)));
			return nullptr;
		}
		const FPath ExprPath = MakeFieldPath(Path, Name);
		yyjson_mut_val* Items = yyjson_mut_arr(Doc);
		if (Expr.UsesTagSet())
		{
			for (const FGameplayTag& Tag : Expr.TagSet)
			{
				yyjson_mut_arr_append(Items, ArcJson::CopyName(Doc, Tag.GetTagName()));
			}
		}
		else
		{
			for (int32 Index = 0; Index < Expr.ExprSet.Num(); ++Index)
			{
				const FPath ItemPath = MakeIndexPath(&ExprPath, Index);
				yyjson_mut_val* Item = ExportTagQueryExpression(Doc, Expr.ExprSet[Index], &ItemPath, Report);
				if (Item != nullptr)
				{
					yyjson_mut_arr_append(Items, Item);
				}
			}
		}
		yyjson_mut_val* Object = yyjson_mut_obj(Doc);
		yyjson_mut_obj_add(Object, ArcJson::CopyString(Doc, Name), Items);
		return Object;
	}

	yyjson_mut_val* ExportGameplayTagQuery(yyjson_mut_doc* Doc, const FGameplayTagQuery& Query, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (Query.IsEmpty())
		{
			return yyjson_mut_obj(Doc);
		}
		bool bAllTagsRegistered = true;
		for (const FGameplayTag& Tag : Query.GetGameplayTagArray())
		{
			if (!FGameplayTag::RequestGameplayTag(Tag.GetTagName(), false).IsValid())
			{
				AddError(Report, Path, FString::Printf(TEXT("unknown gameplay tag '%s'."), *Tag.GetTagName().ToString()));
				bAllTagsRegistered = false;
			}
		}
		if (!bAllTagsRegistered)
		{
			return nullptr;
		}
		FGameplayTagQueryExpression Root = FGameplayTagQueryExpression();
		Query.GetQueryExpr(Root);
		if (Root.ExprType == EGameplayTagQueryExprType::Undefined)
		{
			AddError(Report, Path, TEXT("tag query has no root expression."));
			return nullptr;
		}
		return ExportTagQueryExpression(Doc, Root, Path, Report);
	}

	yyjson_mut_val* ExportObjectReference(yyjson_mut_doc* Doc, const FObjectProperty& Property, const void* Value, const FPath* Path, FArcJsonRecordReport& Report)
	{
		if (Property.HasAnyPropertyFlags(CPF_InstancedReference))
		{
			AddError(Report, Path, TEXT("instanced object references are not supported."));
			return nullptr;
		}
		const UObject* Object = Property.GetObjectPropertyValue(Value);
		return Object != nullptr ? ArcJson::CopyString(Doc, Object->GetPathName()) : yyjson_mut_null(Doc);
	}

	yyjson_mut_val* ExportStructValue(
		yyjson_mut_doc* Doc,
		const FStructProperty& Property,
		const void* Value,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		const UScriptStruct* Struct = Property.Struct;
		if (Struct == FInstancedStruct::StaticStruct())
		{
			return ExportInstancedStruct(Doc, *static_cast<const FInstancedStruct*>(Value), Path, Report);
		}
		if (const FInstancedStructWrapper* Wrapper = FindInstancedStructWrapper(*Struct))
		{
			const FInstancedStruct& Instanced = *Wrapper->InstancedStructProperty->ContainerPtrToValuePtr<FInstancedStruct>(Value);
			return ExportInstancedStruct(Doc, Instanced, Path, Report);
		}
		if (Struct == FGameplayTagQuery::StaticStruct())
		{
			return ExportGameplayTagQuery(Doc, *static_cast<const FGameplayTagQuery*>(Value), Path, Report);
		}
		if (Struct == FGameplayTag::StaticStruct())
		{
			const FGameplayTag& Tag = *static_cast<const FGameplayTag*>(Value);
			return Tag.IsValid() ? ArcJson::CopyName(Doc, Tag.GetTagName()) : yyjson_mut_strncpy(Doc, "", 0);
		}
		if (Struct == FGameplayTagContainer::StaticStruct())
		{
			yyjson_mut_val* Array = yyjson_mut_arr(Doc);
			for (const FGameplayTag& Tag : *static_cast<const FGameplayTagContainer*>(Value))
			{
				yyjson_mut_arr_append(Array, ArcJson::CopyName(Doc, Tag.GetTagName()));
			}
			return Array;
		}
		if (Struct == TBaseStructure<FGuid>::Get())
		{
			return ArcJson::CopyString(Doc, static_cast<const FGuid*>(Value)->ToString(EGuidFormats::Digits));
		}
		if (Struct->IsChildOf(TBaseStructure<FSoftObjectPath>::Get()))
		{
			return ArcJson::CopyString(Doc, static_cast<const FSoftObjectPath*>(Value)->ToString());
		}
		if (Struct == FInstancedPropertyBag::StaticStruct())
		{
			AddError(Report, Path, TEXT("property bags are not supported."));
			return nullptr;
		}
		if (ArcJsonRecord::IsNativeTextStruct(*Struct))
		{
			FString Text;
			Property.ExportTextItem_Direct(Text, Value, nullptr, nullptr, PPF_None);
			return ArcJson::CopyString(Doc, Text);
		}
		yyjson_mut_val* Object = yyjson_mut_obj(Doc);
		ExportFields(Doc, Object, *Struct, Value, nullptr, Path, Report);
		return Object;
	}

	yyjson_mut_val* ExportNumber(yyjson_mut_doc* Doc, const FNumericProperty& Numeric, const void* Value)
	{
		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(&Numeric))
		{
			return yyjson_mut_float(Doc, FloatProperty->GetPropertyValue(Value));
		}
		if (Numeric.IsFloatingPoint())
		{
			return yyjson_mut_double(Doc, Numeric.GetFloatingPointPropertyValue(Value));
		}
		if (Numeric.IsA<FUInt64Property>())
		{
			return yyjson_mut_uint(Doc, Numeric.GetUnsignedIntPropertyValue(Value));
		}
		return yyjson_mut_sint(Doc, Numeric.GetSignedIntPropertyValue(Value));
	}

	yyjson_mut_val* ExportMap(
		yyjson_mut_doc* Doc,
		const FMapProperty& Property,
		const void* Value,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		const FScriptMapHelper Helper(&Property, Value);
		const bool bObjectForm = IsStringForm(*Property.KeyProp);
		yyjson_mut_val* Out = bObjectForm ? yyjson_mut_obj(Doc) : yyjson_mut_arr(Doc);
		for (FScriptMapHelper::FIterator It(Helper); It; ++It)
		{
			const FPath EntryPath = MakeIndexPath(Path, It.GetLogicalIndex());
			const FPath KeyPath = MakeFieldPath(&EntryPath, TEXT("Key"));
			const FPath ValuePath = MakeFieldPath(&EntryPath, TEXT("Value"));
			yyjson_mut_val* Key = ExportValue(Doc, *Property.KeyProp, Helper.GetKeyPtr(It), &KeyPath, Report);
			yyjson_mut_val* EntryValue = ExportValue(Doc, *Property.ValueProp, Helper.GetValuePtr(It), &ValuePath, Report);
			if (Key == nullptr || EntryValue == nullptr)
			{
				continue;
			}
			if (bObjectForm)
			{
				yyjson_mut_obj_add(Out, Key, EntryValue);
				continue;
			}
			yyjson_mut_val* Entry = yyjson_mut_obj(Doc);
			yyjson_mut_obj_add(Entry, yyjson_mut_str(Doc, "Key"), Key);
			yyjson_mut_obj_add(Entry, yyjson_mut_str(Doc, "Value"), EntryValue);
			yyjson_mut_arr_append(Out, Entry);
		}
		return Out;
	}

	yyjson_mut_val* ExportValue(
		yyjson_mut_doc* Doc,
		const FProperty& Property,
		const void* Value,
		const FPath* Path,
		FArcJsonRecordReport& Report)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			return ExportStructValue(Doc, *StructProperty, Value, Path, Report);
		}
		if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
		{
			return yyjson_mut_bool(Doc, BoolProperty->GetPropertyValue(Value));
		}
		if (const UEnum* Enum = FindEnum(Property))
		{
			const int64 EnumValue = GetEnumStorage(Property)->GetSignedIntPropertyValue(Value);
			const FString Name = Enum->GetNameStringByValue(EnumValue);
			if (Name.IsEmpty())
			{
				AddError(Report, Path, FString::Printf(TEXT("%lld is not a value of %s."), EnumValue, *Enum->GetName()));
				return nullptr;
			}
			return ArcJson::CopyString(Doc, Name);
		}
		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(&Property))
		{
			return ExportNumber(Doc, *NumericProperty, Value);
		}
		if (Property.IsA<FStrProperty>())
		{
			return ArcJson::CopyString(Doc, *static_cast<const FString*>(Value));
		}
		if (Property.IsA<FNameProperty>())
		{
			return ArcJson::CopyName(Doc, *static_cast<const FName*>(Value));
		}
		if (Property.IsA<FTextProperty>())
		{
			return ArcJson::CopyString(Doc, static_cast<const FText*>(Value)->ToString());
		}
		if (Property.IsA<FSoftObjectProperty>())
		{
			return ArcJson::CopyString(Doc, static_cast<const FSoftObjectPtr*>(Value)->ToSoftObjectPath().ToString());
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			yyjson_mut_val* Array = yyjson_mut_arr(Doc);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				const FPath ItemPath = MakeIndexPath(Path, Index);
				yyjson_mut_val* Item = ExportValue(Doc, *ArrayProperty->Inner, Helper.GetRawPtr(Index), &ItemPath, Report);
				if (Item != nullptr)
				{
					yyjson_mut_arr_append(Array, Item);
				}
			}
			return Array;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			const FScriptSetHelper Helper(SetProperty, Value);
			yyjson_mut_val* Array = yyjson_mut_arr(Doc);
			for (FScriptSetHelper::FIterator It(Helper); It; ++It)
			{
				const FPath ItemPath = MakeIndexPath(Path, It.GetLogicalIndex());
				yyjson_mut_val* Item = ExportValue(Doc, *SetProperty->ElementProp, Helper.GetElementPtr(It), &ItemPath, Report);
				if (Item != nullptr)
				{
					yyjson_mut_arr_append(Array, Item);
				}
			}
			return Array;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			return ExportMap(Doc, *MapProperty, Value, Path, Report);
		}
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(&Property))
		{
			return ExportObjectReference(Doc, *ObjectProperty, Value, Path, Report);
		}
		if (Property.IsA<FObjectPropertyBase>())
		{
			AddError(Report, Path, TEXT("weak and lazy object references are not supported."));
			return nullptr;
		}
		AddError(Report, Path, FString::Printf(TEXT("unsupported property type %s."), *Property.GetCPPType()));
		return nullptr;
	}
}

bool ArcJsonRecord::ImportStruct(const UScriptStruct& Struct, void* Out, const yyjson_val* Object, FArcJsonRecordReport& Report)
{
	const int32 ErrorCount = Report.Errors.Num();
	ArcJsonRecordSerializerPrivate::ImportFields(Struct, Out, const_cast<yyjson_val*>(Object), nullptr, false, Report);
	return Report.Errors.Num() == ErrorCount;
}

yyjson_mut_val* ArcJsonRecord::ExportStructDelta(yyjson_mut_doc* Doc, const UScriptStruct& Struct, const void* Value)
{
	FInstancedStruct Defaults;
	Defaults.InitializeAs(&Struct);
	FArcJsonRecordReport Report;
	yyjson_mut_val* Object = yyjson_mut_obj(Doc);
	ArcJsonRecordSerializerPrivate::ExportFields(Doc, Object, Struct, Value, Defaults.GetMemory(), nullptr, Report);
	if (Report.Errors.Num() == 0)
	{
		return Object;
	}
	for (const FString& Error : Report.Errors)
	{
		UE_LOGF(LogArcJsonRecord, Error, "Cannot export %ls: %ls", *Struct.GetName(), *Error);
	}
	return nullptr;
}

bool ArcJsonRecord::ParseRecord(const UScriptStruct& Struct, TArray<uint8>& Utf8, FInstancedStruct& Out, FArcJsonRecordReport& Report)
{
	FString ParseError;
	const ArcJson::FDocPtr Doc = ArcJson::ParseInSitu(Utf8, &ParseError);
	if (!Doc.IsValid())
	{
		Report.Errors.Add(FString::Printf(TEXT("invalid JSON: %s"), *ParseError));
		return false;
	}
	Out.InitializeAs(&Struct);
	return ImportStruct(Struct, Out.GetMutableMemory(), yyjson_doc_get_root(Doc.Get()), Report);
}

TArray<uint8> ArcJsonRecord::FormatRecord(const FInstancedStruct& Record)
{
	const UScriptStruct* Struct = Record.GetScriptStruct();
	if (Struct == nullptr)
	{
		UE_LOGF(LogArcJsonRecord, Error, "Cannot format an empty record.");
		return TArray<uint8>();
	}
	const ArcJson::FMutDocPtr Doc(yyjson_mut_doc_new(nullptr));
	yyjson_mut_val* Root = ExportStructDelta(Doc.Get(), *Struct, Record.GetMemory());
	if (Root == nullptr)
	{
		return TArray<uint8>();
	}
	yyjson_mut_doc_set_root(Doc.Get(), Root);
	return ArcJson::WritePretty(Doc.Get());
}

bool ArcJsonRecord::IsSerializedField(const UScriptStruct& Owner, const FProperty& Property)
{
	if (Property.HasAnyPropertyFlags(CPF_EditorOnly | CPF_Deprecated | CPF_Transient))
	{
		return false;
	}
	// Curve tangents, weights and extrapolation are bare UPROPERTY() fields (RichCurve.h:105-118, RealCurve.h:229-234).
	const bool bAllReflected = &Owner == FRichCurve::StaticStruct() || &Owner == FRichCurveKey::StaticStruct();
	return bAllReflected || Property.HasAnyPropertyFlags(CPF_Edit);
}

bool ArcJsonRecord::IsNativeTextStruct(const UScriptStruct& Struct)
{
	const uint32 TextFlags = STRUCT_ExportTextItemNative | STRUCT_ImportTextItemNative;
	return (static_cast<uint32>(Struct.StructFlags) & TextFlags) == TextFlags;
}

const FStructProperty* ArcJsonRecord::FindInstancedStructWrapperProperty(const UScriptStruct& Struct)
{
	const ArcJsonRecordSerializerPrivate::FInstancedStructWrapper* Wrapper = ArcJsonRecordSerializerPrivate::FindInstancedStructWrapper(Struct);
	return Wrapper ? Wrapper->InstancedStructProperty : nullptr;
}

bool ArcJsonRecord::RegisterInstancedStructWrapper(const UScriptStruct& Wrapper, FName InstancedStructProperty, TFunction<void(void* WrapperValue)> PostImport)
{
	check(IsInGameThread());
	const FStructProperty* Property = CastField<FStructProperty>(Wrapper.FindPropertyByName(InstancedStructProperty));
	if (Property == nullptr || Property->Struct != FInstancedStruct::StaticStruct() || Property->ArrayDim != 1)
	{
		UE_LOGF(LogArcJsonRecord, Error, "Cannot register %ls as an instanced struct wrapper: %ls is not an FInstancedStruct property of it.", *Wrapper.GetName(), *InstancedStructProperty.ToString());
		return false;
	}
	TMap<const UScriptStruct*, ArcJsonRecordSerializerPrivate::FInstancedStructWrapper>& Wrappers = ArcJsonRecordSerializerPrivate::GetInstancedStructWrappers();
	if (Wrappers.Contains(&Wrapper))
	{
		UE_LOGF(LogArcJsonRecord, Error, "Cannot register %ls as an instanced struct wrapper: already registered.", *Wrapper.GetName());
		return false;
	}
	ArcJsonRecordSerializerPrivate::FInstancedStructWrapper& Entry = Wrappers.Add(&Wrapper);
	Entry.InstancedStructProperty = Property;
	Entry.PostImport = MoveTemp(PostImport);
	return true;
}

void ArcJsonRecord::UnregisterInstancedStructWrapper(const UScriptStruct& Wrapper)
{
	check(IsInGameThread());
	ArcJsonRecordSerializerPrivate::GetInstancedStructWrappers().Remove(&Wrapper);
}
