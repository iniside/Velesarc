// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "ArcNamedPrimaryAssetId.h"
#include "AssetRegistry/AssetIdentifier.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/ArcCoreAssetManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonRecordSerializer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"

namespace ArcMCPToolsetRecordsPrivate
{
	struct FRecordWalker
	{
		const FPrimaryAssetId& Target;
		TArray<FString>& OutPaths;

		void WalkStruct(const UScriptStruct& Struct, const void* Data, const FString& Path) const;
		void WalkInstancedStruct(const FInstancedStruct& Instanced, const FString& Path) const;
		void WalkValue(const FProperty& Property, const void* Value, const FString& Path) const;
	};

	FString JoinPath(const FString& Path, const FString& Name)
	{
		return Path.IsEmpty() ? Name : Path + TEXT(".") + Name;
	}

	void FRecordWalker::WalkInstancedStruct(const FInstancedStruct& Instanced, const FString& Path) const
	{
		const UScriptStruct* Inner = Instanced.GetScriptStruct();
		const uint8* Memory = Instanced.GetMemory();
		if (Inner && Memory)
		{
			WalkStruct(*Inner, Memory, FString::Printf(TEXT("%s(%s)"), *Path, *Inner->GetName()));
		}
	}

	void FRecordWalker::WalkStruct(const UScriptStruct& Struct, const void* Data, const FString& Path) const
	{
		if (&Struct == FArcNamedPrimaryAssetId::StaticStruct())
		{
			if (static_cast<const FArcNamedPrimaryAssetId*>(Data)->AssetId == Target)
			{
				OutPaths.Add(Path);
			}
			return;
		}
		if (&Struct == FInstancedStruct::StaticStruct())
		{
			WalkInstancedStruct(*static_cast<const FInstancedStruct*>(Data), Path);
			return;
		}
		if (const FStructProperty* WrapperProperty = ArcJsonRecord::FindInstancedStructWrapperProperty(Struct))
		{
			WalkInstancedStruct(*WrapperProperty->ContainerPtrToValuePtr<FInstancedStruct>(Data), Path);
			return;
		}
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const FString Name = Property->GetAuthoredName();
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				const FString ElementName = Property->ArrayDim > 1 ? FString::Printf(TEXT("%s[%d]"), *Name, Index) : Name;
				WalkValue(*Property, Property->ContainerPtrToValuePtr<void>(Data, Index), JoinPath(Path, ElementName));
			}
		}
	}

	void FRecordWalker::WalkValue(const FProperty& Property, const void* Value, const FString& Path) const
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			WalkStruct(*StructProperty->Struct, Value, Path);
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				WalkValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), FString::Printf(TEXT("%s[%d]"), *Path, Index));
			}
		}
		else if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			FScriptSetHelper Helper(SetProperty, Value);
			int32 Logical = 0;
			for (FScriptSetHelper::FIterator It(Helper); It; ++It)
			{
				WalkValue(*SetProperty->ElementProp, Helper.GetElementPtr(It), FString::Printf(TEXT("%s[%d]"), *Path, Logical));
				++Logical;
			}
		}
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			FScriptMapHelper Helper(MapProperty, Value);
			int32 Logical = 0;
			for (FScriptMapHelper::FIterator It(Helper); It; ++It)
			{
				WalkValue(*MapProperty->KeyProp, Helper.GetKeyPtr(It), FString::Printf(TEXT("%s[%d].Key"), *Path, Logical));
				WalkValue(*MapProperty->ValueProp, Helper.GetValuePtr(It), FString::Printf(TEXT("%s[%d].Value"), *Path, Logical));
				++Logical;
			}
		}
	}
}

FString UArcMCPToolset::FindRecordReferencers(const FString& RecordId)
{
	using namespace ArcMCPToolsetRecordsPrivate;

	const FPrimaryAssetId Id = FPrimaryAssetId::FromString(RecordId);
	if (!Id.IsValid())
	{
		return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("'%s' is not a record id (expected Type:Name)"), *RecordId));
	}
	const FString IdString = Id.ToString();

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	if (AssetRegistry.IsLoadingAssets())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Asset Registry is still loading; retry when the scan has finished"));
	}

	UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();

	const FArcJsonAssetTypeInfo* IdType = AssetManager.FindJsonRecordType(Id.PrimaryAssetType);
	const bool bIndexed = IdType && IdType->Struct && AssetManager.FindJsonRecord(Id, *IdType->Struct) != nullptr;

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(FAssetIdentifier(FArcNamedPrimaryAssetId::StaticStruct(), FName(IdString)), Referencers,
		UE::AssetRegistry::EDependencyCategory::SearchableName);
	TArray<FString> AssetNames;
	for (const FAssetIdentifier& Referencer : Referencers)
	{
		if (!Referencer.PackageName.IsNone())
		{
			AssetNames.AddUnique(Referencer.PackageName.ToString());
		}
	}
	AssetNames.Sort();

	TArray<TSharedPtr<FJsonValue>> AssetValues;
	for (const FString& AssetName : AssetNames)
	{
		AssetValues.Add(MakeShared<FJsonValueString>(AssetName));
	}

	TArray<TSharedPtr<FJsonValue>> RecordValues;
	for (const FPrimaryAssetType& Type : AssetManager.GetJsonRecordTypes())
	{
		const FArcJsonAssetTypeInfo* Info = AssetManager.FindJsonRecordType(Type);
		if (!Info || !Info->Struct)
		{
			continue;
		}
		for (const FArcJsonRecordEntry& Entry : AssetManager.GetJsonRecordEntries(Type))
		{
			const void* Record = AssetManager.FindJsonRecord(Entry.Id, *Info->Struct);
			if (!Record)
			{
				continue;
			}
			TArray<FString> Paths;
			const FRecordWalker Walker{ Id, Paths };
			Walker.WalkStruct(*Info->Struct, Record, FString());
			if (Paths.IsEmpty())
			{
				continue;
			}
			const FString EntryId = Entry.Id.ToString();
			const FString File = AssetManager.GetJsonRecordFilename(Entry.Id);
			for (const FString& Path : Paths)
			{
				TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
				Hit->SetStringField(TEXT("id"), EntryId);
				Hit->SetStringField(TEXT("file"), File);
				Hit->SetStringField(TEXT("property"), Path);
				RecordValues.Add(MakeShared<FJsonValueObject>(Hit));
			}
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("id"), IdString);
	Result->SetBoolField(TEXT("indexed"), bIndexed);
	Result->SetArrayField(TEXT("assets"), AssetValues);
	Result->SetArrayField(TEXT("records"), RecordValues);
	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
