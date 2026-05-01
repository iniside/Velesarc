// Copyright Lukasz Baran. All Rights Reserved.

#include "PCG/PCGMassFragmentOverrideHelpers.h"

#include "PCGContext.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataDomain.h"
#include "MassEntityTypes.h"
#include "UObject/UnrealType.h"

namespace PCGMassFragmentOverrides
{

TMap<FName, TArray<FName>> DiscoverFragmentAttributes(const UPCGMetadata* Metadata)
{
	TMap<FName, TArray<FName>> Result;
	if (!Metadata)
	{
		return Result;
	}

	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;
	Metadata->GetAttributes(AttributeNames, AttributeTypes);

	for (const FName& AttrName : AttributeNames)
	{
		FString AttrString = AttrName.ToString();
		int32 DotIndex = INDEX_NONE;
		if (!AttrString.FindChar(TEXT('.'), DotIndex))
		{
			continue;
		}

		FName FragmentName = FName(*AttrString.Left(DotIndex));
		FName PropertyName = FName(*AttrString.Mid(DotIndex + 1));

		if (FragmentName.IsNone() || PropertyName.IsNone())
		{
			continue;
		}

		Result.FindOrAdd(FragmentName).AddUnique(PropertyName);
	}

	return Result;
}

UScriptStruct* ResolveFragmentStruct(FName FragmentName, FPCGContext* Context)
{
	FString StructName = FragmentName.ToString();

	// Try exact name first (e.g. "FMyFragment")
	UScriptStruct* FoundStruct = FindFirstObject<UScriptStruct>(*StructName, EFindFirstObjectOptions::NativeFirst);
	if (FoundStruct)
	{
		return FoundStruct;
	}

	if (Context)
	{
		PCGLog::LogWarningOnGraph(
			FText::Format(
				NSLOCTEXT("PCGMassFragmentOverrides", "UnknownFragment", "Unknown fragment struct '{0}'. Overrides for this fragment will be skipped."),
				FText::FromName(FragmentName)),
			Context);
	}

	return nullptr;
}

FArcPlacedEntityFragmentOverrides AssembleOverridesForPoint(
	const UPCGBasePointData* PointData,
	int32 PointIndex,
	const TMap<FName, TArray<FName>>& FragmentAttributeMap,
	const TMap<FName, UScriptStruct*>& ResolvedStructs,
	FPCGContext* Context)
{
	FArcPlacedEntityFragmentOverrides Overrides;

	for (const TPair<FName, TArray<FName>>& FragmentPair : FragmentAttributeMap)
	{
		UScriptStruct* const* FoundStruct = ResolvedStructs.Find(FragmentPair.Key);
		if (!FoundStruct || !*FoundStruct)
		{
			continue;
		}

		UScriptStruct* FragmentStruct = *FoundStruct;
		FInstancedStruct FragmentInstance;
		FragmentInstance.InitializeAs(FragmentStruct);
		void* StructMemory = FragmentInstance.GetMutableMemory();

		bool bAnyPropertyWritten = false;

		for (const FName& PropertyName : FragmentPair.Value)
		{
			FProperty* Property = FragmentStruct->FindPropertyByName(PropertyName);
			if (!Property)
			{
				if (Context)
				{
					PCGLog::LogWarningOnGraph(
						FText::Format(
							NSLOCTEXT("PCGMassFragmentOverrides", "UnknownProperty", "Unknown property '{0}' on fragment '{1}'. Skipping."),
							FText::FromName(PropertyName),
							FText::FromName(FragmentPair.Key)),
						Context);
				}
				continue;
			}

			// Build full attribute name: "FragmentName.PropertyName"
			FName FullAttrName = FName(*FString::Printf(TEXT("%s.%s"),
				*FragmentPair.Key.ToString(), *PropertyName.ToString()));

			if (WriteAttributeToProperty(PointData, PointIndex, FullAttrName, Property, StructMemory, Context))
			{
				bAnyPropertyWritten = true;
			}
		}

		if (bAnyPropertyWritten)
		{
			Overrides.FragmentValues.Add(MoveTemp(FragmentInstance));
		}
	}

	return Overrides;
}

bool WriteAttributeToProperty(
	const UPCGBasePointData* PointData,
	int32 PointIndex,
	FName AttributeName,
	FProperty* TargetProperty,
	void* StructMemory,
	FPCGContext* Context)
{
	const UPCGMetadata* Metadata = PointData->ConstMetadata();
	if (!Metadata)
	{
		return false;
	}

	const int32 MetadataEntry = PointData->GetMetadataEntry(PointIndex);
	if (MetadataEntry == INDEX_NONE)
	{
		return false;
	}

	void* PropertyAddr = TargetProperty->ContainerPtrToValuePtr<void>(StructMemory);

	// Float/Double
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<double>* Attr = Metadata->GetConstTypedAttribute<double>(AttributeName);
		if (!Attr)
		{
			const FPCGMetadataAttribute<float>* AttrF = Metadata->GetConstTypedAttribute<float>(AttributeName);
			if (AttrF)
			{
				FloatProp->SetPropertyValue(PropertyAddr, AttrF->GetValue(MetadataEntry));
				return true;
			}
			return false;
		}
		FloatProp->SetPropertyValue(PropertyAddr, static_cast<float>(Attr->GetValue(MetadataEntry)));
		return true;
	}

	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<double>* Attr = Metadata->GetConstTypedAttribute<double>(AttributeName);
		if (!Attr)
		{
			const FPCGMetadataAttribute<float>* AttrF = Metadata->GetConstTypedAttribute<float>(AttributeName);
			if (AttrF)
			{
				DoubleProp->SetPropertyValue(PropertyAddr, static_cast<double>(AttrF->GetValue(MetadataEntry)));
				return true;
			}
			return false;
		}
		DoubleProp->SetPropertyValue(PropertyAddr, Attr->GetValue(MetadataEntry));
		return true;
	}

	// Int32
	if (FIntProperty* IntProp = CastField<FIntProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<int32>* Attr = Metadata->GetConstTypedAttribute<int32>(AttributeName);
		if (Attr)
		{
			IntProp->SetPropertyValue(PropertyAddr, Attr->GetValue(MetadataEntry));
			return true;
		}
		return false;
	}

	// Int64
	if (FInt64Property* Int64Prop = CastField<FInt64Property>(TargetProperty))
	{
		const FPCGMetadataAttribute<int64>* Attr = Metadata->GetConstTypedAttribute<int64>(AttributeName);
		if (Attr)
		{
			Int64Prop->SetPropertyValue(PropertyAddr, Attr->GetValue(MetadataEntry));
			return true;
		}
		return false;
	}

	// Bool
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<bool>* Attr = Metadata->GetConstTypedAttribute<bool>(AttributeName);
		if (Attr)
		{
			BoolProp->SetPropertyValue(PropertyAddr, Attr->GetValue(MetadataEntry));
			return true;
		}
		return false;
	}

	// FString
	if (FStrProperty* StrProp = CastField<FStrProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<FString>* Attr = Metadata->GetConstTypedAttribute<FString>(AttributeName);
		if (Attr)
		{
			StrProp->SetPropertyValue(PropertyAddr, Attr->GetValue(MetadataEntry));
			return true;
		}
		return false;
	}

	// FName
	if (FNameProperty* NameProp = CastField<FNameProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<FName>* Attr = Metadata->GetConstTypedAttribute<FName>(AttributeName);
		if (Attr)
		{
			NameProp->SetPropertyValue(PropertyAddr, Attr->GetValue(MetadataEntry));
			return true;
		}
		// Try FString fallback
		const FPCGMetadataAttribute<FString>* StrAttr = Metadata->GetConstTypedAttribute<FString>(AttributeName);
		if (StrAttr)
		{
			NameProp->SetPropertyValue(PropertyAddr, FName(*StrAttr->GetValue(MetadataEntry)));
			return true;
		}
		return false;
	}

	// FStructProperty — FVector, FRotator, FTransform, FSoftObjectPath
	if (FStructProperty* StructProp = CastField<FStructProperty>(TargetProperty))
	{
		UScriptStruct* InnerStruct = StructProp->Struct;

		if (InnerStruct == TBaseStructure<FVector>::Get())
		{
			const FPCGMetadataAttribute<FVector>* Attr = Metadata->GetConstTypedAttribute<FVector>(AttributeName);
			if (Attr)
			{
				*static_cast<FVector*>(PropertyAddr) = Attr->GetValue(MetadataEntry);
				return true;
			}
		}
		else if (InnerStruct == TBaseStructure<FRotator>::Get())
		{
			const FPCGMetadataAttribute<FRotator>* Attr = Metadata->GetConstTypedAttribute<FRotator>(AttributeName);
			if (Attr)
			{
				*static_cast<FRotator*>(PropertyAddr) = Attr->GetValue(MetadataEntry);
				return true;
			}
		}
		else if (InnerStruct == TBaseStructure<FTransform>::Get())
		{
			const FPCGMetadataAttribute<FTransform>* Attr = Metadata->GetConstTypedAttribute<FTransform>(AttributeName);
			if (Attr)
			{
				*static_cast<FTransform*>(PropertyAddr) = Attr->GetValue(MetadataEntry);
				return true;
			}
		}
		else if (InnerStruct == TBaseStructure<FSoftObjectPath>::Get())
		{
			const FPCGMetadataAttribute<FSoftObjectPath>* Attr = Metadata->GetConstTypedAttribute<FSoftObjectPath>(AttributeName);
			if (Attr)
			{
				*static_cast<FSoftObjectPath*>(PropertyAddr) = Attr->GetValue(MetadataEntry);
				return true;
			}
		}

		return false;
	}

	// FSoftObjectProperty
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(TargetProperty))
	{
		const FPCGMetadataAttribute<FSoftObjectPath>* Attr = Metadata->GetConstTypedAttribute<FSoftObjectPath>(AttributeName);
		if (Attr)
		{
			FSoftObjectPtr SoftPtr(Attr->GetValue(MetadataEntry));
			SoftObjProp->SetPropertyValue(PropertyAddr, SoftPtr);
			return true;
		}
		return false;
	}

	return false;
}

} // namespace PCGMassFragmentOverrides
