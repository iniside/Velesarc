// Copyright Lukasz Baran. All Rights Reserved.

#include "ML/ArcFactionStrategyInteractor.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsActions.h"
#include "ML/ArcStrategyAgentProxy.h"

// ---- SpecifyAgentObservation -----------------------------------------------

void UArcFactionStrategyInteractor::SpecifyAgentObservation_Implementation(
    FLearningAgentsObservationSchemaElement& OutObservationSchemaElement,
    ULearningAgentsObservationSchema* InObservationSchema)
{
    // 4 named scalar floats for own state
    FLearningAgentsObservationSchemaElement DominanceElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Dominance"));
    FLearningAgentsObservationSchemaElement StabilityElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Stability"));
    FLearningAgentsObservationSchemaElement WealthElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Wealth"));
    FLearningAgentsObservationSchemaElement ExpansionElem =
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, FLearningAgentsFloatObservationNormalizationSettings(), true, TEXT("Expansion"));

    // Owned settlement slot: 4 floats (Security, Prosperity, Military, Labor)
    FLearningAgentsObservationSchemaElement SettlementSlotElem =
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, FieldsPerSettlement, FLearningAgentsObservationNormalizationSettings(), true, TEXT("SettlementSlot"));

    // Static array of MaxOwnedSettlements settlement slots
    FLearningAgentsObservationSchemaElement OwnedSettlementsElem =
        ULearningAgentsObservations::SpecifyStaticArrayObservation(InObservationSchema, SettlementSlotElem, MaxOwnedSettlements, true, TEXT("OwnedSettlements"));

    // Known faction slot: 5 floats (IsValid, DiplomaticStance, EstimatedDominance, EstimatedWealth, RelativeStrength)
    FLearningAgentsObservationSchemaElement FactionSlotElem =
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, FieldsPerKnownFaction, FLearningAgentsObservationNormalizationSettings(), true, TEXT("FactionSlot"));

    // Static array of MaxKnownFactions faction slots
    FLearningAgentsObservationSchemaElement KnownFactionsElem =
        ULearningAgentsObservations::SpecifyStaticArrayObservation(InObservationSchema, FactionSlotElem, MaxKnownFactions, true, TEXT("KnownFactions"));

    // Top-level struct
    TArray<FName> ElementNames;
    TArray<FLearningAgentsObservationSchemaElement> Elements;
    ElementNames.Add(TEXT("Dominance"));        Elements.Add(DominanceElem);
    ElementNames.Add(TEXT("Stability"));        Elements.Add(StabilityElem);
    ElementNames.Add(TEXT("Wealth"));           Elements.Add(WealthElem);
    ElementNames.Add(TEXT("Expansion"));        Elements.Add(ExpansionElem);
    ElementNames.Add(TEXT("OwnedSettlements")); Elements.Add(OwnedSettlementsElem);
    ElementNames.Add(TEXT("KnownFactions"));    Elements.Add(KnownFactionsElem);

    OutObservationSchemaElement = ULearningAgentsObservations::SpecifyStructObservationFromArrays(
        InObservationSchema, ElementNames, Elements, true, TEXT("FactionObs"));
}

// ---- GatherAgentObservation ------------------------------------------------

void UArcFactionStrategyInteractor::GatherAgentObservation_Implementation(
    FLearningAgentsObservationObjectElement& OutObservationObjectElement,
    ULearningAgentsObservationObject* InObservationObject,
    const int32 AgentId)
{
    const UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));

    float Dominance  = 0.5f;
    float Stability  = 0.5f;
    float Wealth     = 0.5f;
    float Expansion  = 0.5f;

    if (Proxy)
    {
        const FArcFactionStrategicState& State = Proxy->FactionState;
        // Normalize 0–100 to 0–1
        Dominance = State.Dominance  * 0.01f;
        Stability = State.Stability  * 0.01f;
        Wealth    = State.Wealth     * 0.01f;
        Expansion = State.Expansion  * 0.01f;
    }

    FLearningAgentsObservationObjectElement DominanceObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Dominance, true, TEXT("Dominance"));
    FLearningAgentsObservationObjectElement StabilityObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Stability, true, TEXT("Stability"));
    FLearningAgentsObservationObjectElement WealthObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Wealth, true, TEXT("Wealth"));
    FLearningAgentsObservationObjectElement ExpansionObs =
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Expansion, true, TEXT("Expansion"));

    // Owned settlements — zero-padded for now
    TArray<float> ZeroSettlementData;
    ZeroSettlementData.SetNumZeroed(FieldsPerSettlement);
    TArray<FLearningAgentsObservationObjectElement> SettlementSlots;
    SettlementSlots.Reserve(MaxOwnedSettlements);
    for (int32 i = 0; i < MaxOwnedSettlements; ++i)
    {
        SettlementSlots.Add(ULearningAgentsObservations::MakeContinuousObservation(
            InObservationObject, ZeroSettlementData, true, TEXT("SettlementSlot")));
    }
    FLearningAgentsObservationObjectElement OwnedSettlementsObs =
        ULearningAgentsObservations::MakeStaticArrayObservation(InObservationObject, SettlementSlots, true, TEXT("OwnedSettlements"));

    // Known factions — zero-padded for now
    TArray<float> ZeroFactionData;
    ZeroFactionData.SetNumZeroed(FieldsPerKnownFaction);
    TArray<FLearningAgentsObservationObjectElement> FactionSlots;
    FactionSlots.Reserve(MaxKnownFactions);
    for (int32 i = 0; i < MaxKnownFactions; ++i)
    {
        FactionSlots.Add(ULearningAgentsObservations::MakeContinuousObservation(
            InObservationObject, ZeroFactionData, true, TEXT("FactionSlot")));
    }
    FLearningAgentsObservationObjectElement KnownFactionsObs =
        ULearningAgentsObservations::MakeStaticArrayObservation(InObservationObject, FactionSlots, true, TEXT("KnownFactions"));

    TArray<FName> Names;
    TArray<FLearningAgentsObservationObjectElement> Elems;
    Names.Add(TEXT("Dominance"));        Elems.Add(DominanceObs);
    Names.Add(TEXT("Stability"));        Elems.Add(StabilityObs);
    Names.Add(TEXT("Wealth"));           Elems.Add(WealthObs);
    Names.Add(TEXT("Expansion"));        Elems.Add(ExpansionObs);
    Names.Add(TEXT("OwnedSettlements")); Elems.Add(OwnedSettlementsObs);
    Names.Add(TEXT("KnownFactions"));    Elems.Add(KnownFactionsObs);

    OutObservationObjectElement = ULearningAgentsObservations::MakeStructObservationFromArrays(
        InObservationObject, Names, Elems, true, TEXT("FactionObs"));
}

// ---- SpecifyAgentAction ----------------------------------------------------

void UArcFactionStrategyInteractor::SpecifyAgentAction_Implementation(
    FLearningAgentsActionSchemaElement& OutActionSchemaElement,
    ULearningAgentsActionSchema* InActionSchema)
{
    // 7 continuous floats — one score per EArcFactionAction
    FLearningAgentsActionSchemaElement ActionScoresElem =
        ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, NumFactionActions, FLearningAgentsActionNormalizationSettings(), true, TEXT("ActionScores"));

    // 6 continuous floats — one score per known-faction slot
    FLearningAgentsActionSchemaElement TargetFactionScoresElem =
        ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, MaxKnownFactions, FLearningAgentsActionNormalizationSettings(), true, TEXT("TargetFactionScores"));

    // 8 continuous floats — one score per owned-settlement slot (spatial targets)
    FLearningAgentsActionSchemaElement TargetLocationScoresElem =
        ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, MaxOwnedSettlements, FLearningAgentsActionNormalizationSettings(), true, TEXT("TargetLocationScores"));

    // 1 float — intensity
    FLearningAgentsActionSchemaElement IntensityElem =
        ULearningAgentsActions::SpecifyFloatAction(InActionSchema, FLearningAgentsFloatActionNormalizationSettings(), true, TEXT("Intensity"));

    TArray<FName> Names;
    TArray<FLearningAgentsActionSchemaElement> Elems;
    Names.Add(TEXT("ActionScores"));        Elems.Add(ActionScoresElem);
    Names.Add(TEXT("TargetFactionScores")); Elems.Add(TargetFactionScoresElem);
    Names.Add(TEXT("TargetLocationScores")); Elems.Add(TargetLocationScoresElem);
    Names.Add(TEXT("Intensity"));           Elems.Add(IntensityElem);

    OutActionSchemaElement = ULearningAgentsActions::SpecifyStructActionFromArrays(
        InActionSchema, Names, Elems, true, TEXT("FactionAction"));
}

// ---- PerformAgentAction ----------------------------------------------------

void UArcFactionStrategyInteractor::PerformAgentAction_Implementation(
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
    FLearningAgentsActionObjectElement TargetFactionScoresElem;
    FLearningAgentsActionObjectElement TargetLocationScoresElem;
    FLearningAgentsActionObjectElement IntensityElem;

    ULearningAgentsActions::GetStructActionElement(ActionScoresElem,        InActionObject, InActionObjectElement, TEXT("ActionScores"),        true, TEXT("FactionAction"));
    ULearningAgentsActions::GetStructActionElement(TargetFactionScoresElem, InActionObject, InActionObjectElement, TEXT("TargetFactionScores"), true, TEXT("FactionAction"));
    ULearningAgentsActions::GetStructActionElement(TargetLocationScoresElem,InActionObject, InActionObjectElement, TEXT("TargetLocationScores"),true, TEXT("FactionAction"));
    ULearningAgentsActions::GetStructActionElement(IntensityElem,           InActionObject, InActionObjectElement, TEXT("Intensity"),           true, TEXT("FactionAction"));

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

    // Decode target faction scores — argmax
    TArray<float> TargetFactionScores;
    ULearningAgentsActions::GetContinuousAction(TargetFactionScores, InActionObject, TargetFactionScoresElem, true, TEXT("TargetFactionScores"));

    int32 BestFactionTargetIndex = 0;
    float BestFactionTargetScore = -MAX_FLT;
    for (int32 i = 0; i < TargetFactionScores.Num(); ++i)
    {
        if (TargetFactionScores[i] > BestFactionTargetScore)
        {
            BestFactionTargetScore = TargetFactionScores[i];
            BestFactionTargetIndex = i;
        }
    }

    // Decode target location scores — argmax
    TArray<float> TargetLocationScores;
    ULearningAgentsActions::GetContinuousAction(TargetLocationScores, InActionObject, TargetLocationScoresElem, true, TEXT("TargetLocationScores"));

    int32 BestLocationTargetIndex = 0;
    float BestLocationTargetScore = -MAX_FLT;
    for (int32 i = 0; i < TargetLocationScores.Num(); ++i)
    {
        if (TargetLocationScores[i] > BestLocationTargetScore)
        {
            BestLocationTargetScore = TargetLocationScores[i];
            BestLocationTargetIndex = i;
        }
    }

    // Decode intensity
    float IntensityValue = 0.5f;
    ULearningAgentsActions::GetFloatAction(IntensityValue, InActionObject, IntensityElem, true, TEXT("Intensity"));
    IntensityValue = FMath::Clamp(IntensityValue, 0.0f, 1.0f);

    // Write decision — TargetEntity/TargetLocation resolution from best indices deferred to subsystem
    FArcStrategyDecision& Decision = Proxy->LastDecision;
    Decision.ActionIndex    = BestActionIndex;
    Decision.Intensity      = IntensityValue;
    Decision.TargetEntity   = FMassEntityHandle();
    Decision.TargetLocation = FVector::ZeroVector;

    Proxy->TargetFactionIndex = BestFactionTargetIndex;
    Proxy->TargetLocationIndex = BestLocationTargetIndex;
}
