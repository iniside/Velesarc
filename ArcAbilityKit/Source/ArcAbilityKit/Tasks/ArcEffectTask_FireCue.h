// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcEffectTask.h"
#include "ArcCues/ArcCueTypes.h"
#include "ArcEffectTask_FireCue.generated.h"

struct FArcActiveEffect;

/** Fires a gameplay cue tagged by CueTag when the effect is applied; if bPersistent, keeps the cue alive until the effect is removed. */
USTRUCT(BlueprintType, meta=(DisplayName="Fire Cue"))
struct ARCABILITYKIT_API FArcEffectTask_FireCue : public FArcEffectTask
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FGameplayTag CueTag;

    UPROPERTY(EditAnywhere)
    bool bPersistent = false;

    UPROPERTY(EditAnywhere)
    FGameplayTagContainer ExtraContextTags;

    /** Persistent path only — populated in OnApplication, consumed in OnRemoved. */
    FArcCueHandle ActiveHandle;

    virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
    virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
    virtual void OnExecute(FArcActiveEffect& OwningEffect) override;
};
