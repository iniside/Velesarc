// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "ArcNamedPrimaryAssetId.h"

#include "ArcMassItemTestsSettings.generated.h"

UCLASS(config = ArcTests, defaultconfig)
class ARCMASSITEMSTEST_API UArcMassItemTestsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Base item with no fragments (no stacking, no tags, no sockets).
	// Used for: AddItem, RemoveItem, AddItemToSlot, RemoveItemFromSlot, ModifyStacks,
	// AttachItem, DetachItem, AddDynamicTag, RemoveDynamicTag, SetLevel, MoveItem,
	// InitializeItemData, FindItemByDefinition, Contains, CountItemsByDefinition,
	// ChangeItemSlot, IsOnAnySlot, DestroyItem, GetAttachedItems, FindAttachedItemOnSlot.
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId SimpleBaseItem;

	// Item with FArcItemStackMethod_StackByType (or FArcMassItemStackMethod_StackByType) configured.
	// Should have MaxStacks high enough to accept multiple additions (e.g. 10+).
	// Used for: StackByType stacking tests.
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStacks;

	// Item with FArcItemStackMethod_CanNotStackUnique configured.
	// Only one instance of this definition allowed in the store.
	// Used for: CanNotStackUnique stacking tests.
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithStacksCannotStack;

	// Item with FArcItemFragment_Tags fragment configured.
	// Must have at least one gameplay tag set in ItemTags (e.g. "Test.Item.Tag").
	// Used for: FindItemByDefinitionTags, FindItemsByDefinitionTags.
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithDefinitionTags;

	// Item with FArcItemFragment_SocketSlots fragment configured.
	// Must have at least one FArcSocketSlot with a valid DefaultSocketItemDefinition
	// pointing to another item definition (can be SimpleBaseItem, ItemWithStacks, etc.).
	// The SlotId tag should be set (e.g. "Test.Item.Socket.Barrel").
	// Used for: socket attachment auto-creation in AddItem.
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithSocketDefaults;

	// Item with FArcItemFragment_GrantedMassAbilities configured.
	// Used for QuickBar input-binding tests.
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemWithGrantedMassAbility;

	// Item with FArcItemFragment_StaticMeshAttachment configured.
	// Must have:
	//   - StaticMesh = a valid UStaticMesh asset (engine cube or a small project-side test mesh)
	//   - SocketName = "TestMesh" (matches the test actor's root component tag — see
	//     AArcMassAttachmentTestActor in ArcMassItemAttachmentIntegrationTests.cpp)
	// Used for: ArcMassItems.Attachments.Integration.RealHandler tests that exercise the
	// real FArcMassAttachmentHandler_StaticMesh end-to-end (asset load + component spawn
	// + actor attach + register lifecycle).
	UPROPERTY(EditDefaultsOnly, Category = "Mass Item Tests", Config,
	    meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId StaticMeshAttachmentItem;

	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	/** Gets the category for the settings */
	virtual FName GetCategoryName() const override { return TEXT("Arc Tests"); }
	/** The unique name for your section of settings */
	virtual FName GetSectionName() const override { return TEXT("Mass Item Tests"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return FText::FromString("Mass Item Tests"); }
	virtual FText GetSectionDescription() const override { return FText::FromString("ArcMassItem definition references for CQTest tests. Configure all item definitions below."); }
#endif
};
