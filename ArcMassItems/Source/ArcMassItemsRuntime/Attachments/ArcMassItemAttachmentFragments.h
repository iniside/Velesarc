// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassItemAttachmentFragments.generated.h"

class UArcMassItemAttachmentConfig;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	FArcItemId OwnerId;

	UPROPERTY()
	FName SocketName = NAME_None;

	UPROPERTY()
	FName SocketComponentTag = NAME_None;

	UPROPERTY()
	FGameplayTag SlotId;

	UPROPERTY()
	FGameplayTag OwnerSlotId;

	UPROPERTY()
	FTransform RelativeTransform;

	UPROPERTY()
	FPrimaryAssetId VisualItemDefinition;

	UPROPERTY()
	FPrimaryAssetId OldVisualItemDefinition;

	UPROPERTY()
	FPrimaryAssetId ItemDefinition;

	UPROPERTY()
	FPrimaryAssetId OwnerItemDefinition;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentObjectArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> Objects;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentNameArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> Names;
};

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Attachments")
	TObjectPtr<UArcMassItemAttachmentConfig> Config;

	// Store types whose events this attachment template should react to.
	// The trait fills this from the trait's InterestedStoreTypes (default: FArcMassItemStoreFragment).
	UPROPERTY(EditAnywhere, Category = "Attachments", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TArray<TObjectPtr<UScriptStruct>> InterestedStoreTypes;
};

template<>
struct TMassFragmentTraits<FArcMassItemAttachmentConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentStateFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassItemAttachment> ActiveAttachments;

	UPROPERTY()
	TMap<FArcItemId, FArcMassItemAttachmentObjectArray> ObjectsAttachedFromItem;

	UPROPERTY()
	TMap<FGameplayTag, FArcMassItemAttachmentNameArray> TakenSockets;
};

template<>
struct TMassFragmentTraits<FArcMassItemAttachmentStateFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
