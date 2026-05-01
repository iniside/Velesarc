// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsInteractor.h"

#include "ArcFactionStrategyInteractor.generated.h"

/**
 * Learning Agents interactor for faction-level strategy agents.
 *
 * Observation (~66 floats):
 *   - 4 named floats: Dominance, Stability, Wealth, Expansion
 *   - 8 owned-settlement structs x 4 floats: Security, Prosperity, Military, Labor
 *   - 6 known-faction structs x 5 floats: IsValid, DiplomaticStance, EstimatedDominance,
 *     EstimatedWealth, RelativeStrength
 *
 * Action (22 floats):
 *   - ActionScores: continuous 7 (one per EArcFactionAction)
 *   - TargetFactionScores: continuous 6
 *   - TargetLocationScores: continuous 8
 *   - Intensity: 1 float
 */
UCLASS(Blueprintable)
class ARCECONOMY_API UArcFactionStrategyInteractor : public ULearningAgentsInteractor
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxOwnedSettlements = 8;
    static constexpr int32 MaxKnownFactions = 6;
    static constexpr int32 NumFactionActions = 7;
    static constexpr int32 FieldsPerSettlement = 4;
    static constexpr int32 FieldsPerKnownFaction = 5;

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
