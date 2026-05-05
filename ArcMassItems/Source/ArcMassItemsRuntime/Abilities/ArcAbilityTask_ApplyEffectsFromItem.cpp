// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityTask_ApplyEffectsFromItem.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassAbilities/ArcAbilitySourceData_Item.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Fragments/ArcItemFragment_MassAbilityEffects.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassAgentComponent.h"

namespace ArcApplyEffectsFromItem
{
    bool PassesTagRequirements(const FArcTagCountContainer& Tags,
                               const FGameplayTagContainer& RequiredTags,
                               const FGameplayTagContainer& IgnoreTags)
    {
        if (!RequiredTags.IsEmpty() && !Tags.HasAll(RequiredTags))
        {
            return false;
        }
        if (!IgnoreTags.IsEmpty() && Tags.HasAny(IgnoreTags))
        {
            return false;
        }
        return true;
    }
}

EStateTreeRunStatus FArcAbilityTask_ApplyEffectsFromItem::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_ApplyEffectsFromItemInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ApplyEffectsFromItemInstanceData>(*this);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle SourceEntity = AbilityContext.GetEntity();

    FInstancedStruct SourceData;
    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, SourceEntity, AbilityContext.GetAbilityHandle());
    if (ActiveAbility)
    {
        SourceData = ActiveAbility->SourceData;
    }

    const FArcAbilitySourceData_Item* ItemSourceData = SourceData.GetPtr<FArcAbilitySourceData_Item>();
    if (!ItemSourceData || !ItemSourceData->ItemDataHandle.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    const FArcItemData* ItemData = ItemSourceData->ItemDataHandle.Get();
    const FArcItemInstance_MassAbilityEffects* EffectInstance = ArcItemsHelper::FindInstance<FArcItemInstance_MassAbilityEffects>(ItemData);
    if (!EffectInstance)
    {
        return EStateTreeRunStatus::Failed;
    }

    TArray<const FArcMassEffectSpecItem*> MatchingSpecs = EffectInstance->GetSpecsByTag(InstanceData.EffectTag);
    if (MatchingSpecs.IsEmpty())
    {
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.TargetDataHandle.HasHitResults())
    {
        return EStateTreeRunStatus::Failed;
    }

    // Get source entity owned tags for source tag requirements
    const FMassEntityView SourceView(EntityManager, SourceEntity);
    const FArcOwnedTagsFragment* SourceTagsFragment = SourceView.GetFragmentDataPtr<FArcOwnedTagsFragment>();

    const TArray<FHitResult>& HitResults = InstanceData.TargetDataHandle.GetHitResults();
    bool bAnyApplied = false;

    for (const FArcMassEffectSpecItem* Spec : MatchingSpecs)
    {
        // Check source tag requirements
        if (SourceTagsFragment)
        {
            if (!ArcApplyEffectsFromItem::PassesTagRequirements(
                SourceTagsFragment->GetTags(), Spec->SourceRequiredTags, Spec->SourceIgnoreTags))
            {
                continue;
            }
        }

        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor)
            {
                continue;
            }

            UMassAgentComponent* AgentComp = HitActor->FindComponentByClass<UMassAgentComponent>();
            if (!AgentComp)
            {
                continue;
            }

            FMassEntityHandle TargetEntity = AgentComp->GetEntityHandle();
            if (!EntityManager.IsEntityValid(TargetEntity))
            {
                continue;
            }

            // Check target tag requirements
            const FMassEntityView TargetView(EntityManager, TargetEntity);
            const FArcOwnedTagsFragment* TargetTagsFragment = TargetView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
            if (TargetTagsFragment)
            {
                if (!ArcApplyEffectsFromItem::PassesTagRequirements(
                    TargetTagsFragment->GetTags(), Spec->TargetRequiredTags, Spec->TargetIgnoreTags))
                {
                    continue;
                }
            }

            for (UArcEffectDefinition* EffectDef : Spec->Effects)
            {
                if (!EffectDef)
                {
                    continue;
                }

                FArcActiveEffectHandle ApplyHandle = ArcEffects::TryApplyEffect(
                    EntityManager, TargetEntity, EffectDef, SourceEntity, SourceData);

                bAnyApplied = bAnyApplied || ApplyHandle.IsValid();
            }
        }
    }

    return bAnyApplied ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
