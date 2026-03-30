// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassElement.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"
#include "Serialization/ArcSerializerRegistry.h"
#include "Serialization/ArcReflectionSerializer.h"

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

	const bool bForceAllProperties = Config.AllowedFragments.Num() > 0;

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
			SaveFragment(FragmentType, FragmentData.GetMemory(), Ar, bForceAllProperties);
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
	FArcSaveArchive& Ar,
	bool bForceAllProperties)
{
	// Write force-all flag so LoadFragment knows which mode to use
	if (bForceAllProperties)
	{
		Ar.WriteProperty(FName("_forceAll"), true);
	}

	const FArcPersistenceSerializerInfo* CustomInfo =
		FArcSerializerRegistry::Get().Find(FragmentType);

	if (CustomInfo)
	{
		// Custom serializer — use as-is (it controls its own property selection)
		Ar.WriteProperty(FName("_version"), CustomInfo->Version);
		CustomInfo->SaveFunc(Data, Ar);
	}
	else if (bForceAllProperties)
	{
		// Force-all reflection: hash ALL properties for version, serialize ALL
		uint32 Version = FArcReflectionSerializer::ComputeSchemaVersionAll(FragmentType);
		Ar.WriteProperty(FName("_version"), Version);
		FArcReflectionSerializer::SaveAll(FragmentType, Data, Ar);
	}
	else
	{
		// Default reflection: SaveGame properties only
		const FArcPersistenceSerializerInfo* FallbackInfo =
			FArcSerializerRegistry::Get().FindOrDefault(FragmentType);
		Ar.WriteProperty(FName("_version"), FallbackInfo->Version);
		FallbackInfo->SaveFunc(Data, Ar);
	}
}

void FArcMassFragmentSerializer::LoadFragment(
	const UScriptStruct* FragmentType,
	void* Data,
	FArcLoadArchive& Ar)
{
	// Check if this fragment was saved with force-all mode
	bool bForceAll = false;
	Ar.ReadProperty(FName("_forceAll"), bForceAll);

	// Read the per-fragment version stored in the fragment's array element scope
	uint32 SavedVersion = 0;
	if (!Ar.ReadProperty(FName("_version"), SavedVersion))
	{
		UE_LOG(LogTemp, Log,
			TEXT("ArcMassPersistence: No version found for %s — discarding"),
			*FragmentType->GetName());
		return;
	}

	const FArcPersistenceSerializerInfo* CustomInfo =
		FArcSerializerRegistry::Get().Find(FragmentType);

	if (CustomInfo)
	{
		// Custom serializer — version check against custom version
		if (SavedVersion != CustomInfo->Version)
		{
			UE_LOG(LogTemp, Log,
				TEXT("ArcMassPersistence: Version mismatch for %s (saved=%u, current=%u) — discarding"),
				*FragmentType->GetName(), SavedVersion, CustomInfo->Version);
			return;
		}
		CustomInfo->LoadFunc(Data, Ar);
	}
	else if (bForceAll)
	{
		// Force-all reflection: compute version from ALL properties
		uint32 CurrentVersion = FArcReflectionSerializer::ComputeSchemaVersionAll(FragmentType);
		if (SavedVersion != CurrentVersion)
		{
			UE_LOG(LogTemp, Log,
				TEXT("ArcMassPersistence: Version mismatch for %s (saved=%u, current=%u) — discarding"),
				*FragmentType->GetName(), SavedVersion, CurrentVersion);
			return;
		}
		FArcReflectionSerializer::LoadAll(FragmentType, Data, Ar);
	}
	else
	{
		// Default reflection: SaveGame properties only
		const FArcPersistenceSerializerInfo* FallbackInfo =
			FArcSerializerRegistry::Get().FindOrDefault(FragmentType);
		if (SavedVersion != FallbackInfo->Version)
		{
			UE_LOG(LogTemp, Log,
				TEXT("ArcMassPersistence: Version mismatch for %s (saved=%u, current=%u) — discarding"),
				*FragmentType->GetName(), SavedVersion, FallbackInfo->Version);
			return;
		}
		FallbackInfo->LoadFunc(Data, Ar);
	}
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
