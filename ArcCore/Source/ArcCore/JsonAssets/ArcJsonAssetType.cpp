// Copyright Lukasz Baran. All Rights Reserved.

#include "JsonAssets/ArcJsonAssetType.h"

#include "JsonAssets/ArcJsonAssetsLog.h"
#include "JsonAssets/ArcJsonRecordSerializer.h"
#include "Misc/Paths.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogArcJsonAssets);

namespace ArcJsonAssetTypePrivate
{
	TArray<FArcJsonAssetTypeInfo>& GetRegisteredTypes()
	{
		static TArray<FArcJsonAssetTypeInfo> Types;
		return Types;
	}

	const FStructProperty* FindGuidProperty(const UScriptStruct& Struct, FName PropertyName)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(&Struct, PropertyName);
		if (!Property || Property->Struct != TBaseStructure<FGuid>::Get() || Property->ArrayDim != 1)
		{
			return nullptr;
		}
		return Property;
	}

	const FStructProperty* FindPrimaryAssetTypeProperty(const UScriptStruct& Struct, FName PropertyName)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(&Struct, PropertyName);
		if (!Property || Property->Struct != StaticStruct<FPrimaryAssetType>() || Property->ArrayDim != 1
			|| !ArcJsonRecord::IsSerializedField(Struct, *Property))
		{
			return nullptr;
		}
		return Property;
	}

	const FArcJsonAssetTypeInfo* FindRegisteredType(FPrimaryAssetType PrimaryAssetType)
	{
		for (const FArcJsonAssetTypeInfo& Info : GetRegisteredTypes())
		{
			if (Info.PrimaryAssetType == PrimaryAssetType)
			{
				return &Info;
			}
		}
		return nullptr;
	}

	bool IsValidTypeName(FPrimaryAssetType PrimaryAssetType)
	{
		const FString Name = PrimaryAssetType.ToString();
		for (const TCHAR Char : Name)
		{
			if (FChar::IsWhitespace(Char) || Char == TEXT('/') || FCString::Strchr(INVALID_LONGPACKAGE_CHARACTERS, Char))
			{
				return false;
			}
		}
		return !Name.IsEmpty();
	}
}

bool ArcJsonAssets::RegisterType(const FArcJsonAssetTypeInfo& Info)
{
	check(IsInGameThread());
	if (Info.Extension.IsEmpty() || Info.Extension.Contains(TEXT(".")) || Info.Extension.Contains(TEXT("/")))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: invalid extension '%ls'", *Info.Extension);
		return false;
	}
	if (!Info.Struct)
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: type for extension '%ls' has no struct", *Info.Extension);
		return false;
	}
	if (!Info.PrimaryAssetType.IsValid())
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: type for extension '%ls' has no primary asset type", *Info.Extension);
		return false;
	}
	if (!ArcJsonAssetTypePrivate::FindGuidProperty(*Info.Struct, Info.IdProperty))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: id property '%ls' of %ls is not an FGuid property",
			*Info.IdProperty.ToString(), *Info.Struct->GetPathName());
		return false;
	}
	if (!Info.TypeProperty.IsNone() && !ArcJsonAssetTypePrivate::FindPrimaryAssetTypeProperty(*Info.Struct, Info.TypeProperty))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: type property '%ls' of %ls is not a serialized FPrimaryAssetType property",
			*Info.TypeProperty.ToString(), *Info.Struct->GetPathName());
		return false;
	}
	if (FindTypeByExtension(Info.Extension))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: extension '%ls' is already registered", *Info.Extension);
		return false;
	}
	if (ArcJsonAssetTypePrivate::FindRegisteredType(Info.PrimaryAssetType))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: primary asset type '%ls' is already registered",
			*Info.PrimaryAssetType.ToString());
		return false;
	}
	ArcJsonAssetTypePrivate::GetRegisteredTypes().Add(Info);
	return true;
}

bool ArcJsonAssets::UnregisterType(const FString& Extension)
{
	check(IsInGameThread());
	return ArcJsonAssetTypePrivate::GetRegisteredTypes().RemoveAll([&Extension](const FArcJsonAssetTypeInfo& Info) -> bool
	{
		return Info.Extension.Equals(Extension, ESearchCase::IgnoreCase);
	}) > 0;
}

TConstArrayView<FArcJsonAssetTypeInfo> ArcJsonAssets::GetTypes()
{
	check(IsInGameThread());
	return ArcJsonAssetTypePrivate::GetRegisteredTypes();
}

const FArcJsonAssetTypeInfo* ArcJsonAssets::FindTypeByExtension(FStringView Extension)
{
	for (const FArcJsonAssetTypeInfo& Info : GetTypes())
	{
		if (FStringView(Info.Extension).Equals(Extension, ESearchCase::IgnoreCase))
		{
			return &Info;
		}
	}
	return nullptr;
}

const FArcJsonAssetTypeInfo* ArcJsonAssets::FindTypeByFilename(FStringView Filename)
{
	const FString Extension = FPaths::GetExtension(FString(Filename));
	return Extension.IsEmpty() ? nullptr : FindTypeByExtension(Extension);
}

const FStructProperty* ArcJsonAssets::FindIdProperty(const FArcJsonAssetTypeInfo& Info)
{
	return Info.Struct ? ArcJsonAssetTypePrivate::FindGuidProperty(*Info.Struct, Info.IdProperty) : nullptr;
}

const FStructProperty* ArcJsonAssets::FindTypeProperty(const FArcJsonAssetTypeInfo& Info)
{
	return Info.Struct && !Info.TypeProperty.IsNone() ? ArcJsonAssetTypePrivate::FindPrimaryAssetTypeProperty(*Info.Struct, Info.TypeProperty) : nullptr;
}

FPrimaryAssetId ArcJsonAssets::MakeRecordId(const FArcJsonAssetTypeInfo& Info, FConstStructView Record, FString* OutError)
{
	if (!Info.Struct || Record.GetScriptStruct() != Info.Struct)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("record struct %ls is not the struct of type %ls"),
				Record.GetScriptStruct() ? *Record.GetScriptStruct()->GetPathName() : TEXT("None"), *Info.PrimaryAssetType.ToString());
		}
		return FPrimaryAssetId();
	}
	const FStructProperty* IdProperty = FindIdProperty(Info);
	if (!IdProperty)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("type %ls has no FGuid id property"), *Info.PrimaryAssetType.ToString());
		}
		return FPrimaryAssetId();
	}
	const FGuid& Guid = *IdProperty->ContainerPtrToValuePtr<FGuid>(Record.GetMemory());
	if (!Guid.IsValid())
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("$.%ls is not a valid GUID"), *IdProperty->GetName());
		}
		return FPrimaryAssetId();
	}

	FPrimaryAssetType IdType = Info.PrimaryAssetType;
	if (const FStructProperty* TypeProperty = FindTypeProperty(Info))
	{
		const FPrimaryAssetType& RecordType = *TypeProperty->ContainerPtrToValuePtr<FPrimaryAssetType>(Record.GetMemory());
		if (RecordType.IsValid())
		{
			if (!ArcJsonAssetTypePrivate::IsValidTypeName(RecordType))
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("$.%ls '%ls' is not a valid primary asset type name"),
						*TypeProperty->GetName(), *RecordType.ToString());
				}
				return FPrimaryAssetId();
			}
			IdType = RecordType;
		}
	}
	return FPrimaryAssetId(IdType, FName(Guid.ToString(EGuidFormats::Digits)));
}
