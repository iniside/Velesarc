// Copyright Lukasz Baran. All Rights Reserved.

#include "Persistence/ArcItemSpecSerializer.h"
#include "Persistence/ArcItemStoreSerializer.h"
#include "Items/ArcItemSpec.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"

UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcItemSpecSerializer, "ArcItemSpec");

void FArcItemSpecSerializer::Save(const FArcItemSpec& Source, FArcSaveArchive& Ar)
{
	FGameplayTag EmptySlot;
	FArcItemStoreSerializer::SaveItemSpec(Source, EmptySlot, Ar);
}

void FArcItemSpecSerializer::Load(FArcItemSpec& Target, FArcLoadArchive& Ar)
{
	FGameplayTag DiscardedSlot;
	FArcItemStoreSerializer::LoadItemSpec(Target, DiscardedSlot, Ar);
}
