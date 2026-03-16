// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassElement.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"
#include "Serialization/ArcSerializerRegistry.h"

void FArcMassFragmentSerializer::SaveEntityFragments(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcMassPersistenceConfigFragment& Config,
	FArcSaveArchive& Ar,
	UWorld* World)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	// Get fragment types from archetype composition
	const FMassArchetypeHandle Archetype = EntityManager.GetArchetypeForEntity(Entity);
	TArray<const UScriptStruct*> FragmentTypes;
	Archetype.GetCompositionBitSetChecked().Get<FMassFragmentBitSet>().ExportTypes(FragmentTypes);

	// Filter to serializable fragments
	TArray<const UScriptStruct*> ToSerialize;
	for (const UScriptStruct* Type : FragmentTypes)
	{
		if (ShouldSerializeFragment(Type, Config))
		{
			ToSerialize.Add(Type);
		}
	}

	FMassEntityView EntityView(EntityManager, Entity);

	Ar.BeginArray(FName("fragments"), ToSerialize.Num());

	for (int32 i = 0; i < ToSerialize.Num(); ++i)
	{
		const UScriptStruct* FragmentType = ToSerialize[i];
		FStructView FragmentData = EntityView.GetFragmentDataStruct(FragmentType);

		if (FragmentData.IsValid())
		{
			Ar.BeginArrayElement(i);
			Ar.WriteProperty(FName("_type"), FString(FragmentType->GetPathName()));
			const FArcPersistenceSerializerInfo* HookInfo =
				FArcSerializerRegistry::Get().Find(FragmentType);
			if (HookInfo && HookInfo->PreSaveFunc && World)
			{
				HookInfo->PreSaveFunc(FragmentData.GetMemory(), *World);
			}
			SaveFragment(FragmentType, FragmentData.GetMemory(), Ar);
			Ar.EndArrayElement();
		}
	}

	Ar.EndArray();
}

void FArcMassFragmentSerializer::LoadEntityFragments(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcLoadArchive& Ar,
	UWorld* World)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	int32 FragmentCount = 0;
	if (!Ar.BeginArray(FName("fragments"), FragmentCount))
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Entity);

	for (int32 i = 0; i < FragmentCount; ++i)
	{
		if (!Ar.BeginArrayElement(i))
		{
			continue;
		}

		FString FragmentTypePath;
		if (Ar.ReadProperty(FName("_type"), FragmentTypePath))
		{
			const UScriptStruct* FragmentType =
				FindObject<UScriptStruct>(nullptr, *FragmentTypePath);

			if (FragmentType)
			{
				FStructView FragmentData = EntityView.GetFragmentDataStruct(FragmentType);
				if (FragmentData.IsValid())
				{
					LoadFragment(FragmentType, FragmentData.GetMemory(), Ar);
					const FArcPersistenceSerializerInfo* HookInfo =
						FArcSerializerRegistry::Get().Find(FragmentType);
					if (HookInfo && HookInfo->PostLoadFunc && World)
					{
						HookInfo->PostLoadFunc(FragmentData.GetMemory(), *World);
					}
				}
			}
		}

		Ar.EndArrayElement();
	}

	Ar.EndArray();
}

void FArcMassFragmentSerializer::SaveFragment(
	const UScriptStruct* FragmentType,
	const void* Data,
	FArcSaveArchive& Ar)
{
	const FArcPersistenceSerializerInfo* Info =
		FArcSerializerRegistry::Get().FindOrDefault(FragmentType);

	Ar.WriteProperty(FName("_version"), Info->Version);
	Info->SaveFunc(Data, Ar);
}

void FArcMassFragmentSerializer::LoadFragment(
	const UScriptStruct* FragmentType,
	void* Data,
	FArcLoadArchive& Ar)
{
	// Read the per-fragment version stored in the fragment's array element scope
	uint32 SavedVersion = 0;
	if (!Ar.ReadProperty(FName("_version"), SavedVersion))
	{
		UE_LOG(LogTemp, Log,
			TEXT("ArcMassPersistence: No version found for %s — discarding"),
			*FragmentType->GetName());
		return;
	}

	const FArcPersistenceSerializerInfo* Info =
		FArcSerializerRegistry::Get().FindOrDefault(FragmentType);

	if (SavedVersion != Info->Version)
	{
		UE_LOG(LogTemp, Log,
			TEXT("ArcMassPersistence: Version mismatch for %s — discarding"),
			*FragmentType->GetName());
		return;
	}

	Info->LoadFunc(Data, Ar);
}

bool FArcMassFragmentSerializer::ShouldSerializeFragment(
	const UScriptStruct* FragmentType,
	const FArcMassPersistenceConfigFragment& Config)
{
	if (Config.DisallowedFragments.Contains(const_cast<UScriptStruct*>(FragmentType)))
	{
		return false;
	}

	if (Config.AllowedFragments.Num() > 0)
	{
		return Config.AllowedFragments.Contains(const_cast<UScriptStruct*>(FragmentType));
	}

	// Default: check if fragment has any SaveGame properties
	for (TFieldIterator<FProperty> It(FragmentType); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_SaveGame))
		{
			return true;
		}
	}

	return false;
}
