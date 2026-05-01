// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Mass/EntityHandle.h"
#include "Strategy/ArcStrategicState.h"
#include "Strategy/ArcStrategyTypes.h"
#include "ArcStrategyAgentProxy.generated.h"

/**
 * Lightweight UObject bridging a Mass entity to the Learning Agents system.
 * Learning Agents requires UObject* agents — this proxy holds the entity handle
 * and provides access to strategic state and decision execution.
 */
UCLASS(Transient)
class ARCECONOMY_API UArcStrategyAgentProxy : public UObject
{
    GENERATED_BODY()

public:
    /** The Mass entity this proxy represents (settlement or faction). */
    FMassEntityHandle EntityHandle;

    /** Most recently computed strategic state (settlement). */
    FArcSettlementStrategicState SettlementState;

    /** Most recently computed strategic state (faction). */
    FArcFactionStrategicState FactionState;

    /** The last decision made by the ML policy, to be executed by the subsystem. */
    FArcStrategyDecision LastDecision;

    /** Agent ID assigned by ULearningAgentsManager. */
    int32 AgentId = INDEX_NONE;

    /** Previous strategic state for delta-based reward computation. */
    FArcSettlementStrategicState PreviousSettlementState;
    FArcFactionStrategicState PreviousFactionState;

    /** Previous settlement count for faction territory delta tracking. */
    int32 PreviousSettlementCount = 0;

    /** Defense events this step — reset after reward read. */
    int32 DefenseEventCount = 0;

    /** Target indices from last action decode. Resolved by simulation against spawned entity arrays. */
    int32 TargetFactionIndex = INDEX_NONE;
    int32 TargetLocationIndex = INDEX_NONE;
    int32 TargetNeighborIndex = INDEX_NONE;
};
