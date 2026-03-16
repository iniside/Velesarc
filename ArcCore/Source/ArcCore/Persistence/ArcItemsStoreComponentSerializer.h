// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArcPersistenceSerializer.h"
#include "ArcPlayerProviderDescriptor.h"

class UArcItemsStoreComponent;
class FArcSaveArchive;
class FArcLoadArchive;

/**
 * Registered persistence serializer for UArcItemsStoreComponent.
 * Delegates to FArcItemStoreSerializer for item data.
 * Load clears store then adds deserialized items.
 */
struct ARCCORE_API FArcItemsStoreComponentSerializer
{
	using SourceType = UArcItemsStoreComponent;
	static constexpr uint32 Version = 1;

	static void Save(const UArcItemsStoreComponent& Source, FArcSaveArchive& Ar);
	static void Load(UArcItemsStoreComponent& Target, FArcLoadArchive& Ar);
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcItemsStoreComponentSerializer, ARCCORE_API);
UE_ARC_DECLARE_PLAYER_PROVIDER(UArcItemsStoreComponent, ARCCORE_API);
