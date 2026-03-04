// Copyright Lukasz Baran. All Rights Reserved.

#include "Persistence/ArcItemsStoreComponentSerializer.h"

#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemData.h"
#include "Persistence/ArcItemStoreSerializer.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcItemsStoreComponentSerializer, Log, All);

// ── Serializer registration ─────────────────────────────────────────────────

UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcItemsStoreComponentSerializer, "ArcItemsStoreComponent");

// ── Provider registration (leaf under PlayerState) ──────────────────────────

UE_ARC_IMPLEMENT_PLAYER_PROVIDER(
	UArcItemsStoreComponent,
	UArcItemsStoreComponent,
	APlayerState::StaticClass(),
	[](UObject* Parent) -> UObject* {
		AActor* Actor = Cast<AActor>(Parent);
		return Actor ? Actor->FindComponentByClass<UArcItemsStoreComponent>() : nullptr;
	},
	true
);

// ── Save ────────────────────────────────────────────────────────────────────

void FArcItemsStoreComponentSerializer::Save(
	const UArcItemsStoreComponent& Source, FArcSaveArchive& Ar)
{
	TArray<FArcItemCopyContainerHelper> Items = Source.GetAllInternalItems();
	FArcItemStoreSerializer::Save(Items, Ar);
}

// ── Load (clear-then-add) ───────────────────────────────────────────────────

void FArcItemsStoreComponentSerializer::Load(
	UArcItemsStoreComponent& Target, FArcLoadArchive& Ar)
{
	TArray<FArcItemCopyContainerHelper> Items;
	FArcItemStoreSerializer::Load(Items, Ar);

	// Clear existing root items. DestroyItem handles recursive teardown of attachments.
	TArray<const FArcItemData*> ExistingItems = Target.GetItems();
	for (int32 i = ExistingItems.Num() - 1; i >= 0; --i)
	{
		const FArcItemData* Item = ExistingItems[i];
		if (Item && !Item->GetOwnerId().IsValid())
		{
			Target.DestroyItem(Item->GetItemId());
		}
	}

	// Add loaded items
	for (FArcItemCopyContainerHelper& Helper : Items)
	{
		Helper.AddItems(&Target);
	}

	UE_LOG(LogArcItemsStoreComponentSerializer, Log,
		TEXT("Loaded %d item groups into '%s'"),
		Items.Num(), *Target.GetClass()->GetName());
}
