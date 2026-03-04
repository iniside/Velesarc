// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArcPersistenceSerializer.h"

class UAttributeSet;
class UAbilitySystemComponent;
class FArcSaveArchive;
class FArcLoadArchive;

/**
 * Generic persistence serializer for UAttributeSet subclasses.
 * Iterates FGameplayAttributeData properties via reflection.
 * Only serializes properties tagged with CPF_SaveGame.
 * Load applies values via ASC->SetNumericAttributeBase().
 */
struct ARCCORE_API FArcAttributeSetSerializer
{
	using SourceType = UAttributeSet;
	static constexpr uint32 Version = 1;

	static void Save(const UAttributeSet& Source, FArcSaveArchive& Ar);
	static void Load(UAttributeSet& Target, FArcLoadArchive& Ar);
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcAttributeSetSerializer, ARCCORE_API);
