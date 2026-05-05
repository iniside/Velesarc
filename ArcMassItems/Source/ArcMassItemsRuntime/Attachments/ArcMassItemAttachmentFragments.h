// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemDefinition.h"
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
	TObjectPtr<const UArcItemDefinition> VisualItemDefinition = nullptr;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> OldVisualItemDefinition = nullptr;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> ItemDefinition = nullptr;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> OwnerItemDefinition = nullptr;
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
};

template<>
struct TMassFragmentTraits<FArcMassItemAttachmentConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentStateFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassItemAttachment> ActiveAttachments;

	// Rekeyed from TObjectPtr<const UArcItemDefinition> to FArcItemId.
	// Two items sharing one definition no longer collide.
	UPROPERTY()
	TMap<FArcItemId, FArcMassItemAttachmentObjectArray> ObjectsAttachedFromItem;

	// Phase 1 detach moves snapshot here; Phase 2 reads it next frame.
	// Carries data Phase 2 needs (ItemDefinition for AnimLayer unlink, etc.)
	// because Phase 1 has already removed the entry from ActiveAttachments.
	UPROPERTY()
	TMap<FArcItemId, FArcMassItemAttachment> PendingVisualDetaches;

	UPROPERTY()
	TMap<FGameplayTag, FArcMassItemAttachmentNameArray> TakenSockets;
};

template<>
struct TMassFragmentTraits<FArcMassItemAttachmentStateFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
