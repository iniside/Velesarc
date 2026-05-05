// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcNamedPrimaryAssetId.h"
#include "ArcMassItemAttachmentSlots.generated.h"

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping, Categories = "SlotId"))
	FGameplayTag SlotId;

	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId DefaultVisualItem;

	UPROPERTY(EditDefaultsOnly, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassAttachmentHandler"))
	TArray<FInstancedStruct> Handlers;
};
