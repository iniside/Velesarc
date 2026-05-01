// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionFragments.h"
#include "ArcConditionSaturationAttributes.h"
#include "ArcConditionSaturationHandler.h"
#include "ArcConditionTypes.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "MassEntityManager.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

FMassEntityHandle ArcConditionTestHelpers::CreateConditionEntity(FMassEntityManager& EntityManager)
{
    TArray<FInstancedStruct> Instances;
    Instances.Add(FInstancedStruct::Make(FArcConditionStatesFragment()));

    FMassEntityHandle Entity = EntityManager.CreateEntity(Instances);

    const FConstSharedStruct& ConfigFrag = EntityManager.GetOrCreateConstSharedFragment<FArcConditionConfigsShared>();
    EntityManager.AddConstSharedFragmentToEntity(Entity, ConfigFrag);

    return Entity;
}

FMassEntityHandle ArcConditionTestHelpers::CreateConditionEntityWithEffects(FMassEntityManager& EntityManager)
{
    FMassEntityHandle Entity = CreateConditionEntity(EntityManager);

    TArray<FInstancedStruct> ExtraFragments;
    ExtraFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
    ExtraFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
    ExtraFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
    ExtraFragments.Add(FInstancedStruct::Make(FArcConditionSaturationAttributes()));

    EntityManager.AddFragmentInstanceListToEntity(Entity, ExtraFragments);

    return Entity;
}

FMassEntityHandle ArcConditionTestHelpers::CreateFullConditionEntity(FMassEntityManager& EntityManager)
{
    FMassEntityHandle Entity = CreateConditionEntityWithEffects(EntityManager);

    TArray<FInstancedStruct> ExtraFragments;
    ExtraFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));
    EntityManager.AddFragmentInstanceListToEntity(Entity, ExtraFragments);

    UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();

    struct FPair { FArcAttributeRef Attr; EArcConditionType Type; };
    const TArray<FPair> Pairs = {
        { FArcConditionSaturationAttributes::GetBurningSaturationAttribute(),     EArcConditionType::Burning },
        { FArcConditionSaturationAttributes::GetBleedingSaturationAttribute(),    EArcConditionType::Bleeding },
        { FArcConditionSaturationAttributes::GetChilledSaturationAttribute(),     EArcConditionType::Chilled },
        { FArcConditionSaturationAttributes::GetShockedSaturationAttribute(),     EArcConditionType::Shocked },
        { FArcConditionSaturationAttributes::GetPoisonedSaturationAttribute(),    EArcConditionType::Poisoned },
        { FArcConditionSaturationAttributes::GetDiseasedSaturationAttribute(),    EArcConditionType::Diseased },
        { FArcConditionSaturationAttributes::GetWeakenedSaturationAttribute(),    EArcConditionType::Weakened },
        { FArcConditionSaturationAttributes::GetOiledSaturationAttribute(),       EArcConditionType::Oiled },
        { FArcConditionSaturationAttributes::GetWetSaturationAttribute(),         EArcConditionType::Wet },
        { FArcConditionSaturationAttributes::GetCorrodedSaturationAttribute(),    EArcConditionType::Corroded },
        { FArcConditionSaturationAttributes::GetBlindedSaturationAttribute(),     EArcConditionType::Blinded },
        { FArcConditionSaturationAttributes::GetSuffocatingSaturationAttribute(), EArcConditionType::Suffocating },
        { FArcConditionSaturationAttributes::GetExhaustedSaturationAttribute(),   EArcConditionType::Exhausted },
    };

    for (const FPair& P : Pairs)
    {
        FArcConditionSaturationHandler Handler;
        Handler.ConditionType = P.Type;
        HandlerConfig->Handlers.Add(P.Attr, FInstancedStruct::Make(Handler));
    }

    FArcAttributeHandlerSharedFragment SharedHandler;
    SharedHandler.Config = HandlerConfig;
    const FConstSharedStruct HandlerSharedFrag = EntityManager.GetOrCreateConstSharedFragment(SharedHandler);
    EntityManager.AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

    return Entity;
}
