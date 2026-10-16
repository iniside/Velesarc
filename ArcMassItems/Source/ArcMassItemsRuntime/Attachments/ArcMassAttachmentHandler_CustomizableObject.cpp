// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler_CustomizableObject.h"

#include "Engine/SkeletalMesh.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Equipment/Fragments/ArcItemFragment_CustomizableObject.h"

bool FArcMassAttachmentHandler_CustomizableObject::HandleItemAddedToSlot(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    FArcMassItemAttachmentStateFragment& State,
    const FArcItemData* Item,
    const FArcItemData* OwnerItem) const
{
    if (!Item)
    {
        return false;
    }

    const FArcItemDefinitionData* ItemDef = Item->GetItemDefinition();
    const FArcItemFragment_CustomizableObject* Fragment = ItemDef
        ? ItemDef->FindFragment<FArcItemFragment_CustomizableObject>()
        : nullptr;

    if (!Fragment)
    {
        return false;
    }

    FArcMassItemAttachment Attachment = MakeItemAttachment(Item, OwnerItem, State);
    State.ActiveAttachments.Add(Attachment);
    State.TakenSockets.FindOrAdd(Attachment.SlotId).Names.Add(Attachment.SocketName);

    return true;
}

void FArcMassAttachmentHandler_CustomizableObject::HandleItemAttach(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    FArcMassItemAttachmentStateFragment& State,
    const FArcItemId ItemId,
    const FArcItemId OwnerItem) const
{
    const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
        [&ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
    if (Index == INDEX_NONE)
    {
        return;
    }

    FArcMassItemAttachment& ItemAttachment = State.ActiveAttachments[Index];

    if (State.ObjectsAttachedFromItem.Contains(ItemId))
    {
        return;
    }

    const FArcItemFragment_CustomizableObject* Fragment = FindAttachmentFragment<FArcItemFragment_CustomizableObject>(ItemAttachment);
    if (!Fragment)
    {
        return;
    }

    if (Fragment->CustomizableObject.IsNull()
        || (Fragment->IntParameter.ParameterName.IsEmpty() && Fragment->SkeletalMeshParameter.ParameterName.IsEmpty()))
    {
        return;
    }

    AActor* Owner = FindActor(EntityManager, Entity);
    if (!Owner)
    {
        return;
    }

    UCustomizableSkeletalComponent* CSC = Owner->FindComponentByClass<UCustomizableSkeletalComponent>();
    if (!CSC)
    {
        return;
    }

    UCustomizableObject* CO = Fragment->CustomizableObject.LoadSynchronous();
    if (!CO)
    {
        return;
    }

#if WITH_EDITOR
    if (!CO->IsCompiled())
    {
        FCompileParams CompileParams;
        CompileParams.bSkipIfCompiled = true;
        CompileParams.bAsync = false;
        CO->Compile(CompileParams);
    }
#endif

    if (!CSC->GetCustomizableObjectInstance())
    {
        CSC->SetCustomizableObjectInstance(CO->CreateInstance());
    }

    UCustomizableObjectInstance* Instance = CSC->GetCustomizableObjectInstance();
    if (!Instance)
    {
        return;
    }

    bool bAnyRan = false;

    if (!Fragment->IntParameter.ParameterName.IsEmpty())
    {
        Instance->SetEnumParameterSelectedOption(Fragment->IntParameter.ParameterName, Fragment->IntParameter.SelectedOption);
        bAnyRan = true;
    }

    if (!Fragment->SkeletalMeshParameter.ParameterName.IsEmpty())
    {
        USkeletalMesh* SkMesh = Fragment->SkeletalMeshParameter.SkeletalMesh.LoadSynchronous();
        Instance->SetSkeletalMeshParameterSelectedOption(
            Fragment->SkeletalMeshParameter.ParameterName,
            SkMesh,
            Fragment->SkeletalMeshParameter.RangeIndex);
        bAnyRan = true;
    }

    if (bAnyRan)
    {
        Instance->UpdateSkeletalMeshAsync(true, true);
    }

    State.ObjectsAttachedFromItem.FindOrAdd(ItemId).Objects.Add(CSC);
}

void FArcMassAttachmentHandler_CustomizableObject::HandleItemDetach(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    FArcMassItemAttachmentStateFragment& State,
    const FArcMassItemAttachment& Snapshot) const
{
    UCustomizableSkeletalComponent* CSC = nullptr;

    if (FArcMassItemAttachmentObjectArray* Objs = State.ObjectsAttachedFromItem.Find(Snapshot.ItemId))
    {
        for (const TWeakObjectPtr<UObject>& Obj : Objs->Objects)
        {
            UCustomizableSkeletalComponent* Candidate = Cast<UCustomizableSkeletalComponent>(Obj.Get());
            if (Candidate)
            {
                CSC = Candidate;
                break;
            }
        }
    }

    if (!CSC)
    {
        AActor* Owner = FindActor(EntityManager, Entity);
        if (Owner)
        {
            CSC = Owner->FindComponentByClass<UCustomizableSkeletalComponent>();
        }
    }

    if (!CSC)
    {
        State.ObjectsAttachedFromItem.Remove(Snapshot.ItemId);
        return;
    }

    const FArcItemFragment_CustomizableObject* Fragment = nullptr;
    if (const FArcItemDefinitionData* VisualDefinition = FArcItemDefinitionData::Find(Snapshot.VisualItemDefinition))
    {
        Fragment = VisualDefinition->FindFragment<FArcItemFragment_CustomizableObject>();
    }
    if (!Fragment)
    {
        if (const FArcItemDefinitionData* Definition = FArcItemDefinitionData::Find(Snapshot.ItemDefinition))
        {
            Fragment = Definition->FindFragment<FArcItemFragment_CustomizableObject>();
        }
    }

    if (Fragment && CSC->GetCustomizableObjectInstance())
    {
        bool bAnyCleared = false;

        if (!Fragment->IntParameter.ParameterName.IsEmpty())
        {
            CSC->GetCustomizableObjectInstance()->SetEnumParameterSelectedOption(Fragment->IntParameter.ParameterName, TEXT("None"));
            bAnyCleared = true;
        }

        if (!Fragment->SkeletalMeshParameter.ParameterName.IsEmpty())
        {
            CSC->GetCustomizableObjectInstance()->SetSkeletalMeshParameterSelectedOption(
                Fragment->SkeletalMeshParameter.ParameterName,
                nullptr,
                Fragment->SkeletalMeshParameter.RangeIndex);
            bAnyCleared = true;
        }

        if (bAnyCleared)
        {
            CSC->GetCustomizableObjectInstance()->UpdateSkeletalMeshAsync(true, true);
        }
    }

    State.ObjectsAttachedFromItem.Remove(Snapshot.ItemId);
}
