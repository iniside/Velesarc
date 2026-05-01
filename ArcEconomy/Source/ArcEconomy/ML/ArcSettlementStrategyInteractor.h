// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsInteractor.h"

#include "ArcSettlementStrategyInteractor.generated.h"

/**
 * Learning Agents interactor for settlement-level strategy agents.
 *
 * Observation (~69 floats):
 *   - 5 named floats: Security, Prosperity, Growth, Labor, Military
 *   - 16 continuous floats: ResourceScores (padded)
 *   - 8 neighbor structs x 6 floats: IsValid, DiplomaticStance, EstimatedMilitary,
 *     EstimatedProsperity, Distance, IsSameFaction
 *
 * Action (16 floats):
 *   - ActionScores: continuous 7 (one per EArcSettlementAction)
 *   - TargetScores: continuous 8 (one per neighbor slot)
 *   - Intensity: 1 float
 */
UCLASS(Blueprintable)
class ARCECONOMY_API UArcSettlementStrategyInteractor : public ULearningAgentsInteractor
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxNeighbors = 8;
    static constexpr int32 MaxResourceSlots = 16;
    static constexpr int32 NumSettlementActions = 7;
    static constexpr int32 FieldsPerNeighbor = 6;

    /** Bitmask of enabled actions. Bit N set = action N allowed. Set by training subsystem from curriculum. */
    const uint32* ActionMask = nullptr;

    //~ Begin ULearningAgentsInteractor Interface
    virtual void SpecifyAgentObservation_Implementation(
        FLearningAgentsObservationSchemaElement& OutObservationSchemaElement,
        ULearningAgentsObservationSchema* InObservationSchema) override;

    virtual void GatherAgentObservation_Implementation(
        FLearningAgentsObservationObjectElement& OutObservationObjectElement,
        ULearningAgentsObservationObject* InObservationObject,
        const int32 AgentId) override;

    virtual void SpecifyAgentAction_Implementation(
        FLearningAgentsActionSchemaElement& OutActionSchemaElement,
        ULearningAgentsActionSchema* InActionSchema) override;

    virtual void PerformAgentAction_Implementation(
        const ULearningAgentsActionObject* InActionObject,
        const FLearningAgentsActionObjectElement& InActionObjectElement,
        const int32 AgentId) override;
    //~ End ULearningAgentsInteractor Interface
};
