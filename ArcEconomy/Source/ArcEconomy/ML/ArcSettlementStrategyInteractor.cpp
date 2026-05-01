// Copyright Lukasz Baran. All Rights Reserved.

#include "ML/ArcSettlementStrategyInteractor.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsActions.h"
#include "ML/ArcStrategyAgentProxy.h"
#include "GameplayTagContainer.h"

// ---- SpecifyAgentObservation -----------------------------------------------

void UArcSettlementStrategyInteractor::SpecifyAgentObservation_Implementation(
    FLearningAgentsObservationSchemaElement& OutObservationSchemaElement,
    ULearningAgentsObservationSchema* InObservationSchema)
{
    // 5 named scalar floats (Security, Prosperity, Growth, Labor, Military)
    FLearningAgentsObservationSchemaElement SecurityElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Security"));
    FLearningAgentsObservationSchemaElement ProsperityElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Prosperity"));
    FLearningAgentsObservationSchemaElement GrowthElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Growth"));
    FLearningAgentsObservationSchemaElement LaborElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Labor"));
    FLearningAgentsObservationSchemaElement MilitaryElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Military"));

    // 16-float continuous array for resource scores
    FLearningAgentsObservationSchemaElement ResourceElem =
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, MaxResourceSlots, FLearningAgentsObservationNormalizationSettings(), true, TEXT("ResourceScores"));

    // Neighbor slot: 6 continuous floats (IsValid, DiplomaticStance, EstimatedMilitary, EstimatedProsperity, Distance, IsSameFaction)
    FLearningAgentsObservationSchemaElement NeighborSlotElem =
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, FieldsPerNeighbor, FLearningAgentsObservationNormalizationSettings(), true, TEXT("NeighborSlot"));

    // Static array of MaxNeighbors neighbor slots
    FLearningAgentsObservationSchemaElement NeighborsElem =
        ULearningAgentsObservations::SpecifyStaticArrayObservation(InObservationSchema, NeighborSlotElem, MaxNeighbors, true, TEXT("Neighbors"));

    // Top-level struct combining all elements
    TArray<FName> ElementNames;
    TArray<FLearningAgentsObservationSchemaElement> Elements;
    ElementNames.Add(TEXT("Security"));       Elements.Add(SecurityElem);
    ElementNames.Add(TEXT("Prosperity"));     Elements.Add(ProsperityElem);
    ElementNames.Add(TEXT("Growth"));         Elements.Add(GrowthElem);
    ElementNames.Add(TEXT("Labor"));          Elements.Add(LaborElem);
    ElementNames.Add(TEXT("Military"));       Elements.Add(MilitaryElem);
    ElementNames.Add(TEXT("ResourceScores")); Elements.Add(ResourceElem);
    ElementNames.Add(TEXT("Neighbors"));      Elements.Add(NeighborsElem);

    OutObservationSchemaElement = ULearningAgentsObservations::SpecifyStructObservationFromArrays(
        InObservationSchema, ElementNames, Elements, true, TEXT("SettlementObs"));
}

// ---- GatherAgentObservation ------------------------------------------------

void UArcSettlementStrategyInteractor::GatherAgentObservation_Implementation(
    FLearningAgentsObservationObjectElement& OutObservationObjectElement,
    ULearningAgentsObservationObject* InObservationObject,
    const int32 AgentId)
{
    const UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));

    // Build zero-padded neighbor array (used whether proxy is valid or not for now)
    TArray<float> ZeroNeighborData;
    ZeroNeighborData.SetNumZeroed(FieldsPerNeighbor);
    TArray<FLearningAgentsObservationObjectElement> NeighborSlots;
    NeighborSlots.Reserve(MaxNeighbors);
    for (int32 i = 0; i < MaxNeighbors; ++i)
    {
        NeighborSlots.Add(ULearningAgentsObservations::MakeContinuousObservation(
            InObservationObject, ZeroNeighborData, true, TEXT("NeighborSlot")));
    }
    FLearningAgentsObservationObjectElement NeighborsObs =
        ULearningAgentsObservations::MakeStaticArrayObservation(InObservationObject, NeighborSlots, true, TEXT("Neighbors"));

    float Security   = 0.5f;
    float Prosperity = 0.5f;
    float Growth     = 0.5f;
    float Labor      = 0.5f;
    float Military   = 0.5f;
    TArray<float> ResourceValues;
    ResourceValues.SetNumZeroed(MaxResourceSlots);

    if (Proxy)
    {
        const FArcSettlementStrategicState& State = Proxy->SettlementState;
        // Normalize 0–100 to 0–1
        Security   = State.Security   * 0.01f;
        Prosperity = State.Prosperity * 0.01f;
        Growth     = State.Growth     * 0.01f;
        Labor      = State.Labor      * 0.01f;
        Military   = State.Military   * 0.01f;

        int32 SlotIndex = 0;
        for (const TPair<FGameplayTag, float>& Pair : State.ResourceScores)
        {
            if (SlotIndex >= MaxResourceSlots)
            {
                break;
            }
            ResourceValues[SlotIndex] = Pair.Value * 0.01f;
            ++SlotIndex;
        }
    }

    FLearningAgentsObservationObjectElement SecurityObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Security, true, TEXT("Security"));
    FLearningAgentsObservationObjectElement ProsperityObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Prosperity, true, TEXT("Prosperity"));
    FLearningAgentsObservationObjectElement GrowthObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Growth, true, TEXT("Growth"));
    FLearningAgentsObservationObjectElement LaborObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Labor, true, TEXT("Labor"));
    FLearningAgentsObservationObjectElement MilitaryObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Military, true, TEXT("Military"));
    FLearningAgentsObservationObjectElement ResourceObs =
        ULearningAgentsObservations::MakeContinuousObservation(InObservationObject, ResourceValues, true, TEXT("ResourceScores"));

    TArray<FName> Names;
    TArray<FLearningAgentsObservationObjectElement> Elems;
    Names.Add(TEXT("Security"));       Elems.Add(SecurityObs);
    Names.Add(TEXT("Prosperity"));     Elems.Add(ProsperityObs);
    Names.Add(TEXT("Growth"));         Elems.Add(GrowthObs);
    Names.Add(TEXT("Labor"));          Elems.Add(LaborObs);
    Names.Add(TEXT("Military"));       Elems.Add(MilitaryObs);
    Names.Add(TEXT("ResourceScores")); Elems.Add(ResourceObs);
    Names.Add(TEXT("Neighbors"));      Elems.Add(NeighborsObs);

    OutObservationObjectElement = ULearningAgentsObservations::MakeStructObservationFromArrays(
        InObservationObject, Names, Elems, true, TEXT("SettlementObs"));
}

// ---- SpecifyAgentAction ----------------------------------------------------

void UArcSettlementStrategyInteractor::SpecifyAgentAction_Implementation(
    FLearningAgentsActionSchemaElement& OutActionSchemaElement,
    ULearningAgentsActionSchema* InActionSchema)
{
    // 7 continuous floats — one score per EArcSettlementAction
    FLearningAgentsActionSchemaElement ActionScoresElem =
        ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, NumSettlementActions, FLearningAgentsActionNormalizationSettings(), true, TEXT("ActionScores"));

    // 8 continuous floats — one score per neighbor slot
    FLearningAgentsActionSchemaElement TargetScoresElem =
        ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, MaxNeighbors, FLearningAgentsActionNormalizationSettings(), true, TEXT("TargetScores"));

    // 1 float — intensity
    FLearningAgentsActionSchemaElement IntensityElem =
        ULearningAgentsActions::SpecifyFloatAction(InActionSchema, FLearningAgentsFloatActionNormalizationSettings(), true, TEXT("Intensity"));

    TArray<FName> Names;
    TArray<FLearningAgentsActionSchemaElement> Elems;
    Names.Add(TEXT("ActionScores")); Elems.Add(ActionScoresElem);
    Names.Add(TEXT("TargetScores")); Elems.Add(TargetScoresElem);
    Names.Add(TEXT("Intensity"));    Elems.Add(IntensityElem);

    OutActionSchemaElement = ULearningAgentsActions::SpecifyStructActionFromArrays(
        InActionSchema, Names, Elems, true, TEXT("SettlementAction"));
}

// ---- PerformAgentAction ----------------------------------------------------

void UArcSettlementStrategyInteractor::PerformAgentAction_Implementation(
    const ULearningAgentsActionObject* InActionObject,
    const FLearningAgentsActionObjectElement& InActionObjectElement,
    const int32 AgentId)
{
    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (!Proxy)
    {
        return;
    }

    // Unpack top-level struct by element name
    FLearningAgentsActionObjectElement ActionScoresElem;
    FLearningAgentsActionObjectElement TargetScoresElem;
    FLearningAgentsActionObjectElement IntensityElem;

    ULearningAgentsActions::GetStructActionElement(ActionScoresElem, InActionObject, InActionObjectElement, TEXT("ActionScores"), true, TEXT("SettlementAction"));
    ULearningAgentsActions::GetStructActionElement(TargetScoresElem, InActionObject, InActionObjectElement, TEXT("TargetScores"), true, TEXT("SettlementAction"));
    ULearningAgentsActions::GetStructActionElement(IntensityElem,    InActionObject, InActionObjectElement, TEXT("Intensity"),    true, TEXT("SettlementAction"));

    // Decode action scores — argmax with masking
    TArray<float> ActionScores;
    ULearningAgentsActions::GetContinuousAction(ActionScores, InActionObject, ActionScoresElem, true, TEXT("ActionScores"));

    // Apply mask: disabled actions get -MAX_FLT
    if (ActionMask)
    {
        for (int32 i = 0; i < ActionScores.Num(); ++i)
        {
            if (!((*ActionMask) & (1u << i)))
            {
                ActionScores[i] = -MAX_FLT;
            }
        }
    }

    int32 BestActionIndex = 0;
    float BestActionScore = -MAX_FLT;
    for (int32 i = 0; i < ActionScores.Num(); ++i)
    {
        if (ActionScores[i] > BestActionScore)
        {
            BestActionScore = ActionScores[i];
            BestActionIndex = i;
        }
    }

    // Decode target scores — argmax
    TArray<float> TargetScores;
    ULearningAgentsActions::GetContinuousAction(TargetScores, InActionObject, TargetScoresElem, true, TEXT("TargetScores"));

    int32 BestTargetIndex = 0;
    float BestTargetScore = -MAX_FLT;
    for (int32 i = 0; i < TargetScores.Num(); ++i)
    {
        if (TargetScores[i] > BestTargetScore)
        {
            BestTargetScore = TargetScores[i];
            BestTargetIndex = i;
        }
    }

    // Decode intensity
    float IntensityValue = 0.5f;
    ULearningAgentsActions::GetFloatAction(IntensityValue, InActionObject, IntensityElem, true, TEXT("Intensity"));
    IntensityValue = FMath::Clamp(IntensityValue, 0.0f, 1.0f);

    // Write decision — TargetEntity resolution from BestTargetIndex deferred to subsystem
    FArcStrategyDecision& Decision = Proxy->LastDecision;
    Decision.ActionIndex    = BestActionIndex;
    Decision.Intensity      = IntensityValue;
    Decision.TargetEntity   = FMassEntityHandle();
    Decision.TargetLocation = FVector::ZeroVector;

    Proxy->TargetNeighborIndex = BestTargetIndex;
}
