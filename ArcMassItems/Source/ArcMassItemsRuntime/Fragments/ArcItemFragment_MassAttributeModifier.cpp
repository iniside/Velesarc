#include "Fragments/ArcItemFragment_MassAttributeModifier.h"
#include "Items/ArcItemsHelpers.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"

void FArcItemFragment_MassAttributeModifier::OnItemAddedToSlot(const FArcItemData* InItem,
                                                                const FGameplayTag& InSlotId) const
{
    if (!InItem->MassEntityHandle.IsValid() || !InItem->World.IsValid())
        return;
    UMassEntitySubsystem* Subsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(InItem->World.Get());
    if (!Subsystem)
        return;
    FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
    const FMassEntityHandle Entity = InItem->MassEntityHandle;
    if (!EntityManager.IsEntityValid(Entity))
        return;

    FArcItemInstance_MassAttributeModifiers* Instance =
        ArcItemsHelper::FindMutableInstance<FArcItemInstance_MassAttributeModifiers>(InItem);

    for (const FArcDirectModifier& Modifier : Modifiers)
    {
        FArcModifierHandle Handle = ArcModifiers::ApplyInfinite(
            EntityManager, Entity, Entity, Modifier);

        if (Handle.IsValid())
        {
            Instance->AppliedModifiers.Add(Handle);
        }
    }
}

void FArcItemFragment_MassAttributeModifier::OnItemRemovedFromSlot(const FArcItemData* InItem,
                                                                     const FGameplayTag& InSlotId) const
{
    if (!InItem->MassEntityHandle.IsValid() || !InItem->World.IsValid())
        return;
    UMassEntitySubsystem* Subsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(InItem->World.Get());
    if (!Subsystem)
        return;
    FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
    const FMassEntityHandle Entity = InItem->MassEntityHandle;
    if (!EntityManager.IsEntityValid(Entity))
        return;

    FArcItemInstance_MassAttributeModifiers* Instance =
        ArcItemsHelper::FindMutableInstance<FArcItemInstance_MassAttributeModifiers>(InItem);

    for (const FArcModifierHandle& Handle : Instance->AppliedModifiers)
    {
        ArcModifiers::RemoveModifier(EntityManager, Entity, Handle);
    }
    Instance->AppliedModifiers.Empty();
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_MassAttributeModifier::GetDescription(const UScriptStruct* InStruct) const
{
    FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
    Desc.CommonPairings = {
        FName(TEXT("FArcItemFragment_GrantedMassAbilities")),
        FName(TEXT("FArcItemFragment_GrantedMassEffects"))
    };
    Desc.Prerequisites = { FName(TEXT("UArcCoreMassAgentComponent")) };
    Desc.UsageNotes = TEXT(
        "Directly modifies Mass entity attributes when equipped. "
        "Each FArcDirectModifier specifies an attribute, operation, and magnitude. "
        "Modifiers are applied as infinite-duration aggregator entries — no effect asset needed. "
        "Requires FArcAggregatorFragment on the Mass entity.");
    return Desc;
}
#endif
