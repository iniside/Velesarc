// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/ArcMontageMarkEntry.h"

class UAnimMontage;

namespace ArcMontageMarks
{
    // Walks Montage notifies, collects UArcAnimNotify_MarkGameplayEvent entries.
    // OutMarks: (Time = Event->GetTriggerTime(), Tag = MGE->GetEventTag()). Times are unscaled
    // (PlayRate=1.0); callers apply their own play-rate scaling.
    ARCABILITYKIT_API void ExtractMarks(const UAnimMontage* Montage, TArray<FArcMontageMarkEntry>& OutMarks);
}
