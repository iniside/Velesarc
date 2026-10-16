// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttribute.h"
#include "Serialization/ArcPersistenceSerializer.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"
#include "UObject/SoftObjectPath.h"

/**
 * Custom persistence serializer for FArcAttribute.
 *
 * The generic reflection serializer recurses into nested structs through its
 * CPF_SaveGame-filtered path even under "force all" (whitelisted) fragments, so
 * FArcAttribute's float members would otherwise serialize to an empty "{}".
 * The registry is consulted before generic recursion in both save and load,
 * so registering here intercepts FArcAttribute everywhere it appears nested.
 */
struct FArcAttributePersistenceSerializer
{
	using SourceType = FArcAttribute;
	static constexpr uint32 Version = 1;

	static void Save(const FArcAttribute& Source, FArcSaveArchive& Ar)
	{
		Ar.WriteProperty(FName(TEXT("BaseValue")), Source.BaseValue);
		Ar.WriteProperty(FName(TEXT("CurrentValue")), Source.CurrentValue);
	}

	static void Load(FArcAttribute& Target, FArcLoadArchive& Ar)
	{
		Ar.ReadProperty(FName(TEXT("BaseValue")), Target.BaseValue);
		Ar.ReadProperty(FName(TEXT("CurrentValue")), Target.CurrentValue);
	}
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcAttributePersistenceSerializer, )
UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcAttributePersistenceSerializer, "ArcAttribute")

/** Writes the fragment struct as a path so the reflection serializer's hard-object warning never fires. */
struct FArcAttributeRefPersistenceSerializer
{
	using SourceType = FArcAttributeRef;
	static constexpr uint32 Version = 1;

	static void Save(const FArcAttributeRef& Source, FArcSaveArchive& Ar)
	{
		Ar.WriteProperty(FName(TEXT("FragmentType")), Source.FragmentType != nullptr ? Source.FragmentType->GetPathName() : FString());
		Ar.WriteProperty(FName(TEXT("PropertyName")), Source.PropertyName);
	}

	static void Load(FArcAttributeRef& Target, FArcLoadArchive& Ar)
	{
		FString FragmentPath;
		FName PropertyName = NAME_None;
		Ar.ReadProperty(FName(TEXT("FragmentType")), FragmentPath);
		Ar.ReadProperty(FName(TEXT("PropertyName")), PropertyName);

		Target = FArcAttributeRef();
		Target.FragmentType = FragmentPath.IsEmpty() ? nullptr : Cast<UScriptStruct>(FSoftObjectPath(FragmentPath).ResolveObject());
		Target.PropertyName = PropertyName;
	}
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcAttributeRefPersistenceSerializer, )
UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcAttributeRefPersistenceSerializer, "ArcAttributeRef")
