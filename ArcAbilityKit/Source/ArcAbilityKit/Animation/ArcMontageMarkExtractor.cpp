// Copyright Lukasz Baran. All Rights Reserved.

#include "Animation/ArcMontageMarkExtractor.h"
#include "AnimNotifies/ArcAnimNotify_MarkGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"

void ArcMontageMarks::ExtractMarks(const UAnimMontage* Montage, TArray<FArcMontageMarkEntry>& OutMarks)
{
    OutMarks.Reset();
    if (Montage == nullptr)
    {
        return;
    }

    const float Length = Montage->GetPlayLength();
    if (Length <= 0.f)
    {
        return;
    }

    FAnimNotifyContext NotifyContext;
    Montage->GetAnimNotifies(0.f, Length, NotifyContext);

    OutMarks.Reserve(NotifyContext.ActiveNotifies.Num());
    for (const FAnimNotifyEventReference& Ref : NotifyContext.ActiveNotifies)
    {
        const FAnimNotifyEvent* Event = Ref.GetNotify();
        if (Event == nullptr)
        {
            continue;
        }

        const UArcAnimNotify_MarkGameplayEvent* Marker = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
        if (Marker == nullptr)
        {
            continue;
        }

        FArcMontageMarkEntry& Entry = OutMarks.AddDefaulted_GetRef();
        Entry.Time = Event->GetTriggerTime();
        Entry.Tag = Marker->GetEventTag();
#if WITH_EDITORONLY_DATA
        Entry.SourceNotifyGuid = Event->Guid;
#endif
    }
}
