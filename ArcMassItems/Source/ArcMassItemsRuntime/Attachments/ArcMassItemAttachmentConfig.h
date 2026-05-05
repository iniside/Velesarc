// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "ArcMassItemAttachmentConfig.generated.h"

UCLASS(BlueprintType)
class ARCMASSITEMSRUNTIME_API UArcMassItemAttachmentConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Attachments", meta = (TitleProperty = "SlotId"))
    TArray<FArcMassItemAttachmentSlot> Slots;

    const FArcMassItemAttachmentSlot* FindSlot(FGameplayTag SlotId) const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    mutable TMap<FGameplayTag, const FArcMassItemAttachmentSlot*> SlotsBySlotIdCache;
    mutable bool bCacheBuilt = false;

    void BuildCache() const;
};
