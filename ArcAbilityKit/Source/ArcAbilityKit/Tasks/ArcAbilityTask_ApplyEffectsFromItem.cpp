// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_ApplyEffectsFromItem.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "ItemFragments/ArcItemFragment_MassAbilityEffects.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"

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

    const FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, SourceEntity, AbilityContext.GetAbilityHandle());

    const FArcItemData* ItemData = ArcAbilityKit::FindSourceItem(EntityManager, SourceEntity, AbilityContext.GetAbilityHandle());
    if (!ItemData)
    {
        return EStateTreeRunStatus::Failed;
    }

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
            const FMassEntityHandle TargetEntity = ArcMassPhysicsEntityLink::ResolveHit(Hit);
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

                const FArcEffectApplyResult ApplyResult = ArcEffects::TryApplyEffect(
					EntityManager, TargetEntity, EffectDef, EffectContext);

                bAnyApplied = bAnyApplied || ApplyResult.bApplied;
            }
        }
    }

    return bAnyApplied ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
