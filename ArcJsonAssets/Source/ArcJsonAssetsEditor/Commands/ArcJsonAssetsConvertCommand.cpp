// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/ArcCoreAssetManager.h"
#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "JsonAssets/ArcJsonAssetFile.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonAssetsLog.h"
#include "JsonAssets/ArcJsonRecordSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Greater.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace ArcJsonAssetsConvertCommandPrivate
{
	bool IsTagQueryDescription(const UScriptStruct& Struct, const FProperty& Property)
	{
		static const FName UserDescriptionName(TEXT("UserDescription"));
		static const FName AutoDescriptionName(TEXT("AutoDescription"));
		return &Struct == FGameplayTagQuery::StaticStruct()
			&& (Property.GetFName() == UserDescriptionName || Property.GetFName() == AutoDescriptionName);
	}

	FString ExportForLog(const FProperty& Property, const void* Value)
	{
		FString Text;
		Property.ExportTextItem_Direct(Text, Value, nullptr, nullptr, PPF_None);
		return Text;
	}

	void AddDifference(TArray<FString>& OutDifferences, const FString& Path, const FString& Message)
	{
		OutDifferences.Add(FString::Printf(TEXT("%s: %s"), Path.IsEmpty() ? TEXT("<record>") : *Path, *Message));
	}

	FString MakeIndexPath(const FString& Path, int32 Index)
	{
		return FString::Printf(TEXT("%s[%d]"), *Path, Index);
	}

	void CompareValue(const FProperty& Property, const void* Expected, const void* Actual, const FString& Path, TArray<FString>& OutDifferences);

	void CompareFields(const UScriptStruct& Struct, const void* Expected, const void* Actual, const FString& Path, TArray<FString>& OutDifferences)
	{
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty& Property = **It;
			// The record stores a tag query as its expression; the descriptions are not part of the format.
			if (!ArcJsonRecord::IsSerializedField(Struct, Property) || IsTagQueryDescription(Struct, Property))
			{
				continue;
			}
			const FString Name = Struct.GetAuthoredNameForField(&Property);
			const FString FieldPath = Path.IsEmpty() ? Name : Path + TEXT(".") + Name;
			for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
			{
				CompareValue(Property, Property.ContainerPtrToValuePtr<void>(Expected, Index), Property.ContainerPtrToValuePtr<void>(Actual, Index),
					Property.ArrayDim == 1 ? FieldPath : MakeIndexPath(FieldPath, Index), OutDifferences);
			}
		}
	}

	void CompareLeaf(const FProperty& Property, const void* Expected, const void* Actual, const FString& Path, TArray<FString>& OutDifferences)
	{
		if (!Property.Identical(Expected, Actual, PPF_None))
		{
			AddDifference(OutDifferences, Path, FString::Printf(TEXT("asset %s, file %s"), *ExportForLog(Property, Expected), *ExportForLog(Property, Actual)));
		}
	}

	void CompareStruct(const FStructProperty& Property, const void* Expected, const void* Actual, const FString& Path, TArray<FString>& OutDifferences)
	{
		const UScriptStruct& Struct = *Property.Struct;
		if (&Struct == FInstancedStruct::StaticStruct())
		{
			const FInstancedStruct& ExpectedInstanced = *static_cast<const FInstancedStruct*>(Expected);
			const FInstancedStruct& ActualInstanced = *static_cast<const FInstancedStruct*>(Actual);
			const UScriptStruct* ExpectedType = ExpectedInstanced.GetScriptStruct();
			const UScriptStruct* ActualType = ActualInstanced.GetScriptStruct();
			if (ExpectedType != ActualType)
			{
				AddDifference(OutDifferences, Path, FString::Printf(TEXT("asset type %s, file type %s"),
					ExpectedType ? *ExpectedType->GetName() : TEXT("None"), ActualType ? *ActualType->GetName() : TEXT("None")));
				return;
			}
			if (ExpectedType)
			{
				CompareFields(*ExpectedType, ExpectedInstanced.GetMemory(), ActualInstanced.GetMemory(), Path, OutDifferences);
			}
			return;
		}
		if (const FStructProperty* WrappedProperty = ArcJsonRecord::FindInstancedStructWrapperProperty(Struct))
		{
			CompareStruct(*WrappedProperty, WrappedProperty->ContainerPtrToValuePtr<void>(Expected), WrappedProperty->ContainerPtrToValuePtr<void>(Actual),
				Path, OutDifferences);
			return;
		}
		if (&Struct == FGameplayTag::StaticStruct()
			|| &Struct == FGameplayTagContainer::StaticStruct()
			|| &Struct == TBaseStructure<FGuid>::Get()
			|| Struct.IsChildOf(TBaseStructure<FSoftObjectPath>::Get())
			|| &Struct == FInstancedPropertyBag::StaticStruct()
			|| ArcJsonRecord::IsNativeTextStruct(Struct))
		{
			CompareLeaf(Property, Expected, Actual, Path, OutDifferences);
			return;
		}
		CompareFields(Struct, Expected, Actual, Path, OutDifferences);
	}

	void CompareValue(const FProperty& Property, const void* Expected, const void* Actual, const FString& Path, TArray<FString>& OutDifferences)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			CompareStruct(*StructProperty, Expected, Actual, Path, OutDifferences);
			return;
		}
		if (Property.IsA<FTextProperty>())
		{
			// The record stores text as its display string, without key, namespace or culture flags.
			const FString ExpectedText = static_cast<const FText*>(Expected)->ToString();
			const FString ActualText = static_cast<const FText*>(Actual)->ToString();
			if (!ExpectedText.Equals(ActualText, ESearchCase::CaseSensitive))
			{
				AddDifference(OutDifferences, Path, FString::Printf(TEXT("asset \"%s\", file \"%s\""), *ExpectedText, *ActualText));
			}
			return;
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper ExpectedHelper(ArrayProperty, Expected);
			FScriptArrayHelper ActualHelper(ArrayProperty, Actual);
			if (ExpectedHelper.Num() != ActualHelper.Num())
			{
				AddDifference(OutDifferences, Path, FString::Printf(TEXT("asset has %d elements, file has %d"), ExpectedHelper.Num(), ActualHelper.Num()));
				return;
			}
			for (int32 Index = 0; Index < ExpectedHelper.Num(); ++Index)
			{
				CompareValue(*ArrayProperty->Inner, ExpectedHelper.GetRawPtr(Index), ActualHelper.GetRawPtr(Index), MakeIndexPath(Path, Index), OutDifferences);
			}
			return;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			const FScriptSetHelper ExpectedHelper(SetProperty, Expected);
			const FScriptSetHelper ActualHelper(SetProperty, Actual);
			if (ExpectedHelper.Num() != ActualHelper.Num())
			{
				AddDifference(OutDifferences, Path, FString::Printf(TEXT("asset has %d elements, file has %d"), ExpectedHelper.Num(), ActualHelper.Num()));
				return;
			}
			FScriptSetHelper::FIterator ActualIt(ActualHelper);
			for (FScriptSetHelper::FIterator ExpectedIt(ExpectedHelper); ExpectedIt; ++ExpectedIt, ++ActualIt)
			{
				CompareValue(*SetProperty->ElementProp, ExpectedHelper.GetElementPtr(ExpectedIt), ActualHelper.GetElementPtr(ActualIt),
					MakeIndexPath(Path, ExpectedIt.GetLogicalIndex()), OutDifferences);
			}
			return;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			const FScriptMapHelper ExpectedHelper(MapProperty, Expected);
			const FScriptMapHelper ActualHelper(MapProperty, Actual);
			if (ExpectedHelper.Num() != ActualHelper.Num())
			{
				AddDifference(OutDifferences, Path, FString::Printf(TEXT("asset has %d entries, file has %d"), ExpectedHelper.Num(), ActualHelper.Num()));
				return;
			}
			FScriptMapHelper::FIterator ActualIt(ActualHelper);
			for (FScriptMapHelper::FIterator ExpectedIt(ExpectedHelper); ExpectedIt; ++ExpectedIt, ++ActualIt)
			{
				const FString EntryPath = MakeIndexPath(Path, ExpectedIt.GetLogicalIndex());
				CompareValue(*MapProperty->KeyProp, ExpectedHelper.GetKeyPtr(ExpectedIt), ActualHelper.GetKeyPtr(ActualIt), EntryPath + TEXT(".Key"), OutDifferences);
				CompareValue(*MapProperty->ValueProp, ExpectedHelper.GetValuePtr(ExpectedIt), ActualHelper.GetValuePtr(ActualIt), EntryPath + TEXT(".Value"), OutDifferences);
			}
			return;
		}
		CompareLeaf(Property, Expected, Actual, Path, OutDifferences);
	}

	void CollectUnregisteredFields(const UScriptStruct& Struct, const void* Value, TSet<FName>& OutTags);

	void AddIfUnregistered(const FGameplayTag& Tag, TSet<FName>& OutTags)
	{
		const FName TagName = Tag.GetTagName();
		if (!TagName.IsNone() && !FGameplayTag::RequestGameplayTag(TagName, false).IsValid())
		{
			OutTags.Add(TagName);
		}
	}

	void CollectUnregisteredValue(const FProperty& Property, const void* Value, TSet<FName>& OutTags);

	void CollectUnregisteredStruct(const UScriptStruct& Struct, const void* Value, TSet<FName>& OutTags)
	{
		if (&Struct == FInstancedStruct::StaticStruct())
		{
			const FInstancedStruct& Instanced = *static_cast<const FInstancedStruct*>(Value);
			if (const UScriptStruct* Type = Instanced.GetScriptStruct())
			{
				CollectUnregisteredFields(*Type, Instanced.GetMemory(), OutTags);
			}
			return;
		}
		if (const FStructProperty* WrappedProperty = ArcJsonRecord::FindInstancedStructWrapperProperty(Struct))
		{
			CollectUnregisteredStruct(*WrappedProperty->Struct, WrappedProperty->ContainerPtrToValuePtr<void>(Value), OutTags);
			return;
		}
		if (&Struct == FGameplayTag::StaticStruct())
		{
			AddIfUnregistered(*static_cast<const FGameplayTag*>(Value), OutTags);
			return;
		}
		if (&Struct == FGameplayTagContainer::StaticStruct())
		{
			for (const FGameplayTag& Tag : *static_cast<const FGameplayTagContainer*>(Value))
			{
				AddIfUnregistered(Tag, OutTags);
			}
			return;
		}
		if (&Struct == FGameplayTagQuery::StaticStruct())
		{
			for (const FGameplayTag& Tag : static_cast<const FGameplayTagQuery*>(Value)->GetGameplayTagArray())
			{
				AddIfUnregistered(Tag, OutTags);
			}
			return;
		}
		if (&Struct == TBaseStructure<FGuid>::Get()
			|| Struct.IsChildOf(TBaseStructure<FSoftObjectPath>::Get())
			|| &Struct == FInstancedPropertyBag::StaticStruct()
			|| ArcJsonRecord::IsNativeTextStruct(Struct))
		{
			return;
		}
		CollectUnregisteredFields(Struct, Value, OutTags);
	}

	void CollectUnregisteredValue(const FProperty& Property, const void* Value, TSet<FName>& OutTags)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			CollectUnregisteredStruct(*StructProperty->Struct, Value, OutTags);
			return;
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				CollectUnregisteredValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), OutTags);
			}
			return;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			const FScriptSetHelper Helper(SetProperty, Value);
			for (FScriptSetHelper::FIterator It(Helper); It; ++It)
			{
				CollectUnregisteredValue(*SetProperty->ElementProp, Helper.GetElementPtr(It), OutTags);
			}
			return;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			const FScriptMapHelper Helper(MapProperty, Value);
			for (FScriptMapHelper::FIterator It(Helper); It; ++It)
			{
				CollectUnregisteredValue(*MapProperty->KeyProp, Helper.GetKeyPtr(It), OutTags);
				CollectUnregisteredValue(*MapProperty->ValueProp, Helper.GetValuePtr(It), OutTags);
			}
		}
	}

	void CollectUnregisteredFields(const UScriptStruct& Struct, const void* Value, TSet<FName>& OutTags)
	{
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty& Property = **It;
			if (!ArcJsonRecord::IsSerializedField(Struct, Property))
			{
				continue;
			}
			for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
			{
				CollectUnregisteredValue(Property, Property.ContainerPtrToValuePtr<void>(Value, Index), OutTags);
			}
		}
	}

	TArray<FString> SortTagNames(const TSet<FName>& Tags)
	{
		TArray<FString> Names;
		Names.Reserve(Tags.Num());
		for (const FName& Tag : Tags)
		{
			Names.Add(Tag.ToString());
		}
		Names.Sort();
		return Names;
	}

	bool IsNullInstancedStruct(const FProperty& Property, const void* Value)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
		if (!StructProperty)
		{
			return false;
		}
		if (StructProperty->Struct == FInstancedStruct::StaticStruct())
		{
			return !static_cast<const FInstancedStruct*>(Value)->IsValid();
		}
		if (const FStructProperty* WrappedProperty = ArcJsonRecord::FindInstancedStructWrapperProperty(*StructProperty->Struct))
		{
			return IsNullInstancedStruct(*WrappedProperty, WrappedProperty->ContainerPtrToValuePtr<void>(Value));
		}
		return false;
	}

	void RemoveNullElementsInFields(const UScriptStruct& Struct, void* Value, const FString& Path, TArray<FString>& OutRemoved);

	void RemoveNullElementsInValue(const FProperty& Property, void* Value, const FString& Path, TArray<FString>& OutRemoved)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			const UScriptStruct& Struct = *StructProperty->Struct;
			if (&Struct == FInstancedStruct::StaticStruct())
			{
				FInstancedStruct& Instanced = *static_cast<FInstancedStruct*>(Value);
				if (const UScriptStruct* Type = Instanced.GetScriptStruct())
				{
					RemoveNullElementsInFields(*Type, Instanced.GetMutableMemory(), Path, OutRemoved);
				}
				return;
			}
			if (const FStructProperty* WrappedProperty = ArcJsonRecord::FindInstancedStructWrapperProperty(Struct))
			{
				RemoveNullElementsInValue(*WrappedProperty, WrappedProperty->ContainerPtrToValuePtr<void>(Value), Path, OutRemoved);
				return;
			}
			if (&Struct == FGameplayTag::StaticStruct()
				|| &Struct == FGameplayTagContainer::StaticStruct()
				|| &Struct == FGameplayTagQuery::StaticStruct()
				|| &Struct == TBaseStructure<FGuid>::Get()
				|| Struct.IsChildOf(TBaseStructure<FSoftObjectPath>::Get())
				|| &Struct == FInstancedPropertyBag::StaticStruct()
				|| ArcJsonRecord::IsNativeTextStruct(Struct))
			{
				return;
			}
			RemoveNullElementsInFields(Struct, Value, Path, OutRemoved);
			return;
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			TArray<int32> NullIndices;
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				const FString ElementPath = MakeIndexPath(Path, Index);
				if (IsNullInstancedStruct(*ArrayProperty->Inner, Helper.GetRawPtr(Index)))
				{
					NullIndices.Add(Index);
					OutRemoved.Add(ElementPath);
				}
				else
				{
					RemoveNullElementsInValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), ElementPath, OutRemoved);
				}
			}
			for (int32 NullIndex = NullIndices.Num() - 1; NullIndex >= 0; --NullIndex)
			{
				Helper.RemoveValues(NullIndices[NullIndex], 1);
			}
			return;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			FScriptSetHelper Helper(SetProperty, Value);
			const int32 RemovedBefore = OutRemoved.Num();
			TArray<int32> NullIndices;
			for (FScriptSetHelper::FIterator It(Helper); It; ++It)
			{
				const FString ElementPath = MakeIndexPath(Path, It.GetLogicalIndex());
				if (IsNullInstancedStruct(*SetProperty->ElementProp, Helper.GetElementPtr(It)))
				{
					NullIndices.Add(It.GetInternalIndex());
					OutRemoved.Add(ElementPath);
				}
				else
				{
					RemoveNullElementsInValue(*SetProperty->ElementProp, Helper.GetElementPtr(It), ElementPath, OutRemoved);
				}
			}
			NullIndices.Sort(TGreater<int32>());
			for (const int32 NullIndex : NullIndices)
			{
				Helper.RemoveAt(NullIndex);
			}
			if (OutRemoved.Num() != RemovedBefore)
			{
				Helper.Rehash();
			}
			return;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			FScriptMapHelper Helper(MapProperty, Value);
			for (FScriptMapHelper::FIterator It(Helper); It; ++It)
			{
				RemoveNullElementsInValue(*MapProperty->ValueProp, Helper.GetValuePtr(It), MakeIndexPath(Path, It.GetLogicalIndex()) + TEXT(".Value"), OutRemoved);
			}
		}
	}

	void RemoveNullElementsInFields(const UScriptStruct& Struct, void* Value, const FString& Path, TArray<FString>& OutRemoved)
	{
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty& Property = **It;
			if (!ArcJsonRecord::IsSerializedField(Struct, Property))
			{
				continue;
			}
			const FString Name = Struct.GetAuthoredNameForField(&Property);
			const FString FieldPath = Path.IsEmpty() ? Name : Path + TEXT(".") + Name;
			for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
			{
				RemoveNullElementsInValue(Property, Property.ContainerPtrToValuePtr<void>(Value, Index),
					Property.ArrayDim == 1 ? FieldPath : MakeIndexPath(FieldPath, Index), OutRemoved);
			}
		}
	}

	bool MakeOutputFilename(const FString& Directory, const FString& AssetName, const FString& Extension, FString& OutFilename)
	{
		const FArcJsonAssetContentRoots& Roots = ArcJsonAssetPaths::GetContentRoots();
		FString TrimmedDirectory = Directory;
		while (TrimmedDirectory.Len() > 1 && (TrimmedDirectory.EndsWith(TEXT("/")) || TrimmedDirectory.EndsWith(TEXT("\\"))))
		{
			TrimmedDirectory.LeftChopInline(1);
		}
		const FString Candidate = TrimmedDirectory / (AssetName + TEXT(".") + Extension);
		FString Filename;
		if (!ArcJsonAssetPaths::ToFilename(Roots, Candidate, Filename))
		{
			Filename = Candidate;
		}
		OutFilename = ArcJsonAssetPaths::NormalizeFilename(Filename);
		FString PackagePath;
		return ArcJsonAssetPaths::ToPackagePath(Roots, OutFilename, PackagePath);
	}

	bool CopyFromAsset(const FArcJsonAssetTypeInfo& Type, const FStructProperty& IdProperty, const UObject& Asset, FInstancedStruct& OutRecord, TArray<FString>& OutErrors)
	{
		const UScriptStruct& Struct = *Type.Struct;
		const UClass& SourceClass = *Asset.GetClass();
		OutRecord.InitializeAs(&Struct);

		TSet<FName> MirroredNames;
		const FStructProperty* TypeProperty = ArcJsonAssets::FindTypeProperty(Type);
		bool bIdCopied = false;
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty& MirrorProperty = **It;
			if (!ArcJsonRecord::IsSerializedField(Struct, MirrorProperty))
			{
				continue;
			}
			MirroredNames.Add(MirrorProperty.GetFName());
			const FProperty* SourceProperty = SourceClass.FindPropertyByName(MirrorProperty.GetFName());
			if (!SourceProperty)
			{
				UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: %ls has no property %ls; the record keeps its default",
					*SourceClass.GetName(), *MirrorProperty.GetName());
				continue;
			}
			if (!MirrorProperty.SameType(SourceProperty) || MirrorProperty.ArrayDim != SourceProperty->ArrayDim)
			{
				OutErrors.Add(FString::Printf(TEXT("%s is %s in %s and %s in %s"), *MirrorProperty.GetName(),
					*SourceProperty->GetCPPType(), *SourceClass.GetName(), *MirrorProperty.GetCPPType(), *Struct.GetName()));
				continue;
			}
			if (&MirrorProperty == TypeProperty)
			{
				const FPrimaryAssetType SourceType = *SourceProperty->ContainerPtrToValuePtr<FPrimaryAssetType>(&Asset);
				FPrimaryAssetTypeInfo SourceTypeInfo;
				if (!SourceType.IsValid())
				{
					UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: %ls.%ls is None; the record keeps its default type",
						*SourceClass.GetName(), *MirrorProperty.GetName());
					continue;
				}
				if (UAssetManager::Get().GetPrimaryAssetTypeInfo(SourceType, SourceTypeInfo) && !SourceTypeInfo.bIsDynamicAsset)
				{
					UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: %ls.%ls is %ls, a type of scanned assets; the record keeps its default type",
						*SourceClass.GetName(), *MirrorProperty.GetName(), *SourceType.ToString());
					continue;
				}
			}
			MirrorProperty.CopyCompleteValue(MirrorProperty.ContainerPtrToValuePtr<void>(OutRecord.GetMutableMemory()), SourceProperty->ContainerPtrToValuePtr<void>(&Asset));
			bIdCopied |= &MirrorProperty == &IdProperty;
		}

		for (TFieldIterator<FProperty> It(&SourceClass); It; ++It)
		{
			if (!MirroredNames.Contains(It->GetFName()))
			{
				UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: %ls.%ls has no mirror property in %ls", *SourceClass.GetName(), *It->GetName(), *Struct.GetName());
			}
		}

		FGuid& Id = *IdProperty.ContainerPtrToValuePtr<FGuid>(OutRecord.GetMutableMemory());
		if (!bIdCopied)
		{
			Id = FGuid::NewGuid();
		}
		else if (!Id.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s has no valid %s"), *Asset.GetPathName(), *IdProperty.GetName()));
		}

		return OutErrors.IsEmpty();
	}

	enum class EConvertStatus : uint8
	{
		Converted,
		WouldConvert,
		SkippedExisting,
		Failed
	};

	struct FConvertOutcome
	{
		EConvertStatus Status = EConvertStatus::Failed;
		FString Filename;
		FString Reason;
		FPrimaryAssetId Id;
		int32 DifferenceCount = 0;
		TArray<FString> DroppedElements;
	};

	FConvertOutcome ConvertAsset(const FArcJsonAssetTypeInfo& Type, const FStructProperty& IdProperty, const UObject& Asset, const FString& Directory,
		bool bDryRun, TSet<FName>& InOutUnregisteredTags)
	{
		FConvertOutcome Outcome;
		if (!MakeOutputFilename(Directory, Asset.GetName(), Type.Extension, Outcome.Filename))
		{
			Outcome.Reason = FString::Printf(TEXT("%s is not under a JSON asset content root"), *Directory);
			return Outcome;
		}
		if (FPaths::FileExists(Outcome.Filename))
		{
			Outcome.Status = EConvertStatus::SkippedExisting;
			return Outcome;
		}

		FInstancedStruct Record;
		TArray<FString> Errors;
		if (!CopyFromAsset(Type, IdProperty, Asset, Record, Errors))
		{
			Outcome.Reason = FString::Join(Errors, TEXT("; "));
			return Outcome;
		}
		RemoveNullElementsInFields(*Type.Struct, Record.GetMutableMemory(), FString(), Outcome.DroppedElements);
		for (const FString& Dropped : Outcome.DroppedElements)
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Convert: %ls: %ls is an empty instanced struct (deleted type?); left out of the record",
				*Asset.GetPathName(), *Dropped);
		}
		TSet<FName> UnregisteredTags;
		CollectUnregisteredFields(*Type.Struct, Record.GetMemory(), UnregisteredTags);
		if (!UnregisteredTags.IsEmpty())
		{
			InOutUnregisteredTags.Append(UnregisteredTags);
			Outcome.Reason = FString::Printf(TEXT("unregistered gameplay tags %s"), *FString::Join(SortTagNames(UnregisteredTags), TEXT(", ")));
			return Outcome;
		}
		FString IdError;
		Outcome.Id = ArcJsonAssets::MakeRecordId(Type, Record, &IdError);
		if (!Outcome.Id.IsValid())
		{
			Outcome.Reason = IdError;
			return Outcome;
		}
		if (bDryRun)
		{
			Outcome.Status = EConvertStatus::WouldConvert;
			return Outcome;
		}

		if (!ArcJsonAssetFile::WriteRecordFile(Outcome.Filename, Record))
		{
			Outcome.Reason = TEXT("the record could not be written");
			return Outcome;
		}
		UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: wrote %ls from %ls", *Outcome.Filename, *Asset.GetPathName());

		if (UArcCoreAssetManager* AssetManager = Cast<UArcCoreAssetManager>(UAssetManager::GetIfInitialized()))
		{
			if (!AssetManager->ReindexJsonFile(Outcome.Filename))
			{
				UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Convert: %ls was not indexed", *Outcome.Filename);
			}
		}

		FInstancedStruct Parsed;
		if (!ArcJsonAssetFile::ReadRecordFile(Type, Outcome.Filename, Parsed))
		{
			Outcome.Reason = TEXT("written, but does not parse back; field check not run");
			return Outcome;
		}
		TArray<FString> Differences;
		CompareFields(*Type.Struct, Record.GetMemory(), Parsed.GetMemory(), FString(), Differences);
		for (const FString& Difference : Differences)
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Convert: %ls: %ls", *Outcome.Filename, *Difference);
		}
		Outcome.DifferenceCount = Differences.Num();
		Outcome.Status = EConvertStatus::Converted;
		return Outcome;
	}

	void LogOutcome(const FString& SourcePath, const FConvertOutcome& Outcome)
	{
		switch (Outcome.Status)
		{
		case EConvertStatus::Converted:
			if (Outcome.DifferenceCount == 0)
			{
				UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: %ls -> %ls (%ls): 0 differences",
					*SourcePath, *Outcome.Filename, *Outcome.Id.ToString());
			}
			else
			{
				UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Convert: %ls -> %ls (%ls): %d differences",
					*SourcePath, *Outcome.Filename, *Outcome.Id.ToString(), Outcome.DifferenceCount);
			}
			break;
		case EConvertStatus::WouldConvert:
			UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: would write %ls -> %ls (%ls)", *SourcePath, *Outcome.Filename, *Outcome.Id.ToString());
			break;
		case EConvertStatus::SkippedExisting:
			UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: skipped %ls: %ls already exists", *SourcePath, *Outcome.Filename);
			break;
		case EConvertStatus::Failed:
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Convert: %ls was not converted: %ls", *SourcePath, *Outcome.Reason);
			break;
		}
	}

	FString GetConvertMapFilename()
	{
		return FPaths::ProjectSavedDir() / TEXT("ArcJsonAssets") / TEXT("ConvertMap.json");
	}

	bool LoadConvertMap(TMap<FString, FString>& OutMap)
	{
		const FString Filename = GetConvertMapFilename();
		if (!FPaths::FileExists(Filename))
		{
			return true;
		}
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Filename) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Convert: %ls is not a JSON object; nothing converted", *Filename);
			return false;
		}
		for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Root->Values)
		{
			FString Value;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Value))
			{
				UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Convert: %ls: '%ls' is not a string; nothing converted", *Filename, *FString(Pair.Key));
				return false;
			}
			OutMap.Add(FString(Pair.Key), Value);
		}
		return true;
	}

	void SaveConvertMap(TMap<FString, FString> Map)
	{
		Map.KeySort(TLess<FString>());
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : Map)
		{
			Root->SetStringField(Pair.Key, Pair.Value);
		}
		FString Text;
		const FString Filename = GetConvertMapFilename();
		if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text))
			|| !FFileHelper::SaveStringToFile(Text, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Convert: %ls could not be written", *Filename);
			return;
		}
		UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Convert: %ls has %d entries", *Filename, Map.Num());
	}

	bool FindConvertibleType(FString Extension, const FArcJsonAssetTypeInfo*& OutType, const FStructProperty*& OutIdProperty)
	{
		Extension.RemoveFromStart(TEXT("."));
		OutType = ArcJsonAssets::FindTypeByExtension(Extension);
		OutIdProperty = OutType ? ArcJsonAssets::FindIdProperty(*OutType) : nullptr;
		if (!OutType || !OutType->Struct || !OutIdProperty)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Convert: .%ls is not a registered JSON asset type with an id property", *Extension);
			return false;
		}
		return true;
	}

	void ConvertCommand(const TArray<FString>& Args)
	{
		if (Args.Num() != 3)
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Convert <extension> <asset object path> <output package path or directory>");
			return;
		}
		const FArcJsonAssetTypeInfo* Type = nullptr;
		const FStructProperty* IdProperty = nullptr;
		if (!FindConvertibleType(Args[0], Type, IdProperty))
		{
			return;
		}

		const UObject* Asset = LoadObject<UObject>(nullptr, *Args[1]);
		if (!Asset)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.Convert: %ls could not be loaded", *Args[1]);
			return;
		}

		TMap<FString, FString> ConvertMap;
		if (!LoadConvertMap(ConvertMap))
		{
			return;
		}
		TSet<FName> UnregisteredTags;
		const FConvertOutcome Outcome = ConvertAsset(*Type, *IdProperty, *Asset, Args[2], false, UnregisteredTags);
		LogOutcome(Asset->GetPathName(), Outcome);
		if (Outcome.Status == EConvertStatus::Converted)
		{
			ConvertMap.Add(Asset->GetPathName(), Outcome.Id.ToString());
			SaveConvertMap(MoveTemp(ConvertMap));
		}
	}

	struct FConvertClass
	{
		const FArcJsonAssetTypeInfo* Type = nullptr;
		const FStructProperty* IdProperty = nullptr;
		FTopLevelAssetPath ClassPath;
	};

	bool ParseConvertAllArgs(const TArray<FString>& Args, bool& bOutDryRun, TArray<FConvertClass>& OutClasses, TArray<FName>& OutPackagePaths)
	{
		for (const FString& Arg : Args)
		{
			FString Extension;
			FString ClassPathText;
			if (Arg.Equals(TEXT("-DryRun"), ESearchCase::IgnoreCase))
			{
				bOutDryRun = true;
			}
			else if (Arg.Split(TEXT("="), &Extension, &ClassPathText))
			{
				FConvertClass& Entry = OutClasses.AddDefaulted_GetRef();
				if (!FindConvertibleType(Extension, Entry.Type, Entry.IdProperty))
				{
					return false;
				}
				const UClass* Class = LoadObject<UClass>(nullptr, *ClassPathText);
				if (!Class)
				{
					UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.ConvertAll: %ls is not a class", *ClassPathText);
					return false;
				}
				Entry.ClassPath = Class->GetClassPathName();
			}
			else if (Arg.StartsWith(TEXT("/")))
			{
				FString PackagePath = Arg;
				while (PackagePath.Len() > 1 && PackagePath.EndsWith(TEXT("/")))
				{
					PackagePath.LeftChopInline(1);
				}
				OutPackagePaths.Add(FName(PackagePath));
			}
			else
			{
				UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.ConvertAll: '%ls' is neither -DryRun, <extension>=<class path> nor a package path", *Arg);
				return false;
			}
		}
		if (OutClasses.IsEmpty() || OutPackagePaths.IsEmpty())
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.ConvertAll [-DryRun] <extension>=<class path> ... <package path> ...");
			return false;
		}
		return true;
	}

	void ConvertAllCommand(const TArray<FString>& Args)
	{
		bool bDryRun = false;
		TArray<FConvertClass> Classes;
		TArray<FName> PackagePaths;
		if (!ParseConvertAllArgs(Args, bDryRun, Classes, PackagePaths))
		{
			return;
		}

		const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		if (AssetRegistry.IsLoadingAssets())
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.ConvertAll: the asset registry is still scanning; nothing converted");
			return;
		}

		TMap<FSoftObjectPath, int32> ClassIndexByAsset;
		TSet<FSoftObjectPath> AmbiguousAssets;
		for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ++ClassIndex)
		{
			FARFilter Filter;
			Filter.ClassPaths.Add(Classes[ClassIndex].ClassPath);
			Filter.bRecursiveClasses = false;
			Filter.PackagePaths = PackagePaths;
			Filter.bRecursivePaths = true;
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssets(Filter, Assets);
			for (const FAssetData& AssetData : Assets)
			{
				const FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();
				const int32* ExistingIndex = ClassIndexByAsset.Find(AssetPath);
				if (!ExistingIndex)
				{
					ClassIndexByAsset.Add(AssetPath, ClassIndex);
				}
				else if (*ExistingIndex != ClassIndex)
				{
					AmbiguousAssets.Add(AssetPath);
				}
			}
		}
		TArray<FSoftObjectPath> AssetPaths;
		ClassIndexByAsset.GenerateKeyArray(AssetPaths);
		AssetPaths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) -> bool
		{
			return A.ToString() < B.ToString();
		});

		TMap<FString, FString> ConvertMap;
		if (!LoadConvertMap(ConvertMap))
		{
			return;
		}

		int32 ConvertedCount = 0;
		int32 WouldConvertCount = 0;
		int32 SkippedCount = 0;
		TArray<FString> FailedLines;
		TArray<FString> DifferenceLines;
		TArray<FString> DroppedLines;
		TSet<FName> UnregisteredTags;
		for (const FSoftObjectPath& AssetPath : AssetPaths)
		{
			const FString SourcePath = AssetPath.ToString();
			if (AmbiguousAssets.Contains(AssetPath))
			{
				FailedLines.Add(FString::Printf(TEXT("%s: matches more than one listed class"), *SourcePath));
				continue;
			}
			const UObject* Asset = AssetPath.TryLoad();
			if (!Asset)
			{
				FailedLines.Add(FString::Printf(TEXT("%s: could not be loaded"), *SourcePath));
				continue;
			}
			const FConvertClass& Entry = Classes[ClassIndexByAsset.FindChecked(AssetPath)];
			const FString Directory = FPackageName::GetLongPackagePath(Asset->GetPackage()->GetName());
			const FConvertOutcome Outcome = ConvertAsset(*Entry.Type, *Entry.IdProperty, *Asset, Directory, bDryRun, UnregisteredTags);
			LogOutcome(Asset->GetPathName(), Outcome);
			for (const FString& Dropped : Outcome.DroppedElements)
			{
				DroppedLines.Add(FString::Printf(TEXT("%s: %s"), *SourcePath, *Dropped));
			}
			switch (Outcome.Status)
			{
			case EConvertStatus::Converted:
				++ConvertedCount;
				ConvertMap.Add(Asset->GetPathName(), Outcome.Id.ToString());
				if (Outcome.DifferenceCount > 0)
				{
					DifferenceLines.Add(FString::Printf(TEXT("%s: %d differences"), *Outcome.Filename, Outcome.DifferenceCount));
				}
				break;
			case EConvertStatus::WouldConvert:
				++WouldConvertCount;
				break;
			case EConvertStatus::SkippedExisting:
				++SkippedCount;
				break;
			case EConvertStatus::Failed:
				FailedLines.Add(FString::Printf(TEXT("%s: %s"), *SourcePath, *Outcome.Reason));
				break;
			}
		}

		if (ConvertedCount > 0)
		{
			SaveConvertMap(MoveTemp(ConvertMap));
		}

		if (bDryRun)
		{
			UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.ConvertAll (dry run): %d assets, %d would be converted, %d skipped (record exists), %d failed",
				AssetPaths.Num(), WouldConvertCount, SkippedCount, FailedLines.Num());
		}
		else
		{
			UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.ConvertAll: %d assets, %d converted, %d skipped (record exists), %d failed",
				AssetPaths.Num(), ConvertedCount, SkippedCount, FailedLines.Num());
		}
		for (const FString& Line : DifferenceLines)
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.ConvertAll: differences: %ls", *Line);
		}
		for (const FString& Line : DroppedLines)
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.ConvertAll: dropped empty instanced struct: %ls", *Line);
		}
		for (const FString& Line : FailedLines)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets.ConvertAll: failed: %ls", *Line);
		}
		if (!UnregisteredTags.IsEmpty())
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.ConvertAll: %d unregistered gameplay tags:", UnregisteredTags.Num());
			for (const FString& Tag : SortTagNames(UnregisteredTags))
			{
				UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.ConvertAll:   %ls", *Tag);
			}
		}
	}

	FAutoConsoleCommand ConvertConsoleCommand(
		TEXT("ArcJsonAssets.Convert"),
		TEXT("ArcJsonAssets.Convert <extension> <asset object path> <output package path or directory>: writes the asset's mirrored properties as a new JSON asset file, checks the file against the asset and adds it to Saved/ArcJsonAssets/ConvertMap.json."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ConvertCommand));

	FAutoConsoleCommand ConvertAllConsoleCommand(
		TEXT("ArcJsonAssets.ConvertAll"),
		TEXT("ArcJsonAssets.ConvertAll [-DryRun] <extension>=<class path> ... <package path> ...: converts every asset of exactly the classes (no subclasses) under the package paths into a record next to it, skips existing records, lists failures, dropped empty instanced struct elements and unregistered gameplay tags, and merges Saved/ArcJsonAssets/ConvertMap.json (source object path -> record id). -DryRun writes nothing."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ConvertAllCommand));
}
