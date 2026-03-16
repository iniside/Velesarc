// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "Equipment/ArcAttachmentHandler.h"
#include "Equipment/ArcSocketArray.h"
#include "Items/ArcItemDefinition.h"

#include "ArcItemAttachmentIntegrationTests.generated.h"

// ===================================================================
// Subclassed store for TSubclassOf matching in tests
// ===================================================================

UCLASS()
class UArcItemAttachmentTestStore : public UArcItemsStoreComponent
{
	GENERATED_BODY()
};

// ===================================================================
// Simple test handler that creates a USceneComponent attachment.
// Exercises the full handler pipeline without needing real mesh assets.
// ===================================================================

USTRUCT()
struct FArcTestAttachmentHandler : public FArcAttachmentHandlerCommon
{
	GENERATED_BODY()

	virtual bool HandleItemAddedToSlot(
		UArcItemAttachmentComponent* InAttachmentComponent,
		const FArcItemData* InItem,
		const FArcItemData* InOwnerItem) const override;

	virtual void HandleItemAttach(
		UArcItemAttachmentComponent* InAttachmentComponent,
		const FArcItemId InItemId,
		const FArcItemId InOwnerItem) const override;

	virtual void HandleItemDetach(
		UArcItemAttachmentComponent* InAttachmentComponent,
		const FArcItemId InItemId,
		const FArcItemId InOwnerItem) const override;

	/**
	 * Creates an FInstancedStruct containing a configured FArcTestAttachmentHandler.
	 * @param InComponentTag - Tag that must match a USceneComponent on the owning actor.
	 */
	static FInstancedStruct Make(FName InComponentTag);
};

// ===================================================================
// Test actor with items store + attachment component + root scene
// ===================================================================

UCLASS()
class AArcItemAttachmentTestActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<USceneComponent> AttachRoot;

	UPROPERTY()
	TObjectPtr<UArcItemAttachmentTestStore> ItemsStore;

	UPROPERTY()
	TObjectPtr<UArcItemAttachmentComponent> AttachmentComp;

	AArcItemAttachmentTestActor();
};
