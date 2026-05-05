// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassItemAttachmentConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassItemAttachmentConfig, Log, All);

const FArcMassItemAttachmentSlot* UArcMassItemAttachmentConfig::FindSlot(FGameplayTag SlotId) const
{
    if (!bCacheBuilt) BuildCache();
    if (const FArcMassItemAttachmentSlot* const* Found = SlotsBySlotIdCache.Find(SlotId))
    {
        return *Found;
    }
    return nullptr;
}

void UArcMassItemAttachmentConfig::BuildCache() const
{
    SlotsBySlotIdCache.Reset();
    for (const FArcMassItemAttachmentSlot& Slot : Slots)
    {
        if (SlotsBySlotIdCache.Contains(Slot.SlotId))
        {
            UE_LOG(LogArcMassItemAttachmentConfig, Warning,
                TEXT("UArcMassItemAttachmentConfig '%s' has duplicate SlotId '%s' - first wins"),
                *GetName(), *Slot.SlotId.ToString());
            continue;
        }
        SlotsBySlotIdCache.Add(Slot.SlotId, &Slot);
    }
    bCacheBuilt = true;
}

#if WITH_EDITOR
void UArcMassItemAttachmentConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    bCacheBuilt = false;
    SlotsBySlotIdCache.Reset();
}
#endif
